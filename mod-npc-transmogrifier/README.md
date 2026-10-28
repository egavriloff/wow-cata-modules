# NPC Transmogrifier for TCPP 4.3.4

Static workspace module targeting TCPP
`b061f46ef86f84f713ea3956b2318383e0b71278` and client 4.3.4.15595.
See [compatibility investigation](compatibility.md) and [attribution](NOTICE.md).
Compilation and offline tests do not establish in-game correctness.
Executed checks and binary linkage evidence are recorded in [verification.md](verification.md).

## Installation

1. Build from the workspace with `./tools/build.ps1 -Configuration RelWithDebInfo`.
   `module.json` enables discovery; `Addmod_npc_transmogrifierScripts` registers
   the `npc_transmogrifier` CreatureScript and configuration WorldScript.
2. Installation creates `modules/npc-transmogrifier.conf` beside the installed
   `worldserver.conf`. Edit that file; its `[worldserver]` section is required.
   The module continues to use native `ConfigMgr` getters. The template
   `conf/npc-transmogrifier.conf.dist` preserves every previous key/default.
   Reinstallation preserves the user `.conf` and refreshes the adjacent `.conf.dist`;
   compare and merge new defaults manually. Missing required files or malformed
   INI abort startup; a failed reload preserves the previous configuration.
   For an existing installation, copy your `Transmogrification.*` values from
   `worldserver.conf` into the module file, then remove those main-file entries.
   Until removed, main-file values take precedence with a migration warning.
   Back up both files before migration. Never call `LoadInitial` from the module.
3. Back up databases and stop worldserver/other writers. Review the migrations.
   Use the workspace SQL updater explicitly, with the installed configuration:

   ```powershell
   python tools/sql-updater/update.py plan --target characters --target world --module mod-npc-transmogrifier --config .build/install/windows-modules/RelWithDebInfo/worldserver.conf
   python tools/sql-updater/update.py apply --target characters --target world --module mod-npc-transmogrifier --config .build/install/windows-modules/RelWithDebInfo/worldserver.conf
   ```

   No SQL is executed by the build. `sql/characters/001_sets.sql` creates only
   `custom_transmogrification_sets`. `sql/world/001_npc.sql` installs optional
   template 900101, model 26078 and binding `npc_transmogrifier`; it creates no
   spawn. Target selection can install characters independently of world.
   World preconditions refuse existing NPC-900101 data rather than overwrite
   an unknown custom installation. The SQL itself is idempotent for manual
   repair of module-owned rows. No adoption assertions are supplied; an existing
   manual installation needs reviewed assertions before updater adoption.
4. Restart worldserver. Verify the script binding before explicitly spawning
   template 900101 at a location selected by an administrator. No automatic
   spawn is provided. Existing module-owned spawns inherit template flags via
   NULL overrides, modelid=0 and empty per-spawn ScriptName.

The native transmogrifier NPC flag is intentionally absent: the Gossip module
handles its own pricing and compatibility. Adding the native flag also exposes
the separate native packet handler and its pricing rules.

## Gameplay and configuration

Choose an equipped slot, then an eligible item from backpack or equipped bags.
The source's base item entry is used, rather than its own applied appearance.
Source items remain owned by the player; BoE/BoU sources become bound. Source
and target lose refund/group-trade eligibility, following native TCPP behavior.
Individual and full equipped-appearance removal are free.

All supplied donor configuration keys and defaults are retained. The quality,
allow/deny lists, faction, class, race, skill, spell, level, holiday, armor and
weapon rules use actual TCPP template getters. Allowed entries override donor
quality/requirement filters; denied entries otherwise fail. Structural item
class and inventory compatibility still apply. `IgnoreReqStats` remains a
compatibility no-op, as in the supplied donor; statless items are accepted by
the custom Gossip policy. This policy is intentionally more permissive than
`Item::CanTransmogrifyItemWithItem` and is not advertised as native eligibility.
The donor's unsafe unconditional weapon-to-offhand shortcut is removed; mixed
ranged subclasses require both items to be ranged. Cataclysm's separate ranged
equipment slot is included.

Gold is charged on application after validation. Tokens are additional; zero
entry/count with RequireToken enabled rejects the operation. A token entry
matching source or target is rejected to prevent consuming a referenced item.
Negative, non-finite or overflowing prices reject payment rather than becoming
free. Explicit zero prices remain supported. The displayed gold quote is
rechecked at selection, including after configuration reload.

Sets store authorized appearances already present on the owner's equipment.
Creating a set costs the configured appearance-price sum; applying and deleting
it are free, preserving donor behavior. Applying a set does not consume tokens
or require the original source item again. This is an intentional preset policy,
not a collection system. Every saved slot is checked before any item changes;
an incompatible, missing, duplicate or malformed slot rejects the whole set.
At most ten sets are supported. Names are valid UTF-8, 1вЂ“48 bytes, without control
characters or WoW markup separators. Database escaping protects quotes; data
contains only generated integers. Never import untrusted preset rows.

Russian menu actions and confirmations use UTF-8, compiled with `/utf-8` on MSVC.
Item links use TCPP's default-locale template names; this module does not alter
localization projects or provide a per-locale string database.

## Validation and acceptance

```powershell
$env:CMAKE = 'D:\DevTools\cmake\bin\cmake.exe' # optional compiler-tool location
python -m unittest discover -s tools/module-integration/tests -v
python -m unittest discover -s tools/sql-updater/tests -v
python -m unittest discover -s modules/mod-npc-transmogrifier/tests -v
```

The module harness compiles the real script with test doubles for core services.
It checks price overflow, template restrictions, ownership, trade state, menu
origin/ID, stale price quotes, gold/tokens, binding, replay, set parsing and
all-or-nothing application, and loader registration. Offline schema checks use
the checked-out core and the actual workspace SQL catalog/parser. These checks
do not execute MySQL or simulate the 4.3.4 client.

Before deployment, test on disposable databases: migration apply/reapply and
schema, registration at startup, slot/back/close navigation, bag selection,
gold/token deductions and failed payments, individual/all removal, set
create/apply/delete, names with quotes and Russian text, config reload (requires live acceptance), item
binding, and invalidated selections after item movement. Confirm appearance in
inventory tooltips, character paper doll and from a second player's client;
then logout/relog and restart worldserver. Inspect native enchantments and
money/inventory persistence. Verify refund and trade restrictions.

See `compatibility.md` for database failure semantics and unverified runtime
conditions. No server startup, NPC interaction, appearance change or persistence
test against a running server is claimed.

## Configuration reload and upgrades

Startup logs report the database ScriptID for `npc_transmogrifier` on
`server.loading`, with an error if it is zero. For a short diagnostic session,
enable `Logger.modules.npc_transmogrifier=2,Server` in the main server config
(using an existing DEBUG-capable file appender). DEBUG markers identify `GetAI`,
`GossipHello` and a sent main menu with item count. Linking alone does not certify
runtime binding or client display. The workspace's `docs/utf8-gossip-diagnostics.md`
documents the separate Windows console investigation and native-NPC comparison.

The managed bridge reloads the main file and enabled module files as one validated
snapshot. This module already reads its options in `WorldScript::OnConfigLoad`;
no new callback or gameplay change is required. ConfigMgr reload is covered by
an offline C++ harness, but full `.reload config` in a running worldserver is
not verified. Restart is the deployment acceptance path until that is tested.
Changing module selection, filenames or the template's allowed key set requires
reconfigure, rebuild, install and restart. New templates never replace existing
user settings. See the workspace module integration documentation for the exact
load order, allowlist and macOS layout.
