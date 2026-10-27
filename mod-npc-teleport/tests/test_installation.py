"""Validate installation inputs against checked-out schema without executing SQL."""
import csv
import io
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parents[1]


def inserts(sql):
    statements = {}
    sql = re.sub(r"--[^\n]*", "", sql)
    for match in re.finditer(r"INSERT INTO `(\w+)`\s*\((.*?)\)\s*VALUES\s*\((.*?)\)\s*ON DUPLICATE KEY UPDATE\s*(.*?);", sql, re.S):
        columns = re.findall(r"`(\w+)`", match[2])
        values = next(csv.reader(io.StringIO(match[3].replace("\n", " ")), quotechar="'", skipinitialspace=True))
        if len(columns) != len(values):
            raise ValueError("Column/value count mismatch")
        updates = dict(re.findall(r"`(\w+)`\s*=\s*('[^']*'|\d+)", match[4]))
        statements[match[1]] = (dict(zip(columns, values)), {k: v.strip("'") for k, v in updates.items()})
    return statements


def table_columns(source, table):
    match = re.search(r"CREATE TABLE (?:IF NOT EXISTS )?`" + table + r"`\s*\((.*?)\) ENGINE", source, re.S)
    if not match:
        raise ValueError(f"Missing schema for {table}")
    return dict(re.findall(r"^\s*`(\w+)`\s+([^\n]+)", match[1], re.M))


TABLES = ("creature_template", "creature", "creature_template_model")


def current_schema(core):
    """Replay relevant repository DDL, not data, without connecting to a database.

    This intentionally supports only DDL forms used by these three tables.
    Unknown changes fail closed rather than silently validating an old schema.
    """
    schema = {}
    paths = [core / "sql/base/dev/world_database.sql"]
    paths += sorted((core / "sql/updates/world/4.3.4").glob("*.sql"))
    names = "|".join(TABLES)
    pattern = (r"(CREATE TABLE(?: IF NOT EXISTS)?|DROP TABLE(?: IF EXISTS)?|ALTER TABLE)"
               r"\s+`(" + names + r")`\s*(.*?);")
    for path in paths:
        source = re.sub(r"--[^\n]*", "", path.read_text(encoding="utf-8-sig"))
        for match in re.finditer(pattern, source, re.S | re.I):
            operation, table, body = match.groups()
            operation = operation.upper()
            if operation.startswith("DROP TABLE"):
                schema.pop(table, None)
            elif operation.startswith("CREATE TABLE"):
                schema[table] = table_columns(match[0], table)
            else:
                for clause in body.split(","):
                    clause = clause.strip()
                    change = re.fullmatch(r"(ADD|MODIFY) (?:COLUMN )?`(\w+)` (.+)", clause, re.S | re.I)
                    rename = re.fullmatch(r"CHANGE (?:COLUMN )?`(\w+)` `(\w+)` (.+)", clause, re.S | re.I)
                    drop = re.fullmatch(r"DROP (?:COLUMN )?`(\w+)`", clause, re.I)
                    columns = schema[table]
                    if change:
                        action, column, spec = change.groups()
                        if (action.upper() == "ADD") == (column in columns):
                            raise ValueError(f"Unexpected {action} {table}.{column} in {path.name}")
                        columns[column] = spec
                    elif rename:
                        old, new, spec = rename.groups()
                        del columns[old]
                        columns[new] = spec
                    elif drop:
                        del columns[drop[1]]
                    else:
                        raise ValueError(f"Unsupported {table} DDL in {path.name}: {clause}")
    if set(schema) != set(TABLES):
        raise ValueError("Incomplete TCPP schema")
    return schema


def validate_sql(sql, schema):
    """Check every installer statement and every quoted identifier; reject unknown SQL."""
    used = {table: set() for table in TABLES}
    sql = re.sub(r"--[^\n]*", "", sql)
    for statement in sql.split(";"):
        statement = statement.strip()
        if not statement or statement in ("SET NAMES utf8mb4", "START TRANSACTION", "COMMIT"):
            continue
        patterns = (
            r"INSERT INTO `(\w+)`\s*\(.*?\)\s*VALUES\s*\(.*?\)\s*ON DUPLICATE KEY UPDATE\s+.+",
            r"UPDATE `(\w+)` SET .+ WHERE .+",
            r"DELETE FROM `(\w+)` WHERE .+",
            r"SELECT\s+.+\s+FROM `(\w+)` WHERE .+",
        )
        matches = [re.fullmatch(p, statement, re.S) for p in patterns]
        match = next((m for m in matches if m), None)
        if not match or match[1] not in schema:
            raise ValueError(f"Unsupported installation statement: {statement}")
        table = match[1]
        identifiers = re.findall(r"`(\w+)`", statement)
        identifiers.remove(table)
        columns = set(identifiers)
        missing = columns - set(schema[table])
        if missing:
            raise ValueError(f"Unknown columns in {table}: {sorted(missing)}")
        used[table].update(columns)
    return used


class InstallationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sql = (ROOT / "sql/world/install.sql").read_text(encoding="utf-8")
        cls.parsed = inserts(cls.sql)
        cls.core = WORKSPACE / "externals/core"
        cls.schema = current_schema(cls.core)

    def test_fresh_install_and_repair_have_same_required_values(self):
        self.assertEqual(set(self.parsed), {"creature_template", "creature_template_model"})
        template, template_update = self.parsed["creature_template"]
        self.assertEqual(template["entry"], "900100")
        self.assertEqual(template["ScriptName"], "npc_teleport")
        self.assertEqual(int(template["npcflag"]) & 1, 1)
        self.assertEqual(template["AIName"], "")
        self.assertEqual(template["faction"], "35")
        self.assertEqual({k: v for k, v in template.items() if k != "entry"}, template_update)
        model, model_update = self.parsed["creature_template_model"]
        self.assertEqual(model["CreatureID"], "900100")
        self.assertEqual(model["Idx"], "0")
        self.assertEqual(model["CreatureDisplayID"], "26078")
        self.assertGreater(float(model["Probability"]), 0)
        self.assertEqual({k: v for k, v in model.items() if k not in ("CreatureID", "Idx")}, model_update)

    def test_columns_and_mandatory_fields_against_tcpp(self):
        used = validate_sql(self.sql, self.schema)
        self.assertTrue(all(used.values()))
        for table in ("creature_template", "creature_template_model"):
            columns = self.schema[table]
            values, updates = self.parsed[table]
            self.assertLessEqual(set(values) | set(updates), set(columns))
            mandatory = {k for k, spec in columns.items() if "NOT NULL" in spec.upper() and "DEFAULT" not in spec.upper()}
            self.assertLessEqual(mandatory, set(values))
        spawn_columns = self.schema["creature"]
        stripped = re.sub(r"--[^\n]*", "", self.sql)
        update = re.search(r"UPDATE `creature` SET (.*?) WHERE `id` = (\d+);", stripped, re.S)
        self.assertEqual(update[2], "900100")
        self.assertLessEqual(set(re.findall(r"`(\w+)`\s*=", update[1])), set(spawn_columns))
        delete = re.search(r"DELETE FROM `(\w+)` WHERE `CreatureID` = (\d+) AND `Idx` <> 0;", stripped)
        self.assertEqual(delete.groups(), ("creature_template_model", "900100"))

    def test_applied_migrations_remove_obsolete_columns(self):
        for table in ("creature", "creature_template"):
            self.assertNotIn("dynamicflags", self.schema[table])
        self.assertNotIn("gossip_menu_id", self.schema["creature_template"])
        self.assertNotIn("exp", self.schema["creature_template"])
        self.assertIn("HealthScalingExpansion", self.schema["creature_template"])
        self.assertEqual(set(self.schema["creature_template_model"]),
                         {"CreatureID", "Idx", "CreatureDisplayID", "Probability", "VerifiedBuild"})
        for field in ("npcflag", "unit_flags", "unit_flags2"):
            spec = self.schema["creature"][field].upper()
            self.assertIn("NULL", spec)
            self.assertNotIn("NOT NULL", spec)

    def test_all_statement_types_reject_obsolete_columns(self):
        # Mutation checks include the spawn UPDATE and final SELECT, not just the INSERT.
        for statement in (
            "INSERT INTO `creature_template` (`entry`, `dynamicflags`) VALUES (900100, 0) ON DUPLICATE KEY UPDATE `dynamicflags` = 0;",
            "UPDATE `creature` SET `dynamicflags` = 0 WHERE `id` = 900100;",
            "SELECT `DisplayScale` FROM `creature_template_model` WHERE `CreatureID` = 900100;",
            "DELETE FROM `creature_template_model` WHERE `DisplayScale` = 1;",
        ):
            with self.subTest(statement=statement), self.assertRaisesRegex(ValueError, "Unknown columns"):
                validate_sql(statement, self.schema)
        with self.assertRaisesRegex(ValueError, "Unsupported installation statement"):
            validate_sql("TRUNCATE TABLE `creature`;", self.schema)

    def test_spawn_flags_inherit_instead_of_disabling_gossip(self):
        stripped = re.sub(r"--[^\n]*", "", self.sql)
        update = re.search(r"UPDATE `creature` SET (.*?) WHERE `id` = 900100;", stripped, re.S)[1]
        assignments = dict(re.findall(r"`(\w+)`\s*=\s*(NULL|\d+|'[^']*')", update))
        self.assertEqual(assignments, {"modelid": "0", "npcflag": "NULL",
                         "unit_flags": "NULL", "unit_flags2": "NULL", "ScriptName": "''"})
        template = self.parsed["creature_template"][0]
        # TCPP ChooseCreatureFlags: a present zero override wins; only NULL inherits.
        effective = lambda override: int(template["npcflag"]) if override is None else override
        self.assertEqual(effective(None) & 1, 1)
        self.assertEqual(effective(0) & 1, 0)

    def test_windle_display_from_cataclysm_tcpp_data(self):
        source = (self.core / "sql/updates/world/4.3.4/2023_09_15_00_world.sql").read_text(encoding="utf-8")
        records = [tuple(map(int, m)) for m in re.findall(r"\(29261,\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)", source)]
        self.assertIn((0, 26078, 1, 15595), records)
        self.assertNotEqual(int(self.parsed["creature_template_model"][0]["CreatureDisplayID"]), 29261)

    def test_utf8_source_and_script_name_contract(self):
        for folder in (ROOT / "src", ROOT / "sql"):
            for path in folder.rglob("*"):
                if path.is_file():
                    raw = path.read_bytes()
                    text = raw.decode("utf-8", errors="strict")
                    self.assertNotIn("\ufffd", text, path)
                    self.assertFalse(raw.startswith(b"\xef\xbb\xbf"), path)
        manifest = json.loads((ROOT / "module.json").read_text(encoding="utf-8"))
        source = (ROOT / "src/npc_teleport.cpp").read_text(encoding="utf-8")
        entry = re.findall(r"void (Add\w+Scripts)\(\)", source)
        self.assertEqual(entry, [manifest["entrypoint"]])
        names = re.findall(r'CreatureScript\("([^"]+)"\)', source)
        self.assertEqual(names, [self.parsed["creature_template"][0]["ScriptName"]])


if __name__ == "__main__":
    unittest.main()
