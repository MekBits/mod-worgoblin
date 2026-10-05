-- MekBits fork only: earlier versions of worgoblin_two_forms.sql spawned the
-- Gilnean Barber at MAX(guid) + 1 and + 2. Those spawns go; the fixed guids
-- are (re)inserted here as well, in case worgoblin_two_forms.sql is not
-- reapplied (Updates.Redundancy = 0).
DELETE FROM `creature` WHERE `id` = 9000001 AND `guid` NOT IN (9000001, 9000002);
DELETE FROM `creature` WHERE `id` = 9000001 AND `guid` IN (9000001, 9000002);
INSERT INTO `creature`
  (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
   `position_x`, `position_y`, `position_z`, `orientation`,
   `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
   `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`,
   `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
  (9000001, 9000001, 1, 0, 0, 1, 1, 0, 10314.5, 835.2, 1326.41, 5.69632,
   300, 0, 0, 1, 0, 0, 0, 0, 0, '', 0, 0, 'Gilnean Barber - worgen start zone'),
  (9000002, 9000001, 0, 0, 0, 1, 1, 0, -8746.5, 659.6, 105.175, 3.1765,
   300, 0, 0, 1, 0, 0, 0, 0, 0, '', 0, 0, 'Gilnean Barber - Stormwind');
