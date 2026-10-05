-- Three worgen faces offered at character creation name textures the client
-- cannot use; the client half is CharSections.dbc in data/patch/DBFilesClient.
--
--   20057  male face 9, skin 1       both textures were empty
--   20403  female face 20, skin 8    both textures were empty
--   20254  female face 5, skin 3     FaceLower was in the FaceUpper slot
--
-- An empty face row is the crash in heyitsbench/mod-worgoblin#14. On the
-- server the rows only matter to playerbots, which roll their appearance out of
-- CharSections; the textures are written so both halves stay identical.
--
-- No column list: older databases have mod-playerbots' charsections_dbc (Id,
-- Race, Gender, GenType, TexturePath1-3, Flags, Type, Color), newer ones the
-- core's (ID, RaceID, SexID, BaseSection, TextureName_1-3, Flags,
-- VariationIndex, ColorIndex). The order is the same, and the core reads the
-- table by position.

DELETE FROM `charsections_dbc` WHERE `ID` IN (20057, 20254, 20403);
INSERT INTO `charsections_dbc` VALUES
  (20057, 12, 0, 1, 'Character\\Worgen\\Male\\WorgenMaleFaceLower09_01.blp', 'Character\\Worgen\\Male\\WorgenMaleFaceUpper09_01.blp', '', 5, 9, 1),
  (20254, 12, 1, 1, 'Character\\Worgen\\Female\\WorgenFemaleFaceLower05_03.blp', 'Character\\Worgen\\Female\\WorgenFemaleFaceUpper05_03.blp', '', 1, 5, 3),
  (20403, 12, 1, 1, 'Character\\Worgen\\Female\\WorgenFemaleFaceLower20_08.blp', 'Character\\Worgen\\Female\\WorgenFemaleFaceUpper20_08.blp', '', 5, 20, 8);
