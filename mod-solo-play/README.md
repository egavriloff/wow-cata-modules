# mod-solo-play

Unified instance scaling for TCPP 4.3.4.15595. **Disabled by default.** These are
initial tuning values, not certified encounter balance. Review
[compatibility](docs/compatibility.md) and [runtime acceptance](docs/runtime-validation.md)
before enabling it on a server used by players.

## Installation

Run the normal workspace `tools/build.ps1` workflow (RelWithDebInfo by default).
Automatic discovery compiles one `Addmod_solo_playScripts` entry and installs
`modules/solo-play.conf` plus `.conf.dist` next to the selected main config.
Existing active config bytes are preserved. No SQL migration, database permission
change, donor module install or manual upstream edit is required. The managed
bridge is required; the module does not compile against an unpatched upstream.
See workspace `docs/module-integration.md` for selection and installation.

Integration uses the delivered `tools/module-integration/patches/bridge.patch`,
metadata and upgrade patches. `.build/prepare-solo-bridge.py` was a one-time
development generator and is not called by any build/integration entry point.
Do not run it as a build step. No ignored helper or previous build output is
required for a fresh managed worktree. The donor extraction is only required
for the donor-integrity audit test, never for compilation.

After disposable-server acceptance, set `SoloPlay.Enable = 1` in the installed
module config. Main-file legacy keys take priority under workspace config rules.
Use `.reload config` or restart. A successful reload publishes a new snapshot;
each instance adopts it only after combat and a stable settle window (5 seconds
by default). Disabling also waits for this safe window. Invalid INI is rejected
by ConfigMgr atomically; invalid numeric values log and use documented defaults.
Unknown profiles disable the module. No database or player stat state persists.

## Context and participants

One context per live map instance contains native difficulty, capacity, roster,
highest player level, config snapshot and revision. Capacity uses
`InstanceMap::GetMaxPlayers`, including 5-player dungeons, 10/25-player raids and
legacy 20/40-player raids. Heroic settings multiply the separate dungeon/raid
settings; actual difficulty templates remain the native baseline.

Only in-world players in the actual map player list with a session count.
GM-mode characters are excluded, as are sessions completing logout. Dead players
and ghosts still count to avoid death-based difficulty reduction. Link-dead
characters count until TCPP removes them from the map; reconnect is counted by
the new map presence. There is no separate spectator API in the inspected TCPP;
spectators must use GM mode. Group membership and characters outside the instance
do not count. Group formation/disbanding do not change the actual roster policy.

Roster/level/config changes are debounced. Any participant combat, tracked
creature combat or `IsEncounterInProgress()` freezes changes, including a
disconnect of the last participant. The settle timer restarts during combat.
At instance entry, scaling starts after the first settle interval; wait before
pulling. Late joins use the existing pull's context until it ends. There is no
independent late-join player buff. Three or more players use native multipliers
after the next safe window. No punitive late-join debuff is applied.

A safe context revision updates every tracked creature's modifiers immediately,
before session/object updates can start the next pull. It does not wait for a
later per-creature tick. Creature spawn/respawn and rebuild hooks still capture
fresh native baselines; no permanent player stat changes need removal.

## Profiles and calculations

Balanced allows 1-2 players; Solo defaults to exactly 1; Duo defaults to exactly 2.
Profile eligibility can be configured but is capped at two. Outside that range,
all multipliers equal 1. Larger groups are not nerfed. Normal and heroic vanilla,
TBC, Wrath and Cataclysm instances share the metadata-driven implementation.
Open world, world bosses outside instances, battlegrounds and arenas are excluded.
Epic dungeon difficulty is excluded because its semantics are not validated.

Let `r = participants / native capacity`. With configurable exponents:

```
H = clamp(r^HealthExponent * profileHealth * contentHealth * rankHealth, minHP, max)
D = clamp(r^DamageExponent * profileDamage * contentDamage * rankDamage, minDamage, max)
P = min(profilePlayerDamage, max / H)
creatureHP = baselineHP * H * P
creatureDamageToPlayer = nativeDamage * D * profileIncomingDamage
playerDamageToCreature = nativeDamage * P
armor = baselineArmor * clamp(r^profileArmorExponent * profileArmor, minArmor, max)
mana = baselineMana * clamp(r^profileManaExponent * profileMana, minMana, max)
NPC healing = nativeHeal * clamp(H * P * profileHealing, minHP, max)
player healing = nativeHeal * profilePlayerHealing
```

