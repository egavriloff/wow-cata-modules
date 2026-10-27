"""Offline audit of shipped data; does not claim client terrain/gameplay validation."""
import csv
import io
import math
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
FIELDS = ("menu_id", "next_menu_id", "icon", "name", "cost", "level", "faction", "map", "x", "y", "z", "o")


def read_cpp(path):
    source = path.read_text(encoding="utf-8")
    rows = []
    for match in re.finditer(r"\{(\d+,.*?)\},", source, re.S):
        values = next(csv.reader(io.StringIO(match[1].replace("\n", " ")), skipinitialspace=True))
        if len(values) != len(FIELDS):
            raise ValueError(f"Malformed teleport row: {values}")
        rows.append(dict(zip(FIELDS, values)))
    return rows


class DestinationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = read_cpp(ROOT / "src/teleport_data.cpp")
        with (ROOT / "destination-audit.csv").open(encoding="utf-8", newline="") as stream:
            cls.audit = list(csv.DictReader(stream))

    def test_complete_audit_and_preserved_data(self):
        self.assertEqual(len(self.audit), 226)
        self.assertEqual([int(r["donor_index"]) for r in self.audit], list(range(226)))
        # Audit remains the immutable donor record; UI labels, icons and menu
        # membership may change, but every gameplay tuple (including duplicates) stays.
        gameplay = ("cost", "level", "faction", "map", "x", "y", "z", "o")
        renamed = {"Громовой утес": "Громовой Утёс", "Мертвые Копи": "Мёртвые копи",
                   "Трон Четырех Ветров": "Трон Четырёх Ветров", "Пиратская бухта": "Пиратская Бухта",
                   "К-З": "К-3", "Подземелья Нексуса": "Нексус — общий вход",
                   "Ледяные залы": "Ледяные залы — общий вход"}
        expected = []
        for record in self.audit:
            if record["status"] == "excluded" or record["next_menu_id"] != "0":
                continue
            name = record["name"].split("|t ")[-1].replace("\\'", "'")
            expected.append((renamed.get(name, name),) + tuple(record[field] for field in gameplay))
        self.assertCountEqual([(r["name"],) + tuple(r[f] for f in gameplay) for r in self.rows if r["next_menu_id"] == "0"], expected)
        self.assertEqual(len(self.rows), 210)

    def test_map_and_coordinate_bounds(self):
        maps = set()
        for row in self.rows:
            if int(row["next_menu_id"]):
                continue
            map_id = int(row["map"])
            maps.add(map_id)
            self.assertIn(map_id, {0, 1, 229, 230, 530, 571, 646, 732})
            coords = [float(row[f].removesuffix("f")) for f in ("x", "y", "z", "o")]
            self.assertTrue(all(math.isfinite(n) for n in coords))
            self.assertTrue(all(abs(n) < 17066.667 for n in coords[:3]))
            self.assertTrue(0 <= coords[3] <= 2 * math.pi)
        self.assertEqual(maps, {0, 1, 229, 230, 530, 571, 646, 732})

    def test_navigation_for_both_factions_and_all_levels(self):
        menus = {int(r["menu_id"]) for r in self.rows}
        self.assertFalse(menus & {6, 13})
        for faction in (1, 2):
            for level in range(1, 86):
                graph = {menu: set() for menu in menus}
                for menu in menus:
                    visible = [r for r in self.rows if int(r["menu_id"]) == menu
                               and int(r["level"]) <= level and int(r["faction"]) in (0, faction)]
                    self.assertTrue(visible, (faction, level, menu))
                    self.assertLessEqual(len(visible) + 1, 32)  # Explicit Close item.
                    for row in visible:
                        target = int(row["next_menu_id"])
                        if target:
                            self.assertIn(target, menus)
                            graph[menu].add(target)
                            self.assertEqual(row["cost"], "0")
                def reachable(start):
                    seen, todo = set(), [start]
                    while todo:
                        current = todo.pop()
                        if current not in seen:
                            seen.add(current)
                            todo.extend(graph[current] - seen)
                    return seen
                shown = reachable(1)
                allowed_destinations = [r for r in self.rows if r["next_menu_id"] == "0"
                                        and int(r["faction"]) in (0, faction) and int(r["level"]) <= level]
                self.assertLessEqual({int(r["menu_id"]) for r in allowed_destinations}, shown)
                for menu in shown:
                    self.assertIn(1, reachable(menu))
                self.assertNotIn(15 if faction == 2 else 14, shown)

    def test_restrained_layout_and_category_membership(self):
        for row in self.rows:
            self.assertNotIn("|", row["name"])
            self.assertIn(row["icon"], ("GOSSIP_ICON_TALK", "GOSSIP_ICON_TAXI"))
        roots = {int(r["next_menu_id"]): r["name"] for r in self.rows if r["menu_id"] == "1"}
        self.assertEqual(roots, {14: "Города Альянса", 15: "Города Орды", 16: "Нейтральные города",
                               2: "Подземелья: Classic", 3: "Подземелья: BC", 4: "Подземелья: WotLK",
                               5: "Подземелья: Cataclysm", 7: "Рейды", 20: "PvP и арены", 17: "Путешествия по миру"})
        for row in self.rows:
            if row["name"] == "Назад":
                self.assertEqual(int(row["next_menu_id"]), 17 if int(row["menu_id"]) in (8, 9, 10, 11, 12) else 1)
        for name in ("Твердыня Крыла Тьмы", "Сумеречный бастион", "Трон Четырёх Ветров",
                     "Крепость Барадин", "Огненные Просторы", "Душа Дракона"):
            self.assertEqual([r["menu_id"] for r in self.rows if r["name"] == name], ["7"])
        self.assertTrue(all(r["menu_id"] == "4" for r in self.rows if r["name"] == "Нексус"))

    def test_payment_and_restriction_fields(self):
        destinations = [r for r in self.rows if r["next_menu_id"] == "0"]
        self.assertEqual(len(destinations), 180)
        for row in self.rows:
            self.assertTrue(0 <= int(row["cost"]) <= 2**32 - 1)
            self.assertEqual(int(row["cost"]), 0)
            self.assertEqual(int(row["level"]), 0)
            self.assertIn(int(row["faction"]), (0, 1, 2))

    def test_exclusions_and_classic_recovery(self):
        excluded = [r for r in self.audit if r["status"] == "excluded"]
        self.assertEqual(len(excluded), 24)
        self.assertEqual(sum(r["next_menu_id"] == "0" for r in excluded), 20)
        moved = [r for r in self.audit if r["status"] == "moved"]
        self.assertEqual(len(moved), 2)
        self.assertTrue(all(r["menu_id"] == "6" and r["target_menu_id"] == "2" for r in moved))


if __name__ == "__main__":
    unittest.main()
