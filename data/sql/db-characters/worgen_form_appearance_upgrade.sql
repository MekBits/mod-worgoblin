-- MekBits fork only: servers that ran an earlier Two Forms have
-- worgen_form_appearance without hs_/hp_, some with a `human` column, and
-- CREATE TABLE IF NOT EXISTS in worgen_form_appearance.sql leaves it that way.
-- Safe to run more than once, before or after the module's own SQL (the
-- updater runs that first: '.' sorts before '_'): each step checks the table
-- and its columns first. The ALTER commits on its own, so a run that stops after
-- it carries the flagged sets over on the next run while `human` is still there.
--
-- The earlier versions restored w_ at login when `characters` held a human set
-- after a crash: always (no `human` column) or when `human` = 1. The current
-- code restores w_ only when `characters` holds a set listed in hs_/hp_, so
-- such rows get that set in hs_: the `characters` set when it differs from w_
-- and is h_ or is flagged.
SET @has_table := (SELECT COUNT(*) FROM `information_schema`.`TABLES`
                   WHERE `TABLE_SCHEMA` = DATABASE()
                     AND `TABLE_NAME` = 'worgen_form_appearance');
SET @has_hs := (SELECT COUNT(*) FROM `information_schema`.`COLUMNS`
                WHERE `TABLE_SCHEMA` = DATABASE()
                  AND `TABLE_NAME` = 'worgen_form_appearance'
                  AND `COLUMN_NAME` = 'hs_skin');
SET @has_human := (SELECT COUNT(*) FROM `information_schema`.`COLUMNS`
                   WHERE `TABLE_SCHEMA` = DATABASE()
                     AND `TABLE_NAME` = 'worgen_form_appearance'
                     AND `COLUMN_NAME` = 'human');

SET @sql := IF(@has_table = 1 AND @has_hs = 0,
  'ALTER TABLE `worgen_form_appearance`
     ADD COLUMN `hs_skin`       TINYINT UNSIGNED NULL DEFAULT NULL COMMENT ''human set the last save wrote into characters'',
     ADD COLUMN `hs_face`       TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hs_hair`       TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hs_haircolor`  TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hs_facialhair` TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hp_skin`       TINYINT UNSIGNED NULL DEFAULT NULL COMMENT ''human set the save writing this row writes into characters'',
     ADD COLUMN `hp_face`       TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hp_hair`       TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hp_haircolor`  TINYINT UNSIGNED NULL DEFAULT NULL,
     ADD COLUMN `hp_facialhair` TINYINT UNSIGNED NULL DEFAULT NULL',
  'DO 0');
PREPARE `stmt` FROM @sql;
EXECUTE `stmt`;
DEALLOCATE PREPARE `stmt`;

SET @sql := IF(@has_table = 1 AND (@has_hs = 0 OR @has_human = 1),
  CONCAT(
    'UPDATE `worgen_form_appearance` f JOIN `characters` c ON c.`guid` = f.`guid`
       SET f.`hs_skin` = c.`skin`, f.`hs_face` = c.`face`, f.`hs_hair` = c.`hairStyle`,
           f.`hs_haircolor` = c.`hairColor`, f.`hs_facialhair` = c.`facialStyle`
     WHERE (c.`skin`, c.`face`, c.`hairStyle`, c.`hairColor`, c.`facialStyle`)
        <> (f.`w_skin`, f.`w_face`, f.`w_hair`, f.`w_haircolor`, f.`w_facialhair`)
       AND (',
    IF(@has_human = 1, 'f.`human` = 1 OR ', ''),
    '(c.`skin`, c.`face`, c.`hairStyle`, c.`hairColor`, c.`facialStyle`)
        = (f.`h_skin`, f.`h_face`, f.`h_hair`, f.`h_haircolor`, f.`h_facialhair`))'),
  'DO 0');
PREPARE `stmt` FROM @sql;
EXECUTE `stmt`;
DEALLOCATE PREPARE `stmt`;

SET @sql := IF(@has_human = 1, 'ALTER TABLE `worgen_form_appearance` DROP COLUMN `human`', 'DO 0');
PREPARE `stmt` FROM @sql;
EXECUTE `stmt`;
DEALLOCATE PREPARE `stmt`;
