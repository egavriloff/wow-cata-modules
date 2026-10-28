# TCPP compatibility assessment

Target inspected locally: `b061f46ef86f84f713ea3956b2318383e0b71278`, client
4.3.4.15595. Donors are the supplied extracted `.build/porting/mod-solocraft`
and `.build/porting/mod-autobalance`, not fetched replacements.

| Area | Actual source evidence and implementation decision |
| --- | --- |
| Player lifecycle | `Scripting/ScriptMgr.h`: `OnLogin(Player*, bool)`, `OnLogout`, `OnMapChanged`, `OnLevelChanged`; donor `OnPlayerLogin`/`OnPlayerMapChanged` names differ. Count `Map::GetPlayers()` on the owning map thread instead of maintaining login counters. |
| Group lifecycle | `GroupScript::OnAddMember`, `OnRemoveMember`, `OnDisband` exist. Group membership outside the instance must not affect scaling. No group hook is needed. |
| Creatures | `CreatureScript` only provides database-bound `GetAI`, not donor `AllCreatureScript`. Replacing encounter AI is inappropriate. Managed bridge adds global lifecycle notifications through `UnitScript`. |
| Initialization | `Creature.cpp::UpdateEntry` selects difficulty templates and updates level-dependent stats; `SelectLevel`, `AddToWorld`, compatibility-mode `Respawn` and `RemoveFromWorld` define baseline boundaries. Restore owned modifiers before entry/level rebuild and removal; apply after world addition and before each creature update. |
| Stats | `Entities/Unit/StatSystem.cpp::Creature::UpdateMaxHealth`, `UpdateArmor`, `UpdateMaxPower` derive from native flat/pct modifiers. Use saved unscaled `BASE_PCT`, not repeatedly multiplying visible max stats. Aura `TOTAL_PCT` remains native. Only mana is adjusted; rage, energy and alternate resources retain encounter semantics. |
| Damage | `Unit.cpp::DealDamage` calls AI adjustments then `OnDamage`, but that legacy hook lacks spell/type and runs after combat logs. New `OnScalingDamage` carries `DamageEffectType` and `SpellInfo` from melee/direct-spell calculations and the two damage periodic handlers, before absorbs and combat logs. The third periodic legacy callback is actually healing and is deliberately not used. Each damage pipeline dispatches once; no DealDamage callback scales damage-sharing again. Armor mitigation remains upstream; armor scales separately. |
| Healing | `Unit.cpp::DealHeal` calls `OnHeal` AFTER `ModifyHealth`. The audited bridge calls `HealInfo::ApplyScaling` at the start of `CalcHealAbsorb`, once per record, updating `_heal` before native absorption. Both HealBySpell and periodic healing call CalcHealAbsorb exactly once. DealHeal keeps native AI adjustment and health modification; periodic logs recover scaled raw heal as remaining+absorbed. Nonidentity adjustments saturate at signed int32 maximum. |
| Map lifecycle | `ScriptMgr.cpp::OnMapUpdate` dispatches only map-ID scripts after object updates. Donor `AllMapScript` is absent. Add before-map-update and destroy notifications; preserve existing instance scripts. |
| Context storage | Donor `CustomData`/`DataMap` absent. Controller owns map state and creature baseline records, removes them on lifecycle notifications. A mutex protects the container across map workers, config reload and diagnostics. No world-thread creature scans. |
| Difficulty | `DataStores/DBCEnums.h`: dungeon normal=0, heroic=1, epic=2; raid 10N=0,25N=1,10H=2,25H=3. `Map::IsHeroic`, `InstanceMap::GetMaxPlayers` and map metadata select difficulty/size; no WotLK-only map ID table. Epic dungeons are unsupported and excluded. |
| Legacy | `DBCStructure.h::MapEntry::Expansion` identifies pre-Cataclysm maps. Context records highest participant level; configurable level gap avoids scaling trivial creatures. |
| Encounters | `InstanceScript::IsEncounterInProgress`, player combat and tracked creature combat freeze roster/config. Dead connected characters count; ghosts do not lower difficulty. Summons, pets, guardians, vehicles, totems, triggers and owned/created creatures are excluded. |
| Config | Native `WorldScript::OnConfigLoad(bool)` and typed getters; workspace descriptors merge module INI before notification. Reload snapshots are deferred per instance until a safe out-of-combat window. |
| Commands | Native `CommandScript::GetCommands()` returns `std::vector<ChatCommand>`, unlike donor command tables. Reuse `RBAC_PERM_COMMAND_SERVER_INFO` for a read-only `.soloplay` diagnostic, no permission SQL needed. |
| Existing scaling | `Creature::_GetHealthMod`, world creature rank rates, template difficulty variants, player aura/stat system and encounter scripts already affect stats. These remain the baseline. No existing instance player-count scaler was found in those pipelines. |

