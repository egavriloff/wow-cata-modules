# NPC Teleport for TCPP 4.3.4

A static gossip teleporter for WoW Cataclysm **4.3.4.15595**, compatible with
TCPP revision `b061f46ef86f84f713ea3956b2318383e0b71278` and workspace integration
API 1. No upstream source changes, runtime DLL plugin, configuration loader or
SQL updater are required.

The functional reference is the supplied `.build/porting/npc-teleport/` SkyFire
5.4.8 port of
[alexkulya/pandaria_5.4.8's original script](https://github.com/alexkulya/pandaria_5.4.8/blob/master/src/server/scripts/Custom/npc_teleport.cpp).
See [NOTICE.md](NOTICE.md) and [COPYING](COPYING) for attribution and
GPL-2.0-or-later terms. The supplied donor has no pinned source revision.

## Destinations and presentation

The port retains **180 destinations and 30 navigation rows** across cities,
Classic/TBC/WotLK/Cataclysm dungeons, raids, Eastern Kingdoms, Kalimdor,
Outland, Northrend, Cataclysm zones and outdoor PvP locations. Faction restrictions, costs, minimum levels, map IDs, coordinates and
orientation are preserved for every retained destination. Labels, icons and menu
membership are polished as described below. All shipped costs and minimum levels
are zero; the donor has no class restriction. Core starting-zone/class/race
restrictions still apply through `Player::TeleportTo`.

[destination-audit.csv](destination-audit.csv) accounts for **all 226 donor
rows**, with the exact original fields, disposition, reason and original port
menu assignment. It is a historical donor audit; current UI menu assignments
are defined in `src/teleport_data.cpp`.
`donor_index` is zero based. No replacement coordinates or silent map redirects
were introduced. Two compatible outdoor entrances originally grouped under
Pandaria, Scholomance and Scarlet Monastery, move to Classic dungeons with
their original map/position/orientation intact. The removed categories have no
remaining links, including their back links.

Only Russian player-facing text was supplied. It remains Russian for every
client locale; there is no English translation or new localization framework.
Additional messages live in `src/teleport_text.h`. The original donor's custom
`npc_text` IDs 300000–300004 were not supplied. The polished menu uses its own
Russian greeting in `npc_text.ID=900100`, installed by the module SQL. Large
embedded texture tags, donor bracketed navigation and popup padding are removed.

### Current menu layout

| Main-menu item | Contents |
| --- | --- |
| Города Альянса / Города Орды | Only the player's own faction category is shown; original city restrictions remain |
| Нейтральные города | Dalaran and Shattrath |
| Подземелья: Classic | Classic approaches, including Scholomance and Scarlet Monastery |
| Подземелья: BC | The Burning Crusade approaches |
| Подземелья: WotLK | Wrath approaches, now also including the former Cataclysm-menu Nexus entry |
| Подземелья: Cataclysm | Cataclysm dungeon approaches; raid approaches moved to Raids |
| Рейды | Original raid approaches plus Blackwing Descent, Bastion of Twilight, Throne of the Four Winds, Baradin Hold, Firelands and Dragon Soul |
| PvP и арены | Existing outdoor PvP locations and arena approaches; no battleground queue or new map |
| Путешествия по миру | Eastern Kingdoms, Kalimdor, Outland, Northrend and Cataclysm zones |

The visible root contains nine categories plus **Закрыть**. Cities, dungeons,
raids and PvP destinations are one category below the root; world zones are two.
All child pages end with **Назад**, then **Закрыть**. Zone Back returns to the
world page; other Back items return to the root. The client-native farewell
button remains available. The longest page has 29 items including Back/Close,
within TCPP's 32-item limit. No empty opposing-faction city category is shown.

Categories and navigation use the small standard gossip talk icon; destinations
use the standard taxi icon. No embedded textures, oversized icons or color tags
are used in menu labels. Expansion names consistently use Classic / BC / WotLK /
Cataclysm. A few spelling/case corrections are explicit in the preservation test;
shared Nexus and Frozen Halls approaches are labelled as common entrances.
Confirmation reads `Отправиться в выбранное место?`, followed by the destination.
The greeting is `Куда отправимся, $N? Выберите раздел, а затем место назначения.`

For an **already installed, working NPC**, apply only
[sql/world/gossip_text.sql](sql/world/gossip_text.sql) manually to the world
DB and restart worldserver after deploying the new executable. That file only
upserts this module's greeting. Fresh installation includes the same text in
`install.sql`; no NPC flags, model or gameplay SQL change was needed for UX.
Without the text update, TCPP's standard missing-text greeting remains usable.

### Excluded destinations

| Donor destinations (English names) | Reason |
| --- | --- |
| Shado-Pan Monastery; Mogu'shan Palace; Gate of the Setting Sun; Siege of Niuzao Temple; Temple of the Jade Serpent; Stormstout Brewery | MoP destinations on map 870 |
| Scarlet Halls | Introduced in MoP; an exterior position on map 0 does not make this instance available in Cataclysm |
| Terrace of Endless Spring; Heart of Fear; Mogu'shan Vaults; Siege of Orgrimmar | MoP destinations on map 870, including the donor's Vale approach for Siege of Orgrimmar |
| Throne of Thunder | MoP Isle of Thunder approach on map 1064 |
| Jade Forest; Valley of the Four Winds; Krasarang Wilds; Kun-Lai Summit; Townlong Steppes; Dread Wastes; Vale of Eternal Blossoms | MoP zones on map 870 |
| Isle of Quel'Danas | Donor uses map 0 for coordinates belonging to the island on map 530. Excluded rather than silently changing the map; the valid Magisters' Terrace and Sunwell approaches on map 530 remain |

Thus **19 post-Cataclysm destinations**, one incorrect map/position pairing and
four category navigation rows are excluded. IDs 6 and 13 are no longer menus.
The UX revision moves Nexus from the Cataclysm category to WotLK and the six
Cataclysm raid approaches to Raids. Every destination row remains available,
including shared entrances and duplicate approaches.

### Map and access audit

| Retained MapID | Cataclysm meaning and handling |
| --- | --- |
| 0 | Eastern Kingdoms; includes Cataclysm zones and outdoor/underwater entrance approaches |
| 1 | Kalimdor; includes Mount Hyjal, Uldum and Caverns of Time approaches |
| 530 | Outland and TBC starting/island zones; includes Silvermoon, Exodar, Magisters' Terrace and Sunwell approaches |
| 571 | Northrend; includes shared outdoor approaches for several dungeon entries |
| 646 | Deepholm world map and Stonecore outdoor entrance |
| 732 | Tol Barad world map, including the Baradin Hold approach; battlefield control/phasing remains the core's responsibility |
| 229 | Blackrock Spire instance: the donor's **Blackwing Lair approach is inside Spire**, not map 469. Normal instance difficulty/access/limits apply |
| 230 | Blackrock Depths instance: the donor's **Molten Core approach is inside Depths**, not map 409. Normal instance difficulty/access/limits apply |

These map identities are consistent with the checked-out TCPP scripts and SQL,
including Blackrock Spire/Depths, Deepholm and Tol Barad implementations. The
build does not include extracted client `Map.dbc` or terrain data for an offline
terrain-height check. At runtime the script requires a DBC map entry with
expansion <= Cataclysm, valid coordinates, available map/vmap files through
`TerrainMgr::ExistMapAndVMap`, and `Map::PlayerCannotEnter == CAN_ENTER` before
calling `TeleportTo`. Missing or incompatible terrain files reject teleportation.
VMAP availability follows the core's VMAP enablement settings.

Most dungeon/raid entries are **entrance approaches**, not direct instance
teleports. Core area triggers enforce final entry conditions. No raid-group,
attunement, instance binding, map-disable or starting-zone check is bypassed.
No quest credit, phasing, underwater breathing, flight, portal activation or
Tol Barad/Wintergrasp ownership is granted. Shared destination coordinates are
intentional donor data. Actual terrain heights and safe arrivals, particularly
underwater, flooded or phased Cataclysm areas, still require in-game testing.

## Build integration

From the workspace root, use the existing dependency-configured Windows build:

```powershell
.\tools\build.ps1
```

`module.json` is discovered automatically; CMake calls `tcpp_register_module`
once. `Addmod_npc_teleportScripts()` registers one `npc_teleport` CreatureScript.
TCPP's current API uses `CreatureScript::GetAI` with
`PassiveAI::GossipHello/GossipSelect`, rather than the donor's obsolete direct
`OnGossipHello/OnGossipSelect` overrides. The generated static workspace loader
calls the entrypoint once after the original core loader.

The module defaults ON with workspace modules enabled. To disable it, configure
the existing build directory with `-DTCPP_MODULE_MOD_NPC_TELEPORT=OFF`, then
rebuild/redeploy. No edits to the integration framework are needed.

## NPC installation and model migrations

Use the workspace SQL updater against the fully updated TCPP world schema;
see [SQL migration workflow](../../docs/sql-migrations.md). Back up the database
and stop worldserver/other writers before applying migrations.

The module's `sql/migrations.json` preserves this explicit order:

1. `world/install.sql`: original template, script binding and greeting.
2. `world/gossip_text.sql`: greeting migration.
3. `world/002_model.sql`: current crystal model, **CreatureDisplayID 11490**.

The crystal display was verified with a read-only query against `tcpp_world`
on 2026-10-28: `creature` GUID **396674**, `id=900100`, `modelid=11490`.
The template still contained the legacy wizard display 26078. Published SQL
and its checksums remain unchanged; do not rerun the original installation
SQL to update an existing NPC, because it restores that old wizard model.

```powershell
.venv/Scripts/python.exe tools/sql-updater/update.py verify --target world --config .build/install/windows-modules/RelWithDebInfo/worldserver.conf
.venv/Scripts/python.exe tools/sql-updater/update.py plan --target world --module mod-npc-teleport --config .build/install/windows-modules/RelWithDebInfo/worldserver.conf
.venv/Scripts/python.exe tools/sql-updater/update.py apply --target world --module mod-npc-teleport --config .build/install/windows-modules/RelWithDebInfo/worldserver.conf
```

For an installation whose original migrations are already recorded as applied,
only the new model migration is pending. Fresh installations run all three in
order. Existing manually installed data without migration history requires the
separate reviewed adoption workflow; do not rewrite history or old assertions.

The new migration replaces only NPC 900100's `creature_template_model` rows
with one index-0 row (display 11490, probability 1, VerifiedBuild 0). Existing
spawns use `modelid=0` to inherit that model and `npcflag=NULL` to inherit the
template's Gossip flag. A spawn-level zero disables Gossip even if the template
has `npcflag=1`. Template name, faction, flags and `ScriptName='npc_teleport'`
are preserved, as are spawn positions, scripts, unit flags and all other settings.
No spawns are added. Repeating the new migration produces the same state.

Restart worldserver after applying SQL to load the model and flags, then check
the existing NPC's appearance and Gossip with normal Horde/Alliance characters.
No worldserver rebuild is required for this SQL-only update. See
[runtime diagnostics](runtime-diagnostics.md) for script registration checks.
The module's `npc_text` greeting does not need database gossip menu bindings.

## Selection and payment safety

The script validates the actual server-side gossip item, menu ID, NPC/source,
sender, action bounds, menu membership and current faction/level before using
the destination. It clears the menu before navigation/teleportation and checks
combat, alive/in-world/flight/vehicle/teleport state, map availability, instance
access and balance. A forged index is never used as a teleport array index.

Navigation is free. Costs are widened from uint32 to int64 **before negation**.
Money changes only after `TeleportTo` accepts the request. The AI handler
consumes the selection, so core database-gossip payment handling does not run
again. Accepted asynchronous transfers can still fail later at worldport ACK;
there is no new transfer/refund subsystem. Shipped destinations are all free,
so that remaining core transfer lifecycle cannot charge players in this data set.

## Validation (2026-10-27)

Executed successfully on Windows with the existing dependencies and workflow:

- Manifest validation/discovery: both `mod-example` and `mod-npc-teleport`
  selected in deterministic order.
- Existing Python integration suite: **25 tests passed, no skips**.
- Module suite: **15 tests passed, no skips**. Six offline data tests cover all
  226 audited donor rows, exact retained fields, exclusions, finite/bounded
  coordinates, approved map IDs, costs, both factions and every level 1–85,
  menu capacity, reachability and back navigation.
- One C++ harness compiles the **actual script** with minimal test doubles and
  executes **29 gossip/payment/registration scenarios**: invalid list/action,
  wrong menu/sender/source, faction/level changes, combat and state restrictions,
  unavailable maps/terrain/access, insufficient money, `TeleportTo` rejection,
  uint32 maximum cost, repeated selection, navigation and single registration.
  The hello test also checks populated menus, the interaction source and faction
  filtering. These doubles do not validate TCPP services or network packets.
- Eight installation tests parse fresh/repair SQL values, reconstruct the four
  referenced tables from the repository base DDL and chronologically ordered
  world updates, and check every INSERT/upsert, UPDATE, DELETE and SELECT column.
  They also check mandatory fields, NULL spawn-flag inheritance, Windle's model,
  script-name/entrypoint consistency, greeting consistency between full/text-only
  installers, and strict source/SQL UTF-8. Negative cases
  reject obsolete columns and unsupported installer statements. Unknown table
  alteration syntax fails validation rather than silently retaining old fields.
- A compiled UTF-8 probe emits all **215 actual module strings** (210 data labels
  plus five messages), which must decode strictly and equal the source strings.
  MSVC `/utf-8` is scoped to this module target, with no global encoding change.
- `tools/build.ps1`: real TCPP CMake configure, `RelWithDebInfo` compile/link,
  install and dependency copying succeeded with both modules enabled. The
  module objects and generated loader are linked into the real worldserver.
- Static generated-loader, symbol and linked script-name checks establish
  linkage; runtime registration after loading a real world database is untested.

Reproduce from the workspace root:

```powershell
python tools/module-integration/integration.py discover --output .build/npc-teleport-discovery
$env:CMAKE = 'D:\DevTools\cmake\bin\cmake.exe' # existing host's CMake, or use your own
python -m unittest discover -s tools/module-integration/tests -v
python -m unittest discover -s modules/mod-npc-teleport/tests -v
.\tools\build.ps1
```

Build output: `.build/npc-teleport-ux-build.log`; integration tests:
`.build/npc-teleport-fix-integration-tests.log`; module tests:
`.build/npc-teleport-ux-tests.log`. Generated selection and loader are under
`.build/windows-modules/workspace-modules/`; installed worldserver is under
`.build/install/windows-modules/RelWithDebInfo/`. Existing nonfatal CMP0218
integration policy warnings remain; the integration framework was not changed.

The first sandboxed build/test attempts failed because Git metadata and compiler
temporary directories were inaccessible; reruns with the required filesystem
access passed. No server startup, SQL execution, database connectivity or
in-game testing was performed. NPC visibility, actual client menus/icons,
all arrival positions, phase/quest interactions and real core access failures
remain operator acceptance checks. No commits, pushes or submodule revision
updates were made.

## Runtime investigation and remaining acceptance

The supplied `DBErrors.log` contains `Script named 'npc_teleport' does not have
a script name assigned in database.` TCPP's registry rejects such an object at
startup. The original installer also skipped repairs whenever the template
already existed. The new installer repairs that state before a full restart.
The log lacks timestamps, so it does not prove which restart lacked the binding.
The old deployed executable matched the prior build SHA-256 and contained all
206 expected UTF-8 strings. All three supplied log files decoded as UTF-8;
the reported conversion-error text was absent from them. No evidence identifies
this module as the source of that intermittent console error.

See [runtime-diagnostics.md](runtime-diagnostics.md) for the inspected source
chain, configuration for quiet-by-default debug logging, and precise in-game
and UTF-8 reproduction steps. These fixes have not been deployed automatically;
SQL has not been executed, and successful gossip operation with the repaired
live database remains an operator verification.

The operator confirmed that the pre-UX module, wizard appearance, gossip and
category teleports work in game. The UX revision was compiled and tested offline;
its exact client rendering, personalized greeting and close/back behavior still
need an in-game review after deployment. Tests compare every destination's
normalized label and gameplay tuple against the donor audit, including duplicate
rows; only presentation, grouping and navigation were changed.
