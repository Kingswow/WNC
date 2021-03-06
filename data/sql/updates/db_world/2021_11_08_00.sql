-- DB update 2021_11_07_05 -> 2021_11_08_00
DROP PROCEDURE IF EXISTS `updateDb`;
DELIMITER //
CREATE PROCEDURE updateDb ()
proc:BEGIN DECLARE OK VARCHAR(100) DEFAULT 'FALSE';
SELECT COUNT(*) INTO @COLEXISTS
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'version_db_world' AND COLUMN_NAME = '2021_11_07_05';
IF @COLEXISTS = 0 THEN LEAVE proc; END IF;
START TRANSACTION;
ALTER TABLE version_db_world CHANGE COLUMN 2021_11_07_05 2021_11_08_00 bit;
SELECT sql_rev INTO OK FROM version_db_world WHERE sql_rev = '1636243951230286700'; IF OK <> 'FALSE' THEN LEAVE proc; END IF;
--
-- START UPDATING QUERIES
--

INSERT INTO `version_db_world` (`sql_rev`) VALUES ('1636243951230286700');

DELETE FROM `creature_formations` WHERE `leaderGUID` = 137971;
INSERT INTO `creature_formations` (`leaderGUID`, `memberGUID`, `dist`, `angle`, `groupAI`, `point_1`, `point_2`) VALUES
(137971, 137971, 0, 0, 515, 0, 0),
(137971, 90976, 0, 0, 515, 0, 0),
(137971, 90975, 0, 0, 515, 0, 0);

--
-- END UPDATING QUERIES
--
UPDATE version_db_world SET date = '2021_11_08_00' WHERE sql_rev = '1636243951230286700';
COMMIT;
END //
DELIMITER ;
CALL updateDb();
DROP PROCEDURE IF EXISTS `updateDb`;
