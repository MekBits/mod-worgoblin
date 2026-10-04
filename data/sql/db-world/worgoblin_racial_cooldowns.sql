-- Racial cooldowns as in Cataclysm. The client half is Spell.dbc in
-- data/patch/DBFilesClient; both must say the same.
--
-- A spell's Category puts it on the cooldown of every other spell in that
-- category (Player::AddSpellAndCategoryCooldowns).
--
-- 68992 Darkflight has no category and its own 2-minute cooldown. In
-- Category 44 it would share one with Sprint, Dash, Blink and about a hundred
-- charges, and a worgen rogue, druid or mage would lose those for two minutes
-- after Darkflight, and the other way round.
--
-- 69041 Rocket Barrage and 69070 Rocket Jump share a 2-minute cooldown in
-- Category 1252, which has no other spell.
--
-- The rows come from workflow/spell_dbc.sql, which sorts before this file.

UPDATE `spell_dbc` SET `Category` = 0, `RecoveryTime` = 120000, `CategoryRecoveryTime` = 0
  WHERE `ID` = 68992;

UPDATE `spell_dbc` SET `Category` = 1252, `RecoveryTime` = 120000, `CategoryRecoveryTime` = 120000
  WHERE `ID` IN (69041, 69070);
