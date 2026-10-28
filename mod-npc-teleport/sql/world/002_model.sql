-- Persist the crystal appearance of custom NPC 900100.
-- Verified by read-only tcpp_world query on 2026-10-28:
-- creature guid 396674, id 900100, modelid 11490 (spawn display override).
START TRANSACTION;

-- A single index-0 model prevents random selection of legacy variants.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 900100;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `Probability`, `VerifiedBuild`)
VALUES (900100, 0, 11490, 1, 0);

-- Inherit the template model and Gossip flags. Zero npcflag disables Gossip.
-- Preserve all other spawn fields, including position, script and unit flags.
UPDATE `creature` SET `modelid` = 0, `npcflag` = NULL WHERE `id` = 900100;
COMMIT;
