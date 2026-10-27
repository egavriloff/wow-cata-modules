-- Optional manual installation/repair in the fully updated TCPP world database.
-- This file owns ONLY custom NPC 900101 and its spawns. Review before execution.
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
    (900101, 'Трансмогрификатор', '', '', 85, 85, 35, 1, 2000, 2000, 1, 7,
     0, 0, '', 'npc_transmogrifier')
ON DUPLICATE KEY UPDATE
    `name` = 'Трансмогрификатор', `femaleName` = '', `subname` = '',
    `minlevel` = 85, `maxlevel` = 85, `faction` = 35, `npcflag` = 1,
    `BaseAttackTime` = 2000, `RangeAttackTime` = 2000, `unit_class` = 1, `type` = 7,
    `unit_flags` = 0, `unit_flags2` = 0,
    `AIName` = '', `ScriptName` = 'npc_transmogrifier';

-- Windle Sparkshine (29261) -> display 26078, confirmed in checked-out TCPP
-- sql/updates/world/4.3.4/2023_09_15_00_world.sql: (29261, 0, 26078, 1, 15595).
-- 29261 is the reference NPC ENTRY, never the display ID. Never alter its rows.
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `Probability`, `VerifiedBuild`)
VALUES (900101, 0, 26078, 1, 0)
ON DUPLICATE KEY UPDATE
    `CreatureDisplayID` = 26078, `Probability` = 1, `VerifiedBuild` = 0;
-- Remove alternate custom-NPC models so the temporary guard cannot be selected.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900101 AND `Idx` <> 0;

-- Existing spawns inherit template appearance, gossip flags and script binding.
-- Preserve their GUIDs, positions, phases, equipment and respawn settings.
-- 2025_03_11_00_world.sql makes flag overrides nullable: zero is an explicit
-- override (npcflag=0 disables gossip), while NULL inherits the template.
-- 2025_03_11_03_world.sql removes dynamicflags from both creature tables.
UPDATE `creature` SET `modelid` = 0, `npcflag` = NULL, `unit_flags` = NULL,
    `unit_flags2` = NULL, `ScriptName` = '' WHERE `id` = 900101;
COMMIT;
