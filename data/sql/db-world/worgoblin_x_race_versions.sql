-- Content with one version per race: worgen take the human one, goblins the
-- orc one, as in worgoblin.sql and worgoblin_x_riding.sql.
--
-- 17 0 24105 / 24104   Hallow's End, "Honoring a Hero" (8149, 8150): the
--                      tribute's script (spell_scripts 24194, 24195) casts
--                      every race's tribute spell and a race condition lets
--                      one through. Without a match the quest cannot be
--                      completed.
-- 23 33307 46752/46758 Argent Tournament, Stormwind quartermaster: the Swift
--                      Gray Steed for humans, and the copy for everyone else
--                      that requires exalted with Stormwind.
-- 23 * 46749/46757     the orc pair. AzerothCore attaches these conditions to
--                      the Darnassus and Exodar quartermasters, which sell
--                      neither item, so the Orgrimmar quartermaster sells
--                      46749 to anyone. They are matched by item, not vendor,
--                      so they stay right if the vendor is corrected.
-- spell_area 35482/35483
--                      the Caverns of Time's human illusion for night elves
--                      and draenei (1032), in Old Hillsbrad and the Culling of
--                      Stratholme: the races that did not belong there yet.
--                      Worgen did not either, and the core checks race 12,
--                      not human. Goblins get the Horde illusion (690) in
--                      worgoblin_x_faction_masks.sql.
--
-- Masks are tested on the ten original races (1791), so other races' bits in
-- them do not matter. `|` is idempotent.

UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 2048
  WHERE `ConditionTypeOrReference` = 16 AND (`ConditionValue1` & 1791) = 1
    AND ((`SourceTypeOrReferenceId` = 17 AND `SourceGroup` = 0 AND `SourceEntry` = 24105)
      OR (`SourceTypeOrReferenceId` = 23 AND `SourceGroup` = 33307 AND `SourceEntry` IN (46752, 46758)));

UPDATE `conditions` SET `ConditionValue1` = `ConditionValue1` | 256
  WHERE `ConditionTypeOrReference` = 16 AND (`ConditionValue1` & 1791) = 2
    AND ((`SourceTypeOrReferenceId` = 17 AND `SourceGroup` = 0 AND `SourceEntry` = 24104)
      OR (`SourceTypeOrReferenceId` = 23 AND `SourceEntry` IN (46749, 46757)));

UPDATE `spell_area` SET `racemask` = `racemask` | 2048
  WHERE `spell` IN (35482, 35483) AND (`racemask` & 1791) = 1032;
