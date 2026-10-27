# Runtime diagnosis and acceptance

## Observed evidence

On 2026-10-27 the operator supplied Server.log, DBErrors.log and Auth.log from
the server installation. DBErrors.log line 2325 at inspection contained:

```text
Script named 'npc_teleport' does not have a script name assigned in database.
```

It also contained earlier missing-display-model errors for entry 900100.
Server.log recorded the module integration example's startup message, proving
the composed loader was active. The deployed executable matched the previous
installed build exactly (SHA-256
`1caf9aed2e9c83415a608d0b9b3ef08c03a31a97c40b68f6cd4dc6da40cbe76c`).
This rules out an absent compiled module in that executable. It does not prove
that its database-bound script survived registration at every startup.

The errors have no timestamps in these logs, and no process was running at
inspection. Therefore the missing-binding event is confirmed, but its relation
to the most recent right-click attempt cannot be established. No live database
queries, server starts, deployment or database changes were performed.

## Verified TCPP execution chain

Paths below are relative to `externals/core/`, pinned revision
`b061f46ef86f84f713ea3956b2318383e0b71278`:

1. The managed integration bridge selects `AddWorkspaceScripts` through
   `ScriptMgr::SetScriptLoader`. The generated loader calls `AddScripts` and
   `Addmod_npc_teleportScripts` once each.
2. `src/server/game/World/World.cpp` loads script names before initializing
   scripts. `Globals/ObjectMgr.cpp::LoadScriptNames` unions database bindings,
   including `creature_template.ScriptName` and `creature.ScriptName`.
3. `Scripting/ScriptMgr.cpp::ScriptRegistry::AddScript` looks up a database ID
   for `CreatureScript("npc_teleport")`. If zero, it emits the observed message,
   queues the object for deletion and returns without putting it in the script
   registry. Editing SQL later or reloading a template does not rerun the static
   module loader. Install the binding **before a full process restart**.
4. `Entities/Creature/Creature.cpp::GetScriptId` prioritizes a spawn-level
   `creature.ScriptName`, then the template's script ID.
   `ScriptMgr::GetCreatureAI` resolves this ID and calls the script's `GetAI`.
   The new SQL clears overrides only for custom entry 900100.
5. `Handlers/NPCHandler.cpp::HandleGossipHelloOpcode` calls
   `Player::GetNPCIfCanInteractWith` with `UNIT_NPC_FLAG_GOSSIP`. It checks
   world/flight/death state, NPC existence/aliveness/flags, charm, reaction and
   range. It then clears menus and calls `unit->AI()->GossipHello(player)`.
6. The actual virtual signature is `bool CreatureAI::GossipHello(Player*)`.
   The module's `PassiveAI` override populates the full faction-filtered menu
   and returns true, consuming the default DB gossip path. There is no current
   `CreatureScript::OnGossipHello` override.
7. `ScriptedGossip.cpp` sets the menu ID and adds server-side items.
   `Entities/Creature/GossipDef.cpp::PlayerMenu::SendGossipMenu` records the
   interaction source, copies all items and sends `GossipMessage::Write()` from
   `Server/Packets/NPCPackets.cpp`. This is the core's Cataclysm packet writer.
8. `Handlers/MiscHandler.cpp::HandleGossipSelectOptionOpcode` checks list-item
   existence, source GUID and NPC interaction, then invokes
   `bool CreatureAI::GossipSelect(Player*, uint32 menuId, uint32 gossipListId)`.
   Its third argument is a **list ID**, not the donor's raw action index. The
   module reads the action from the selected server-side item before validating
   bounds, menu, faction and payment state.

The signatures and packet path were already correct in the port; they were not
replaced with a stub. No proof was found of a second C++ gossip defect. The
confirmed registration rejection and the old SQL's inability to repair an
existing installation are addressed by the repaired SQL, required restart and
explicit registration diagnostics. If the latest failure had another cause,
the diagnostic sequence below distinguishes it.

`Handlers/QueryHandler.cpp::HandleNpcTextQueryOpcode` sends `Greetings $N` when
the requested npc_text is missing. `DEFAULT_GOSSIP_MESSAGE` is also the core's
default text ID. Consequently missing donor npc_text rows alone are not evidence
that the menu cannot open, and no speculative gossip SQL dependency was added.

## Logging and in-game reproduction

The new build emits two INFO messages under `server.loading` during normal
startup: loader invocation with the database script ID, then registration
completion if nonzero. A zero ID emits an actionable ERROR. Existing server
INFO logging should show them without enabling interaction traces.

Interaction logging uses `modules.npc_teleport` at DEBUG and is quiet by
default. For one diagnostic session, manually add this to worldserver.conf:

```ini
Logger.modules.npc_teleport=2,Server
```

Here `Server` must name an existing file appender with level <= 2. The supplied
configuration already has `Appender.Server=2,2,0,Server.log,w`, so it accepts
DEBUG. Its Console appender has level 3 and will not show DEBUG. If hello never
arrives, also temporarily enable:

```ini
Logger.network=2,Server
```

The core then logs rejected NPC interactions. Remove the debug logger entries
after diagnosis and restart; no module config subsystem is introduced.

1. Stop worldserver. Manually apply `sql/world/install.sql` to the world database
   used by this process. Verify its final SELECTs: entry 900100, script
   `npc_teleport`, NPC flag 1, model index 0/display **26078**, probability 1;
   spawn modelid 0, npcflag/unit_flags/unit_flags2 NULL and ScriptName empty.
2. Deploy the newly installed executable and preserve your server config/data.
   Do not leave an older process running. Start worldserver and retain the whole
   new startup log, preferably with timestamps enabled in your normal logging.
