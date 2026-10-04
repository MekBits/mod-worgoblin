-- "A Special Surprise" for worgen and goblin death knights.
--
-- supplementary/dk-quests.sql adds Lord Harford (49355, quest 28649) and Gally
-- Lumpstain (49356, quest 28650) with ScriptName npc_a_special_surprise.
-- AzerothCore has replaced that C++ script with SmartAI on the original
-- prisoners, so the two kneel without ever speaking and the quests cannot be
-- completed. They get the same SmartAI as Ellen Stanbridge (29061): the
-- shared action list 2907400 plays their creature_text groups 0-9.
--
-- Nothing happens unless the supplementary file has been applied: every
-- statement is limited to the creatures (and quests) that exist.

UPDATE `creature_template` SET `AIName` = 'SmartAI', `ScriptName` = '', `RegenHealth` = 0,
    `unit_flags` = `unit_flags` | 768,      -- IMMUNE_TO_PC | IMMUNE_TO_NPC until the speech ends
    `flags_extra` = `flags_extra` & ~8192   -- not CANNOT_ENTER_COMBAT, so the player can kill them
  WHERE `entry` IN (49355, 49356);

UPDATE `creature` SET `curhealth` = 3 WHERE `id` IN (49355, 49356);

UPDATE `creature_text` SET `Emote` = 1 WHERE `CreatureID` IN (49355, 49356) AND `GroupID` IN (0, 2, 3, 4, 5, 6, 7, 8);
UPDATE `creature_text` SET `Emote` = 25 WHERE `CreatureID` IN (49355, 49356) AND `GroupID` = 1;

DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (49355, 49356);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`)
SELECT ct.`entry`, 0, s.`id`, s.`link`, s.`event_type`, s.`phase`, 100, s.`flags`, s.`p1`, s.`p2`, s.`p3`, s.`p4`, s.`p5`, 0, s.`action_type`, s.`a1`, s.`a2`, 0, 0, 0, 0, s.`target_type`, 0, 0, 0, 0, 0, 0, 0, 0, CONCAT(ct.`name`, s.`comment`)
FROM `creature_template` ct
JOIN (
  SELECT 0 AS `id`, 1 AS `link`, 11 AS `event_type`, 0 AS `phase`, 0 AS `flags`, 0 AS `p1`, 0 AS `p2`, 0 AS `p3`, 0 AS `p4`, 0 AS `p5`, 18 AS `action_type`, 256 AS `a1`, 0 AS `a2`, 1 AS `target_type`, ' - On Respawn - Set Flags Immune To Players' AS `comment`
  UNION ALL SELECT 1, 2, 61, 0, 0, 0, 0, 0, 0, 0, 90, 8, 0, 1, ' - On Respawn - Set Flag Standstate Kneel'
  UNION ALL SELECT 2, 0, 61, 0, 0, 0, 0, 0, 0, 0, 22, 1, 0, 1, ' - On Respawn - Set Event Phase 1'
  UNION ALL SELECT 3, 4, 10, 0, 257, 1, 3, 0, 0, 1, 64, 25, 0, 7, ' - On Out of Combat LoS - Store Targetlist (No Repeat)'
  UNION ALL SELECT 4, 5, 61, 0, 0, 0, 0, 0, 0, 0, 22, 2, 0, 1, ' - On Out of Combat LoS - Set Event Phase 2 (No Repeat)'
  UNION ALL SELECT 5, 0, 61, 0, 0, 0, 0, 0, 0, 0, 80, 2907400, 2, 1, ' - On Out of Combat LoS - Run Script (No Repeat)'
  UNION ALL SELECT 6, 0, 1, 1, 0, 30000, 60000, 30000, 60000, 0, 5, 18, 0, 1, ' - Out of Combat - Play Emote 18 (Phase 1)'
) s
WHERE ct.`entry` IN (49355, 49356);

-- Event 3 (SourceGroup 4) only for a player with the quest incomplete.
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 22 AND `SourceGroup` = 4 AND `SourceEntry` IN (49355, 49356);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorType`, `ErrorTextId`, `ScriptName`, `Comment`)
SELECT 22, 4, c.`entry`, 0, 0, 47, 0, c.`quest`, 8, 0, 0, 0, 0, '', CONCAT('The event will only occur when the player has the "A Special Surprise" (', c.`race`, ') quest incomplete.')
FROM (SELECT 49355 AS `entry`, 28649 AS `quest`, 'Worgen' AS `race` UNION ALL SELECT 49356, 28650, 'Goblin') c
JOIN `creature_template` ct ON ct.`entry` = c.`entry`
JOIN `quest_template` q ON q.`ID` = c.`quest`;
