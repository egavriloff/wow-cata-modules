# TCPP transmogrification investigation

Baseline: TCPP `b061f46ef86f84f713ea3956b2318383e0b71278`, inspected locally
on 2026-10-27. Paths below are relative to `externals/core/`.

| Question | Evidence and choice |
| --- | --- |
| Native transmog? | `src/server/game/Handlers/ItemHandler.cpp::WorldSession::HandleTransmogrifyItems` validates native requests and uses `SetEnchantment(TRANSMOGRIFY_ENCHANTMENT_SLOT, entry, 0, 0)`. `Item::CanBeTransmogrified`, `CanTransmogrify` and `CanTransmogrifyItemWithItem` implement native eligibility. |
| Representation? | `Entities/Item/ItemDefines.h` defines enchantment slot 9. `Item.h::GetVisibleEntry()` returns that appearance entry when nonzero. Pandaria dynamic modifiers and modifier masks are not required. |
| Observer equipment? | `Entities/Player/Player.cpp::SetVisibleItemSlot` writes `PLAYER_VISIBLE_ITEM_1_ENTRYID` from `GetVisibleEntry()`. Equip paths call the same function. The module invokes it immediately after apply/reset. |
| Owner inventory? | Native `SetEnchantment` writes item update fields and marks `ITEM_CHANGED`; reset uses `ClearEnchantment`. This follows the native handler's item update path. Client rendering/tooltips still need live verification. |
| Persistence? | `Entities/Item/Item.cpp::SaveToDB` serializes all 15 enchantment slots (ID/duration/charges) to `item_instance.enchantments`; `LoadFromDB` reloads the array. `sql/base/characters_database.sql` defines `guid`/`owner_guid` as unsigned INT and `enchantments` as text. Persistence across relog/restart is supported by this code path, not yet observed in a running server. |
| Native reuse? | Reuse item appearance storage, update and ownership/refund/trade APIs. The custom menu retains donor template eligibility and preset gameplay instead of issuing native packets or duplicating their backend. |
| Core changes? | None. Existing static workspace loader bridge in the managed worktree is sufficient. No change to upstream core, reference repositories, native updater or module framework is required. |
| Configuration? | `src/common/Configuration/Config.*` reads the main INI file and exposes GetBool/Int/Float/StringDefault. `WorldScript::OnConfigLoad(bool)` is the initial/reload notification. Use a documented main-config fragment; do not replace the singleton with a module file. A future shared ModuleConfig could merge read-only defaults and overrides into an independent namespaced snapshot, but no new framework is implemented here. |
| Gossip? | `Handlers/MiscHandler.cpp` invokes `CreatureAI::GossipSelect/Code` with menu/list IDs. The module uses PassiveAI, ScriptedGossip helpers, and validates the server menu entry, source NPC, menu ID, coded status and interaction range before dispatch. Native handler does not debit scripted Gossip BoxMoney; this module validates and debits explicitly. |

## Persistence and data safety

There is one appearance authority: native `item_instance.enchantments`.
No `custom_transmogrification` mirror, no direct item-instance SQL and no SkyFire
`item_instance_transmog` table are introduced. Existing native appearances are
never reset on login or installation. Only user-selected apply/remove operations
change appearance. SkyFire item records cannot be migrated by copying modifier
values into TCPP tables; cross-expansion character migration is outside scope.

`custom_transmogrification_sets` retains unsigned 32-bit Owner and tiny unsigned
PresetID, with composite primary key; rows are always scoped to the current
character. The migration creates the table if absent, without deleting data.
An existing table must be checked for the documented schema/charset before use;
CREATE IF NOT EXISTS is not schema repair. Character-deletion orphan cleanup is
not implemented, consistent with the supplied donor; do not reuse GUIDs while
retaining old preset rows.

Appearance apply/removal uses `SaveInventoryAndGoldToDB` in a native character
transaction, grouping item state, token inventory and money. Saving a preset
escapes its UTF-8 name, appends INSERT and the native gold/inventory save to the
same transaction, and executes synchronously to avoid a following menu SELECT
racing an asynchronous write. Source and target ownership are verified, never
reassigned. Trade items and token entries matching either referenced item are
rejected. Refund/group-trade cleanup follows native TCPP helpers and their own
database behavior, as in the native opcode handler.

TCPP's `DirectCommitTransaction` returns void and logs database errors; its API
does not expose success to the caller. Native save routines can clear dirty
queues before transaction completion. Thus transaction atomicity in the database
does not guarantee in-memory rollback or durable acknowledgment on a database
failure. The UI only says a save was submitted. Treat database errors as requiring
operator attention; recovery/rollback behavior must be accepted on a disposable
server. No stronger guarantee or independent SQL execution engine is claimed.

## World schema

Template 900101 has gossip flag 1, faction 35 and `npc_transmogrifier` binding.
`creature_template_model` is required. Display 26078 is taken from the checked-out
`sql/updates/world/4.3.4/2023_09_15_00_world.sql` row for Windle Sparkshine
(entry 29261). The display ID is not an NPC entry. No native transmogrifier flag
is granted. Fully updated schema is required: flag overrides became nullable in
`2025_03_11_00_world.sql`; dynamicflags was removed by
`2025_03_11_03_world.sql`. Existing module-owned spawns inherit template settings.
The migration never creates a spawn. `ObjectMgr` script-name loading resolves the
template binding to the registered CreatureScript at server startup; an installed
binary symbol alone does not prove a live database binding.
