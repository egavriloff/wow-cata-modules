# Validation record — 2026-10-28

Target TCPP revision: `b061f46ef86f84f713ea3956b2318383e0b71278`.

Executed automated validation:

* Module suite: catalog/config validation, SHA-256 integrity of every supplied
  donor file, actual C++ controller/policy behavioral harness, native damage and
  pre-heal dispatch placement. The harness uses API doubles, not a running server.
  The initial delivery passed 4 tests and 108 behavioral checks; see final audit below.
* Initial workspace integration: 41 tests passed with no skips, including native
  ConfigMgr compilation/startup/reload, real executable pre-database config error
  handling, CMake build/link/install fixtures and exact bridge v1-v5 upgrades with
  conflicting-edit refusal.
* Windows build workflow: 7 tests passed.
* SQL updater: 43 tests passed; no real migration application was requested.
* Real TCPP RelWithDebInfo configure/build/link/install through `tools/build.ps1`.
  The module remains disabled in its configuration template.

Workspace logs:

```
.build/solo-play-module-tests.log
.build/solo-play-integration-tests.log
.build/solo-play-workflow-tests.log
.build/solo-play-sql-tests.log
.build/solo-play-build.log
.build/solo-play-final-build.log
```

The controller harness covers policy bounds for 5/10/20/25/40 players of native
capacity, 1/2 participant scaling and larger-group fallback, coordinated damage/HP
budget, finite numeric validation and saturation, percentage-preserving health,
disabled startup, GM exclusion, self healing, duels, combat late joins/disconnects,
config reload/disable deferral, baseline reset without accumulation, pet/summon/
vehicle/trigger/charm exclusions, PvP, percent-health and shared damage exclusions,
map destruction/reuse and trivial legacy eligibility. Native compilation checks
real TCPP API signatures; doubles establish controller behavior in isolation.

No real database startup, client session, encounter completion, macOS build or
in-game tuning is claimed. Follow `docs/runtime-validation.md` before enabling.
No commits, pushes, donor changes or submodule pointer updates were performed.

## Final audit and stabilization

Final results: 44 integration tests (no skips), 43 SQL updater tests, 7 workflow
tests, and 4 module tests with 128 behavioral checks passed. The 12 preparation
tests also passed separately after stale-registration recovery was added; they
are included in the final 44 integration tests, not an additional distinct suite.

Confirmed defects fixed:

* Heal scaling ran after native heal absorb and did not update HealInfo/log raw
  healing. Scale the actual HealInfo once before CalcHealAbsorb resolution, keep
  native AI processing, and recover scaled raw periodic log healing from the
  record. Regression executes actual HealInfo constructor/ApplyScaling/AbsorbHeal
  with the controller; real aura-loop/gameplay validation still requires runtime.
* Damage-derived spells marked IGNORE_DAMAGE_TAKEN_MODIFIERS could scale a value
  already scaled in the parent hit. They now bypass module damage scaling.
* Lazy per-creature revision updates could leave old creature multipliers beside
  a new roster/healing context at the next pull. Commit all tracked modifiers in
  the safe map revision transaction, before session/object processing.
* Identity event multipliers could clamp native uint32 values to INT32_MAX.
  Identity now returns the original amount unchanged.
* An absent generated worktree could retain its Git registration after deletion
  of .build output. Preparation now recreates only that missing unlocked detached
  pinned registration, without pruning others or overriding branches/locks.
* Unsupported diagnostic arguments were silently accepted. Only bare .soloplay
  is implemented; status/mapstat/creaturestat now return usage. The selected NPC
  output includes its stored healing multiplier. Permission remains native 725.

The ignored .build/prepare-solo-bridge.py is a one-time development generator,
not a dependency of integration.py, CMake or build.ps1. No relocation into tools
is needed. Delivered patch/metadata inputs recreate an exact fresh managed core;
the fresh-preparation regression copies only those inputs and also simulates
deleted generated worktree output. Historical upgrade preimages remain checked.
New module and bridge files must be included in the future independent modules
and workspace commits; no staging, commit or submodule reference update is done.

The final audit used new .build/core/worktrees/solo-play-audit and
.build/solo-play-audit-build directories, with a complete RelWithDebInfo build.
Real CMake OFF -> build/link -> ON -> build/link retained the NPC modules;
OFF worldserver had neither a SoloPlay loader call nor its lifecycle literal.
ON restored exactly one registration. Fixture tests separately remove and readd
the actual module ID and config to verify cache/descriptor retirement.

Installation is isolated at .build/install/solo-play-audit/RelWithDebInfo. The
installed executable SHA-256 matches the built artifact, and --version reports
the pinned Win64 RelWithDebInfo static core. The complete installed module config
and template retain SoloPlay.Enable=0. The ordinary installed solo-play.conf is
unchanged, also disabled/Balanced; it predates the six optional armor/mana exponent
keys, so their validated 0.15 getter defaults apply. Do not overwrite that active
file automatically. Compare its template when preparing a disposable test server.

Current audit evidence is in .build/solo-play-audit-{configure,build,off-configure,
off-build,on-configure,on-build,install,version,integration-tests,prepare-tests,
module-tests,sql-tests,workflow-tests}.log and solo-play-audit-delivery.json.
The version-only executable check does not connect to databases. Installation
checked the destination for running servers; no server process was stopped.

Runtime unknowns remain: real encounter resets/transformations, arbitrary scripted
percent-health/reflection spells, special adds/friendly NPCs, native aura/proc and
combat-log behavior, GM/spectator administration, multi-instance performance and
Solo/Duo tuning. No donor feature parity or production readiness is claimed.
