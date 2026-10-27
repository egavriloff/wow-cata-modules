-- Optional manual installation in the TCPP world database. Never run automatically.
-- Entry 900100 has no matches in the pinned TCPP SQL/source or workspace modules.
-- A live database may contain other custom content: inspect the result below first.
SELECT `entry`, `name`, `ScriptName` FROM `creature_template` WHERE `entry` = 900100;
SELECT * FROM `creature_template_model` WHERE `CreatureID` = 900100;

-- No DELETE, REPLACE or UPDATE: an existing entry is left completely untouched.
START TRANSACTION;
INSERT INTO `creature_template`
    (`entry`, `name`, `femaleName`, `subname`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `BaseAttackTime`, `RangeAttackTime`, `unit_class`, `type`,
     `AIName`, `ScriptName`)
SELECT 900100, 'Телепортатор', '', '', 85, 85, 35, 1, 2000, 2000, 1, 7, '', 'npc_teleport'
WHERE NOT EXISTS (SELECT 1 FROM `creature_template` WHERE `entry` = 900100)
  AND NOT EXISTS (SELECT 1 FROM `creature_template_model` WHERE `CreatureID` = 900100);
SET @npc_teleport_installed = ROW_COUNT();

-- Current TCPP loads display models from this table, not legacy modelid1.
-- Display 19646 is present in TCPP's 2023_09_15_00_world.sql (build 15595).
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `Probability`, `VerifiedBuild`)
SELECT 900100, 0, 19646, 1, 0 WHERE @npc_teleport_installed = 1;
COMMIT;

-- Must show npc_teleport before spawning. A conflicting script/name needs operator review.
SELECT `entry`, `name`, `npcflag`, `ScriptName` FROM `creature_template` WHERE `entry` = 900100;
SELECT * FROM `creature_template_model` WHERE `CreatureID` = 900100;
