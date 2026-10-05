-- Start zone quests for goblins and worgen.
--
-- Worgen start in Shadowglen and goblins in the Valley of Trials, but
-- worgoblin.sql gives them the quests of humans and orcs. Quests there that
-- are limited to night elves or trolls stay closed to them, among them the
-- hunter's Taming the Beast chain, without which a worgen hunter never learns
-- Tame Beast.
--
-- Racial priest spells (Starshards, Elune's Grace, Hex of Weakness,
-- Shadowguard) and the night elf druid quests are left alone; the druid
-- quests have worgen copies in supplementary/optional-class-quests.sql.

-- Worgen, from night elves.
UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` | 2048
  WHERE `ID` IN (
    5842,                    -- Welcome!
    5621, 5622,              -- Garments of the Moon, In Favor of Elune (priest)
    6071,                    -- The Hunter's Path (from the Teldrassil hunter trainers)
    6063, 6101, 6102, 6103,  -- Taming the Beast, Training the Beast (hunter)
    6344, 6341, 6342, 6343   -- Nessa Shadowsong ... Return to Nessa
  ) AND `AllowableRaces` <> 0 AND (`AllowableRaces` & 8) <> 0 AND (`AllowableRaces` & 2048) = 0;

-- Goblins, from trolls: the priest and mage tablets. Orcs have neither class.
UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` | 256
  WHERE `ID` IN (
    3085,  -- Hallowed Tablet (priest)
    3086   -- Glyphic Tablet (mage)
  ) AND `AllowableRaces` <> 0 AND (`AllowableRaces` & 128) <> 0 AND (`AllowableRaces` & 256) = 0;
