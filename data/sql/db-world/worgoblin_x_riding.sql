-- Riding training and mounts for goblins and worgen.
--
-- The race riding trainers and mount vendors show their gossip option only to
-- their own race, or to players exalted with the city (conditions with an
-- ElseGroup). Goblins and worgen have no race there, so before Outland they
-- could only learn Apprentice Riding with exalted reputation.
--
-- Worgen ride like humans (Randal Hunter and the Stormwind horse breeders)
-- and goblins like orcs (Kildar and Ogunaro Wolfrunner): the same pairing
-- worgoblin.sql uses for quests and items, and Gilneas rode horses.

-- "Is human": Randal Hunter (gossip menu 4018) and the horse breeders (4004).
-- Covers the option, the greeting and the negated "not human" greeting.
UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 2048
  WHERE `SourceTypeOrReferenceId` IN (14, 15) AND `SourceGroup` IN (4004, 4018)
    AND `ConditionTypeOrReference` = 16 AND (`ConditionValue1` & 1791) = 1;

-- "Is orc": Kildar (4020) and Ogunaro Wolfrunner (3161).
UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 256
  WHERE `SourceTypeOrReferenceId` IN (14, 15) AND `SourceGroup` IN (3161, 4020)
    AND `ConditionTypeOrReference` = 16 AND (`ConditionValue1` & 1791) = 2;

-- The race riding trainers greet everyone but their own race with a mask of
-- the other nine original races. Goblins and worgen get that greeting too,
-- except from the trainer of the race they ride like.
UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1`
    | IF(`SourceGroup` = 4020, 0, 256) | IF(`SourceGroup` = 4018, 0, 2048)
  WHERE `SourceTypeOrReferenceId` = 14
    AND `SourceGroup` IN (4014, 4015, 4016, 4018, 4019, 4020, 4021, 4022, 8275, 8553)
    AND `ConditionTypeOrReference` = 16 AND `NegativeCondition` = 0
    AND BIT_COUNT(`ConditionValue1` & 1791) = 9;

-- The riding trainer's letter at level 20 and 40.
UPDATE `mail_level_reward` SET `raceMask` = `raceMask` | 2048 WHERE (`raceMask` & 1791) = 1;
UPDATE `mail_level_reward` SET `raceMask` = `raceMask` | 256 WHERE (`raceMask` & 1791) = 2;
