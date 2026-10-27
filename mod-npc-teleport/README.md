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

The port retains **180 destinations and 22 navigation rows** across cities,
Classic/TBC/WotLK/Cataclysm dungeons, raids, Eastern Kingdoms, Kalimdor,
Outland, Northrend, Cataclysm zones and outdoor PvP locations. Original order,
Russian labels, icons, faction restrictions, costs, coordinates and orientation
are preserved for retained destinations. All shipped costs and minimum levels
are zero; the donor has no class restriction. Core starting-zone/class/race
restrictions still apply through `Player::TeleportTo`.

[destination-audit.csv](destination-audit.csv) accounts for **all 226 donor
rows**, with the exact original fields, disposition, reason and target menu.
`donor_index` is zero based. No replacement coordinates or silent map redirects
were introduced. Two compatible outdoor entrances originally grouped under
Pandaria, Scholomance and Scarlet Monastery, move to Classic dungeons with
their original map/position/orientation intact. The removed categories have no
remaining links, including their back links.

Only Russian player-facing text was supplied. It remains Russian for every
client locale; there is no English translation or new localization framework.
Additional messages live in `src/teleport_text.h`. The donor references custom
`npc_text` IDs 300000–300004 but supplies no text or SQL records. This port uses
TCPP's `DEFAULT_GOSSIP_MESSAGE` instead of depending on missing records; custom
greeting text cannot be recovered from the supplied module. Existing icons and
spelling (including questionable icon paths) are preserved.

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
Existing expansion/category oddities such as Nexus under Cataclysm and raids
under its dungeon menu are otherwise retained.

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

## Optional NPC installation

SQL is **manual**, targets the **world** database, and was not executed during
development. Install only against a fully updated world schema matching the
pinned core; the old base schema alone is insufficient.

1. Review [sql/world/install.sql](sql/world/install.sql). Entry **900100** has no
   matches in the checked-out TCPP SQL/source or pre-existing workspace module.
   Check both `creature_template.entry` and `creature_template_model.CreatureID`
   in your live database first; custom live data cannot be checked offline.
2. If unused, run the SQL manually with a UTF-8 capable MySQL client against
   your world database. It inserts a dedicated level 85 neutral teleporter,
   `npcflag=1` (`UNIT_NPC_FLAG_GOSSIP`), empty `AIName` and
   `ScriptName='npc_teleport'`, plus its display model record. Display ID 19646
   is present in TCPP's `2023_09_15_00_world.sql` for client build 15595.
3. The script checks both tables, never deletes/updates/replaces existing
   content, and inserts the model only when it just inserted the template.
   A pre-existing ID leaves everything unchanged. Inspect the final SELECTs;
   an unrelated occupant/orphan model needs operator review and a different
   checked entry in the SQL and spawn command. Repeating installation does not
   repair existing records. Do not use a MySQL mode that continues after SQL
   errors; investigate and roll back a failed transaction.
4. Restart worldserver after installation, log in with appropriate GM command
   permission, stand at the desired location and run:

   ```text
   .npc add 900100
   ```

   TCPP creates the persistent `creature` spawn with the current position/phase;
   no fixed GUID or arbitrary spawn coordinates are included. Use a normal
   visible world phase. Check faction menus with normal Horde and Alliance
   characters as well as a GM.

Current `ObjectMgr::LoadCreatureTemplateModels` uses `creature_template_model`;
legacy `modelid1` alone would leave the NPC without an effective display model.
The `creature` spawn has no overriding `ScriptName`, so the template's binding
is used. No SQL updater/migration framework is installed by this module.

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
- Module suite: **6 tests passed, no skips**. Five offline data tests cover all
  226 audited donor rows, exact retained fields, exclusions, finite/bounded
  coordinates, approved map IDs, costs, both factions and every level 1–85,
  menu capacity, reachability and back navigation.
- One C++ harness compiles the **actual script** with minimal test doubles and
  executes **27 gossip/payment/registration scenarios**: invalid list/action,
  wrong menu/sender/source, faction/level changes, combat and state restrictions,
  unavailable maps/terrain/access, insufficient money, `TeleportTo` rejection,
  uint32 maximum cost, repeated selection, navigation and single registration.
  These doubles do not validate TCPP services or network packets.
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

Build output: `.build/npc-teleport-build.log`; integration tests:
`.build/npc-teleport-integration-tests.log`; module tests:
`.build/npc-teleport-tests.log`. Generated selection and loader are under
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
