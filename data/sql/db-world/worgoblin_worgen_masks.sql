-- Worgen on the quests and items every other race may take.
--
-- worgoblin.sql adds worgen (race 12, 2048) wherever humans are allowed, but
-- skips the masks it reads as "all races", 2047 among them. 2047 is races
-- 1-11, so worgen are not in it. The goblin update runs first and turns the
-- ten-race mask 1791 into 1791 | 256 = 2047, so those quests are skipped too:
-- the Netherwing chain, To Skettis!, Feedin' Da Goolz and others.
--
-- Every mask that names all ten original races (1791) gets worgen. Masks that
-- leave one of them out, and 0 / -1 (no restriction), are not touched. `|` is
-- idempotent.

UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` | 2048
  WHERE `AllowableRaces` > 0 AND (`AllowableRaces` & 1791) = 1791 AND (`AllowableRaces` & 2048) = 0;

UPDATE `item_template` SET `AllowableRace` = `AllowableRace` | 2048
  WHERE `AllowableRace` > 0 AND (`AllowableRace` & 1791) = 1791 AND (`AllowableRace` & 2048) = 0;
