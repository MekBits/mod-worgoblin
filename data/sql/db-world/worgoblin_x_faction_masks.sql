-- Goblins and worgen in the race masks that mean "the whole faction".
--
-- 690 is every original Horde race and 1101 every original Alliance race.
-- worgoblin.sql adds goblins and worgen to quests and items, but not to
-- spell_area, conditions or mail_level_reward. Without this, goblins and
-- worgen miss the Battle for the Undercity phases, the quest invisibility
-- detection in Icecrown and the Storm Peaks, a phase of the death knight
-- chain, Honest Max's gossip, the Injured Rainspeaker Oracle and the flying
-- training mail.
--
-- A mask that contains all original races of a faction gets that faction's
-- new race: goblin (256) for 690, worgen (2048) for 1101. Other masks, and 0
-- (no restriction), are not touched. `|` is idempotent.

UPDATE `spell_area` SET `racemask` = `racemask` | 256
  WHERE (`racemask` & 690) = 690 AND (`racemask` & 256) = 0;

UPDATE `spell_area` SET `racemask` = `racemask` | 2048
  WHERE (`racemask` & 1101) = 1101 AND (`racemask` & 2048) = 0;

-- CONDITION_RACE
UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 256
  WHERE `ConditionTypeOrReference` = 16 AND (`ConditionValue1` & 690) = 690 AND (`ConditionValue1` & 256) = 0;

UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 2048
  WHERE `ConditionTypeOrReference` = 16 AND (`ConditionValue1` & 1101) = 1101 AND (`ConditionValue1` & 2048) = 0;

UPDATE `mail_level_reward` SET `raceMask` = `raceMask` | 256
  WHERE (`raceMask` & 690) = 690 AND (`raceMask` & 256) = 0;

UPDATE `mail_level_reward` SET `raceMask` = `raceMask` | 2048
  WHERE (`raceMask` & 1101) = 1101 AND (`raceMask` & 2048) = 0;