3. Expect a nonzero `Loader called: npc_teleport database scriptId=...` followed
   by `npc_teleport registration completed`. Zero means the process loaded a
   database without the expected exact script name; check its world DB selection
   locally without sharing credentials. An absent loader message means inspect
   the deployed executable and logger/appender configuration first.
4. With DEBUG enabled, expect `GetAI` for entry 900100. If registration succeeds
   but this is absent, inspect spawn binding/AI overrides and whether that spawn
   was loaded. The repaired SQL sets empty AIName and spawn ScriptName.
5. Approach the visible, living NPC with a living, friendly character within
   melee interaction range; dismount from vehicles and stop flight. Right-click.
   Expect `GossipHello`, then `GossipMenu sent` with a nonempty item count. If no
   Hello appears, inspect `network` interaction rejection messages, phase,
   actual spawned entry/flags, charm and reaction.
6. Select a submenu and Back. Expect `GossipSelect` with the displayed menu/list
   IDs and another nonempty menu. Check both factions' city visibility.
7. Test one normal teleport, combat rejection and a map with unavailable data.
   Confirm rejected preconditions never charge money. Shipped destinations are
   all free. Verify the wizard's hat/robe appearance in the Cataclysm client;
   no in-game appearance or equipment check was performed here.

## UTF-8 investigation

All relevant source/SQL files strictly decode as UTF-8. All 206 compiled module
strings were already present as expected UTF-8 bytes in the deployed pre-fix
executable. All three supplied logs also decode strictly, and none contains the
reported conversion-error text. The NPC's valid database name is not evidence
of a module encoding defect, nor does it explain arbitrary console messages.

The exact error text originates in `src/common/Utilities/Util.cpp`'s
`Utf8toWStr(char const*, size_t, wchar_t*, size_t&)` exception handler. It can
reflect a conversion or output-buffer exception. On Windows,
`vutf8printf` uses that overload for console output, including
`Logging/AppenderConsole.cpp::AppenderConsole::_Write`; CLI output also uses
the overload via `CommandLine/CliRunnable.cpp::utf8print`.
The gossip packet writer copies strings directly and does not call this WStr
conversion. Console conversion failures may therefore not appear as the same
error text in Server.log/DBErrors.log: file appenders preserve the original
message. No observed failing input identifies this module or unrelated data
as the source. Global encodings and Russian text were not changed.

The module target now explicitly uses MSVC `/utf-8` for both source and
execution character sets, independently of the host ANSI code page. A compiled
probe emits all destination labels and four messages as binary output; strict
UTF-8 decoding and exact comparison with source validates the result.

To reproduce the unresolved console failure:

1. Use the rebuilt executable and a fresh complete startup/file-log capture.
2. Capture console output at the first error and note the exact command/action,
   timestamp and surrounding lines. First test startup with no client actions;
   then login, right-click, submenu navigation, confirmation and combat failure
   separately. Note whether console commands themselves trigger it.
3. Correlate the console line with the original file-appender message and the
   module's DEBUG markers. Preserve raw file bytes for strict UTF-8 analysis.
4. If all file messages decode correctly but console conversion still fails,
   the exact input/length at the Util.cpp conversion call requires runtime
   debugger investigation. This task did not attach a debugger or alter core.

## Validation limits

Thirteen module tests passed after the SQL schema correction. The previous
build validation passed 25 integration tests; the C++ harness
executes 28 scenarios. The real RelWithDebInfo build/configure/link/install
passed via tools/build.ps1. Installation SQL is **statically** checked against
TCPP table definitions, mandatory fields and reference model data; it was not
executed on a fresh or existing database. MySQL execution and post-repair
startup, actual packets, client appearance and in-game navigation/teleportation
remain the operator's manual acceptance steps.

## SQL schema correction after error 1054

The first installer test used `sql/base/dev/world_database.sql` alone for
creature tables. That historical base dump still contains `dynamicflags` and
non-nullable spawn flags. It does not describe the updated world database.
This was a validation defect in this module's tests.

The original INSERT/upsert into `creature_template` referenced `dynamicflags`
in its column list and duplicate-key assignments. The later UPDATE of `creature`
also referenced it. Repository migration
`sql/updates/world/4.3.4/2025_03_11_03_world.sql` drops the column from **both**
tables. All three references are removed; no replacement dynamic-flags column
is invented because current TCPP no longer stores it there.

Migration `2025_03_11_00_world.sql` makes `creature.npcflag` and `unit_flags`
nullable and adds nullable `unit_flags2`. `2025_06_14_00_world.sql` changes the
npcflag type while preserving nullability. `ObjectMgr::ChooseCreatureFlags`
selects a spawn override when its optional value exists, including zero.
The installer now uses NULL for all three spawn flags to inherit template
values. The previous `npcflag=0` repair could itself disable interaction; this
is an additional confirmed source-level cause of missing gossip on repaired
spawns, independent of the startup registration failure discussed above.

The tests now reconstruct relevant CREATE/DROP/ALTER DDL from the checked-out
base and ordered current world migrations. They check every referenced table
and column, including WHERE predicates and final diagnostic SELECTs. They also
confirm the removal of `gossip_menu_id`, the `exp` rename to
`HealthScalingExpansion`, and the exact five-column model schema. The installer
does not reference those obsolete fields or DisplayScale. NPC 900100, script
`npc_teleport`, template gossip flag 1 and display 26078 remain unchanged.
This is offline structural validation, not execution against MySQL. Apply the
corrected installer manually with the server stopped, then fully restart it.
