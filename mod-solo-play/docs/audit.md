# Pre-commit audit — 2026-10-28

Verdict: ready for review/commit and controlled testing on a disposable server,
with scaling still disabled by default. Not production-ready; no real encounter
completion, client session or live database validation was performed.

## Generator and reproducibility

`.build/prepare-solo-bridge.py` is ignored and untracked. It was a one-time
development generator for the delivered patch inputs. No build entry point calls
it; the original integration.py change only added v5 upgrade recognition. The
helper itself is not idempotent and must not be rerun as a preparation step.
It reads upstream and writes patch assets, not upstream files. It is not moved
into tools because no maintained build dependency needs its implementation.

The tracked infrastructure plus new deliverable patch/metadata files suffice.
A regression copies only patch inputs, creates a fresh exact pinned managed
worktree, repeats preparation, deletes that disposable generated worktree and
recreates it. Missing unlocked detached pinned registrations are recovered
without global pruning. Existing directories, locked registrations and other
branches/revisions are refused. No absolute developer path is needed by module
integration; generated build metadata naturally records its selected locations.

Deleting .build removes optional donor audit inputs and build products, but does
not remove compilation inputs. Reinitialize the normal submodules/dependencies
and run prepare/configure/build again. Donor-integrity testing separately needs
the supplied extraction. The original externals/core remained clean.

All new module files and bridge-v5/v6 metadata/upgrade inputs are still uncommitted
deliverables. They must be included when the user commits the modules repository
and workspace changes. This audit does not stage, commit or change Git links.

## Confirmed defects and fixes

1. Healing ran after absorbs and outside HealInfo, allowing incorrect shield
   consumption and inconsistent logs. ApplyScaling updates HealInfo once before
   CalcHealAbsorb; periodic logs use scaled remaining+absorbed raw healing.
2. Damage derived from another hit could compound a parent scale. Spells marked
   IGNORE_DAMAGE_TAKEN_MODIFIERS now bypass the damage callback.
3. A context revision could publish new healing/roster settings while an NPC
   retained old stats until its next tick. All tracked modifiers now update in
   the same safe map transaction before a new pull can begin.
4. Identity event scaling unnecessarily saturated large native uint32 values.
   Identity returns the untouched amount. Nonidentity adjustments remain bounded.
5. Deleted generated worktrees could leave stale Git metadata blocking recreation.
   Recovery is restricted to the exact missing unlocked detached pinned target.
6. Diagnostic arguments were silently ignored. Bare .soloplay is the supported
   command; unsupported subcommands are rejected. NPC diagnostics include healing.
   A lifecycle update also checks FindMap before dereferencing a detached object.

## Scaling and combat scope

One map context owns roster/capacity/difficulty/config revision. Dead connected
players count; GM-mode players, completed logouts and out-of-map characters do
not. Group membership is not an independent counter. Native instance metadata
provides dungeon/raid size and heroic mode, including legacy and 10/25 variants.
Safe config/roster transitions wait for a stable idle window; combat freezes them.
Creature baselines are saved BASE_PCT modifiers, restored on level/entry rebuild
and removal. Recalculation preserves HP/resource percentage and dead state.

Player bonuses only modify permitted events; no player stat or persistence write
exists. No XP/reward, level, bot, CC-duration or donor SQL behavior is ported.
Pets/guardians/summons/totems/vehicles/triggers/owned/charmed creatures are excluded.
Player-controlled combat cannot receive damage amplification. Open world, BG and
arena maps are excluded. Native unmarked script mechanics cannot be inferred
from spell metadata; use explicit exclusions and encounter acceptance.

See compatibility.md for each exact callback path and its phase. Melee, direct
spells and two periodic damage handlers each dispatch once, before absorbs/logs.
Their native mitigation/flat-adjustment phases differ, so final numerical ratios
are not guaranteed identical across effects. HealInfo scaling is guarded against
repeat application; real aura/proc/log behavior remains a runtime acceptance task.

## Defaults and commands actually verified

The ordinary installed solo-play.conf remains Enable=0 and Balanced. It omits
six later armor/mana exponent keys and uses their 0.15 getter defaults. It was
not rewritten. The isolated audit install includes every current template key,
also disabled/Balanced. Default settle=5000ms; level gap=10; health/damage
exponents=0.55/0.35; floors HP/damage/armor/mana=0.15/0.30/0.60/0.50; max=1.
Boss HP/damage=1.2/1.1, heroic=1.1/1.1, dungeon/raid/elite factors=1.
Balanced allows 1-2 with incoming=0.9/healing=1.15; Solo defaults to 1 with
HP/damage=0.9, incoming=0.8/healing=1.35; Duo defaults to 2 with
incoming=0.95/healing=1.10. Player damage defaults to 1. Formulas and full
validation bounds are in README and the template.

Only .soloplay without arguments is implemented, reporting stored context and
selected-NPC multipliers. No status/mapstat/creaturestat subcommands exist.
RBAC permission 725 is inherited from Administrator Commands in the inspected
auth SQL; ordinary GM access requires an operator grant. Runtime command
execution and effective account grants were not tested against a real database.

## Validation and delivery

* 44 integration tests, including fresh/deleted preparation, v1-v6 upgrades and
  exact SoloPlay removal/readdition fixtures with NPC registrations: passed.
* 43 SQL updater tests, 7 workflow tests: passed, no live SQL executed.
* 4 module tests, 128 behavioral checks: passed. They execute the controller and
  actual HealInfo methods using native API doubles, not the complete live engine.
* Fresh audit managed core and empty audit build: full RelWithDebInfo compilation
  and linking passed. Real SoloPlay OFF/ON reconfiguration and relinking passed.
  OFF removed its loader entry and lifecycle literal from the actual executable;
  ON restored exactly one registration. Shared generic hooks intentionally remain.
* Isolated install passed. Installed worldserver SHA-256 equals its built source
  artifact; --version reports the pinned Win64 RelWithDebInfo static core.
  Delivery data and hashes: .build/solo-play-audit-delivery.json. No DB startup.
* Running-server path protection is covered by workflow tests and was checked
  before isolated installation. No processes were stopped. Main install untouched.

Changed audit files are the controller/policy, its test harness and four module
docs plus this report; integration.py, bridge patch/metadata/upgrade assets,
integration/CMake/preparation tests, and workspace module-integration docs.
Donors, upstream, unrelated module sources, AuctionBot stash and databases were
not modified. Local audit build products/logs remain under .build.

## Remaining acceptance risks

Mandatory multi-character mechanics, scripted instant deaths/percent-health
effects, custom reflection/derived spells without native flags, special or
friendly encounter NPCs that overwrite stats, native absorbs/procs/AI, actual
GM/spectator administration, multi-instance performance and profile tuning all
require runtime testing. Excluded summons can retain lethal native damage even
when their boss is scaled. No generic source audit proves every encounter safe.

Use the disposable-server checklist in runtime-validation.md: baseline with
Enable=0; then solo/duo and 3-player fallback; normal/heroic 10/25 raids and legacy;
join/leave/disconnect/death/reset/reload boundaries; pets/PvP/reflection/percent
effects; direct/periodic healing with partial/full absorbs and overheal; and
separate simultaneous instances. Record diagnostics, HP, damage/healing/log
values and kill times before changing defaults or enabling elsewhere.
