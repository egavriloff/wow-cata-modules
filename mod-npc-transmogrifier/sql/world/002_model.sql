-- Persist the void appearance of custom NPC 900101 (CreatureDisplayID 17704).
-- Also confirmed by read-only tcpp_world query on 2026-10-28:
-- creature guid 396675, id 900101, modelid 17704 (spawn display override).
START TRANSACTION;

-- A single index-0 model prevents random selection of legacy variants.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900101;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `Probability`, `VerifiedBuild`)
VALUES (900101, 0, 17704, 1, 0);

-- Inherit the template model and Gossip flags. Zero npcflag disables Gossip.
-- Preserve all other spawn fields, including position, script and unit flags.
UPDATE `creature` SET `modelid` = 0, `npcflag` = NULL WHERE `id` = 900101;
COMMIT;