`content*` combines dungeon/raid, heroic and optional map/creature override
factors. Boss factors take precedence over elite factors. The HP/player damage
budget keeps `(H*P)/P = H`: a small optional damage bonus cannot independently
stack with the full HP reduction. Player damage defaults to 1 in all profiles,
is capped at 1.25, and healing at 1.5. Creature multipliers never exceed 1.
Armor reduction can additionally improve physical damage; tune it independently.
No player health, mana, spellpower or item stats are modified.

Balanced default trash HP/damage before incoming protection is approximately
0.413/0.569 for 1/5 players, 0.604/0.726 for 2/5, 0.282/0.447 for 1/10 and
0.170/0.324 for 1/25. Boss HP/damage factors default to 1.2/1.1, heroic to
1.1/1.1, and floors prevent arbitrarily low raid values. Solo adds 0.9 HP/damage
factors and 0.8 incoming damage with 1.35 healing. Duo uses 0.95 incoming damage
and 1.10 healing; Balanced uses 0.9 and 1.15 respectively. Difficulty remains
meaningful and must be measured in-game, especially for 25/40-player bosses.

## Ownership and exclusions

Saved unscaled native BASE_PCT modifiers are authoritative. Recalculation always
uses those values and preserves health/mana percentage. Dead creatures stay dead;
positive health is bounded to at least 1. Restore before native entry/level
rebuilds and world removal; recapture after native initialization. Native aura
TOTAL_PCT changes remain effective. No level scaling is performed. Only mana is
scaled; energy, rage and encounter alternate resources are unchanged.

Pets, guardians, temporary summons, vehicles, totems, triggers, charmed creatures
and owned/created creatures are excluded. No player pet empowerment is applied.
Some encounter adds are temporary summons and therefore retain native stats;
this is intentional until encounter-specific testing supports them.
Scripts directly replacing BASE_PCT or max health require per-entry exclusion.

`ExcludedMaps`, `ExcludedCreatures`, `ExcludedSpells` accept comma-separated
positive IDs. `MapOverrides` and `CreatureOverrides` accept quoted
`id:health:damage;id:health:damage` records; factors must be finite in [0.05,4].
Overrides never bypass exclusions or multiplier floors/caps. Exclusions win.
Bad records are logged and ignored. The config template lists every owned key.

Legacy scaling can be disabled entirely. Even when enabled, creatures at least
`Legacy.SkipLevelGap` levels below the highest participant retain native stats.
Player-only healing uses expansion tier caps for the equivalent conservative
legacy eligibility check, since there is no creature in that event.

PvP and player-controlled targets never receive PvE damage amplification.
Healing bonuses are disabled for PvP-flagged players and duels. Self-damage,
environmental damage, reflected self-hits, NODAMAGE redistribution, instant-kill,
percent-health, damage-shield and proc-trigger-damage spells are excluded.
Percent effects and scripted mechanics can still be lethal; this module does not
rewrite encounter logic. Do not run the separate donor scalers alongside it.

## Diagnostics and verification

`.soloplay` uses existing `RBAC_PERM_COMMAND_SERVER_INFO`. It reports the current
instance ID, difficulty, profile, active/pending count, capacity, combat state,
revision, tracked creature count and pending reload; select a creature to show
its multipliers. Console use reports global enable state. No gameplay command
can change multipliers outside native config reload.

Only `.soloplay` without arguments is implemented. `status`, `mapstat` and
`creaturestat` subcommands are unsupported and rejected with usage. Select a
creature and run `.soloplay` for its saved effective multipliers, including NPC
healing. Profile indices are 0=Balanced, 1=Solo, 2=Duo. Permission 725 is linked
to Administrator Commands in the inspected auth SQL; a GM account requires an
explicit permission grant or inherited administrator role. No live RBAC changes
are made by this module.

Healing is scaled once per `HealInfo` before heal absorbs. Native AI adjustment
then runs in DealHeal; effective healing, threat/procs and combat log consume the
same scaled/absorbed record. Damage derived from an already scaled hit and marked
`SPELL_ATTR4_IGNORE_DAMAGE_TAKEN_MODIFIERS` is excluded to avoid a second scale.
Native scripts implementing percent-health or reflection through an unmarked
secondary spell cannot be classified generically: use `ExcludedSpells` and the
runtime checklist. This port does not provide donor feature parity; intentional
omissions are listed in `docs/compatibility.md`.

Run `python -m unittest discover -s modules/mod-solo-play/tests -v` for the
compiled policy/controller regression harness and module discovery checks.
Workspace integration, bridge upgrades and Windows workflow tests remain
separate. Runtime encounter validation is explicitly required; successful
compilation does not establish that every raid can be completed solo.
