-- Optional manual installation/repair in the fully updated TCPP world database.
-- This file owns ONLY custom NPC 900100 and its spawns. Review before execution.
-- Apply while worldserver is stopped, then start the newly built worldserver.
SET NAMES utf8mb4;
START TRANSACTION;

-- All NOT NULL template strings without defaults are supplied explicitly.
-- Neutral faction 35, gossip flag 1, normal interaction flags, script-driven AI.
INSERT INTO `creature_template`
    (`entry`, `name`, `femaleName`, `subname`, `minlevel`, `maxlevel`,
     `faction`, `npcflag`, `BaseAttackTime`, `RangeAttackTime`, `unit_class`, `type`,
     `unit_flags`, `unit_flags2`, `AIName`, `ScriptName`)
VALUES
    (900100, 'Телепортатор', '', '', 85, 85, 35, 1, 2000, 2000, 1, 7,
     0, 0, '', 'npc_teleport')
ON DUPLICATE KEY UPDATE
    `name` = 'Телепортатор', `femaleName` = '', `subname` = '',
    `minlevel` = 85, `maxlevel` = 85, `faction` = 35, `npcflag` = 1,
    `BaseAttackTime` = 2000, `RangeAttackTime` = 2000, `unit_class` = 1, `type` = 7,
    `unit_flags` = 0, `unit_flags2` = 0,
    `AIName` = '', `ScriptName` = 'npc_teleport';

-- Windle Sparkshine (29261) -> display 26078, confirmed in checked-out TCPP
-- sql/updates/world/4.3.4/2023_09_15_00_world.sql: (29261, 0, 26078, 1, 15595).
-- 29261 is the reference NPC ENTRY, never the display ID. Never alter its rows.
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `Probability`, `VerifiedBuild`)
VALUES (900100, 0, 26078, 1, 0)
ON DUPLICATE KEY UPDATE
    `CreatureDisplayID` = 26078, `Probability` = 1, `VerifiedBuild` = 0;
-- Remove alternate custom-NPC models so the temporary guard cannot be selected.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900100 AND `Idx` <> 0;

-- Existing spawns inherit template appearance, gossip flags and script binding.
-- Preserve their GUIDs, positions, phases, equipment and respawn settings.
-- 2025_03_11_00_world.sql makes flag overrides nullable: zero is an explicit
-- override (npcflag=0 disables gossip), while NULL inherits the template.
-- 2025_03_11_03_world.sql removes dynamicflags from both creature tables.
UPDATE `creature` SET `modelid` = 0, `npcflag` = NULL, `unit_flags` = NULL,
    `unit_flags2` = NULL, `ScriptName` = '' WHERE `id` = 900100;
-- Module-owned greeting used by the scripted menu (not a database menu binding).
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`, `BroadcastTextID0`, `lang0`)
VALUES (900100, 'Куда отправимся, $N? Выберите раздел, а затем место назначения.', 'Куда отправимся, $N? Выберите раздел, а затем место назначения.', 1, 0, 0)
ON DUPLICATE KEY UPDATE
    `text0_0` = 'Куда отправимся, $N? Выберите раздел, а затем место назначения.',
    `text0_1` = 'Куда отправимся, $N? Выберите раздел, а затем место назначения.',
    `Probability0` = 1, `BroadcastTextID0` = 0, `lang0` = 0;
COMMIT;

SELECT `entry`, `name`, `faction`, `npcflag`, `AIName`, `ScriptName`
FROM `creature_template` WHERE `entry` = 900100;
SELECT `CreatureID`, `Idx`, `CreatureDisplayID`, `Probability`, `VerifiedBuild`
FROM `creature_template_model` WHERE `CreatureID` = 900100;
SELECT `guid`, `id`, `modelid`, `npcflag`, `unit_flags`, `unit_flags2`, `ScriptName`
FROM `creature` WHERE `id` = 900100;
