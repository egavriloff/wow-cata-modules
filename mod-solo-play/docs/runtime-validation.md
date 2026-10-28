# Disposable-server runtime acceptance

Do not enable on a production realm before completing this checklist. Record
map ID, instance ID, native difficulty, roster, profile, selected-creature
`.soloplay` output, original/scaled HP, hit/heal amounts and time to kill.

1. Start with Enable=0; verify native stats and module config diagnostics. Enable
   Balanced, enter a normal Cataclysm dungeon alone and wait the settle interval.
   Check trash/boss health, mana, armor effects, melee, direct spell, DoT and heals.
2. Repeat with Solo (one player), Duo (two players), and three players (native).
   Compare 10N,25N,10H,25H raids; test vanilla, TBC, Wrath and Cata content.
   Measure meaningful boss kill times and tune dungeon/raid/boss floors separately.
3. At 50% creature health add/remove a player, disconnect/reconnect, kill/resurrect
   a participant, disband/reform the group and reload config. Confirm no HP jump
   or damage context change during the pull. After evade/wipe and settle, verify
   one context revision and percentage-preserving changes, not a full heal.
4. Kill/respawn and unload/reload grids; repeat native UpdateEntry transformations
   and encounter resets. Confirm no cumulative scaling and correct loot eligibility.
5. Inspect pets, guardians, summons, totems, vehicles, triggers and scripted adds.
   They retain native values. Exclude encounter entries whose scripts overwrite
   max health/BASE_PCT; retest those encounters before declaring them supported.
6. Confirm no changes in open world, outdoor bosses, arenas or battlegrounds.
   Duel in an instance and attack controlled pets/charmed creatures; no PvE damage
   bonus. Compare reflection, environmental, percent-health, instant-kill,
   damage-sharing and damage-shield cases against native behavior.
7. Test legacy disabled and overleveled legacy characters; then on-level legacy
   solo/duo. Check player self healing, player-to-player and NPC-to-NPC healing.
8. Reload invalid numeric options, NaN, infinity, malformed overrides, unknown
   profiles and invalid INI. Validate fallbacks/logging, disabled unknown profile,
   atomic INI failure and deferred successful reload. Disable during a pull;
   restoration must occur after combat and settle, without permanent player buffs.
9. Exercise simultaneous separate instances, instance destruction/reuse and
   repeated logout/map changes. Confirm no cross-instance counts or retained
   creature records. Start installed server from outside the workspace using an
   absolute main-config path.

Encounter mechanics requiring simultaneous participants remain outside scope.
Document tuning and any unsupported encounter entries before player deployment.

## Audit follow-up cases

Use `.soloplay` without subcommands; select the NPC for its stored multipliers.
`status`, `mapstat`, `creaturestat` are not implemented and must return usage.
Confirm permission 725 on the test account; the inspected auth baseline grants
it through Administrator Commands, not automatically to every GM account.

With Enable=0, compare melee/direct spells/DoTs/heals, absorb depletion and combat
logs against an otherwise native build. With scaling enabled only on a disposable
realm, cast a 1000 base heal at Balanced 1.15 healing: raw heal should be 1150.
With a 900 heal absorb it should consume 900 and leave 250 before overheal.
With a full absorb of at least 1150 it should heal zero and absorb 1150. Compare
direct and periodic logs, actual health gain, threat and triggered healing procs.
Check NPC healing reduction against an absorb shield as well.

Test a damage-derived secondary spell carrying IGNORE_DAMAGE_TAKEN_MODIFIERS;
it must not receive a second multiplier. Compare marked and custom unmarked
reflection, shields and scripted percent-health effects; exclude uncertain spell
IDs. Native flat reductions/rounding may differ between melee, spell and DoT
phases, so record observed values rather than assuming one exact final ratio.

Finally change 1->2 players or reload while idle, then pull immediately after the
settle boundary. All already tracked creatures must use the committed revision
before their own next update. Repeat on wipe/reset, death/respawn and with a dead
connected participant. No player stats or saved character data should change.