## Audited event paths

* Melee: `Unit::CalculateMeleeDamage` -> native damage bonuses -> legacy melee
  callback -> `OnScalingDamage(DIRECT_DAMAGE)` once -> armor, outcome/crit/block,
  resilience, absorbs -> native log/DealMeleeDamage/DealDamage. No module callback
  in DealDamage, so damage sharing cannot reapply it.
* Direct spells (including native periodic power burn using this calculator):
  `CalculateSpellDamageTaken` -> armor/class mitigation/crit/block -> legacy spell
  callback -> `OnScalingDamage(SPELL_DIRECT_DAMAGE)` once -> absorbs -> log and
  DealSpellDamage/DealDamage. Damage marked IGNORE_DAMAGE_TAKEN_MODIFIERS is
  excluded because TCPP documents it as derived from another hit.
* Periodic damage and periodic health leech: each handler calls the legacy
  periodic callback then `OnScalingDamage(DOT)` once, followed by native periodic
  bonuses, armor/crit/resilience/absorbs and logs/DealDamage. The periodic HEAL
  handler does not call this new damage callback. Native flat adjustments and
  rounding occur at different phases from direct spells; do not assume identical
  final reduction for every effect. Custom scripted health changes bypass these
  hooks and remain unsupported unless accepted per encounter.
* Healing: direct spells/leech/funnel -> HealBySpell -> CalcHealAbsorb; periodic
  healing -> CalcHealAbsorb directly. Both -> HealInfo::ApplyScaling (guarded)
  -> OnBeforeHeal -> native absorb depletion -> DealHeal -> effective healing,
  threat/procs/log. The periodic raw-heal log is updated from HealInfo. The old
  post-absorb addhealth callback was removed. There is no damage-path callback
  on a healing tick, and repeating ApplyScaling cannot stack healing.

Percent-effect filtering uses native effect/aura metadata, not a complete list
of boss scripts. An unmarked spell whose script computes a percent of HP is not
automatically recognizable. Configure ExcludedSpells and validate encounters.

The bridge declarations remain shared infrastructure if SoloPlay is removed;
only empty/default callbacks execute without its registered lifecycle script.
They are not stale module registrations. Removal/readdition fixtures verify the
loader, config descriptors and CMake cache lifecycle alongside the NPC modules.

## Donor scope

AutoBalance supplies the concepts of original-stat ownership, instance context,
combat roster locking, rank/content factors, exclusions and diagnostics. Its
level scaling, reward scaling, bot counts, aura CC edits, gameobject health hooks,
module-script extensions and AC-specific configuration APIs are not portable.
SoloCraft supplies missing-party compensation and class-independent survivability
and healing goals. Its SQL persistence, XP changes, large stat/spellpower buffs and
late-join debuffs are deliberately not ported. Player empowerment is event-based,
so logout/reconnect/death/resurrection require no persisted stat cleanup.

Donor SoloCraft includes a GPLv3 LICENSE and no README in this extraction;
AutoBalance includes a README, source GPLv2-or-later notices, and no standalone
LICENSE. Neither supplied extraction contains a dedicated test suite. Both rely
on AzerothCore headers and hook extensions; `include.sh` is empty. SoloCraft SQL
is unnecessary for the stateless player adaptation. See NOTICE.md for attribution.

## Acceptance limits

Scaling cannot solve mechanics requiring multiple targets, simultaneous switches,
mandatory role checks or scripted instant deaths. Special encounter creatures
must be excluded by entry when native scripts directly replace health or modifier
baselines. Percent-health, instant-kill, damage-shield and proc-trigger-damage
spells are conservatively excluded from event scaling. Environmental events
(no source), self-damage and NODAMAGE events are excluded. In
`Spell.cpp::TargetInfo::PreprocessTarget`, reflection chooses the original caster
as hit target; `DoDamageAndTriggers` uses the original caster as damage source.
Those reflected self-hits are excluded by source==target. Custom scripts that
implement reflection through a separate spell need an explicit spell exclusion.
No level scaling or player pet empowerment is implemented.
