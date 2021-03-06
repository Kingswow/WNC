-- DB update 2021_08_26_03 -> 2021_08_26_04
DROP PROCEDURE IF EXISTS `updateDb`;
DELIMITER //
CREATE PROCEDURE updateDb ()
proc:BEGIN DECLARE OK VARCHAR(100) DEFAULT 'FALSE';
SELECT COUNT(*) INTO @COLEXISTS
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'version_db_world' AND COLUMN_NAME = '2021_08_26_03';
IF @COLEXISTS = 0 THEN LEAVE proc; END IF;
START TRANSACTION;
ALTER TABLE version_db_world CHANGE COLUMN 2021_08_26_03 2021_08_26_04 bit;
SELECT sql_rev INTO OK FROM version_db_world WHERE sql_rev = '1629555309650648100'; IF OK <> 'FALSE' THEN LEAVE proc; END IF;
--
-- START UPDATING QUERIES
--

INSERT INTO `version_db_world` (`sql_rev`) VALUES ('1629555309650648100');

# Add movement to static Vengeful Apparition
UPDATE `creature` SET `MovementType` = 1, `wander_distance` = 5 WHERE `id` = 16328 AND `guid` IN (81883, 81902, 82022, 82028, 82031, 82039);

# Add movement to static Ravening Apparition
UPDATE `creature` SET `MovementType` = 1, `wander_distance` = 5 WHERE `id` = 16327 AND `guid` IN (81872, 81873, 81874, 81880, 81884, 81889, 81903, 81904, 82024, 82034, 82037);

--
-- END UPDATING QUERIES
--
UPDATE version_db_world SET date = '2021_08_26_04' WHERE sql_rev = '1629555309650648100';
COMMIT;
END //
DELIMITER ;
CALL updateDb();
DROP PROCEDURE IF EXISTS `updateDb`;
