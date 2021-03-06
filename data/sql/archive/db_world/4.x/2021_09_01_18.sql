-- DB update 2021_09_01_17 -> 2021_09_01_18
DROP PROCEDURE IF EXISTS `updateDb`;
DELIMITER //
CREATE PROCEDURE updateDb ()
proc:BEGIN DECLARE OK VARCHAR(100) DEFAULT 'FALSE';
SELECT COUNT(*) INTO @COLEXISTS
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'version_db_world' AND COLUMN_NAME = '2021_09_01_17';
IF @COLEXISTS = 0 THEN LEAVE proc; END IF;
START TRANSACTION;
ALTER TABLE version_db_world CHANGE COLUMN 2021_09_01_17 2021_09_01_18 bit;
SELECT sql_rev INTO OK FROM version_db_world WHERE sql_rev = '1630172168898330770'; IF OK <> 'FALSE' THEN LEAVE proc; END IF;
--
-- START UPDATING QUERIES
--

INSERT INTO `version_db_world` (`sql_rev`) VALUES ('1630172168898330770');

-- Bloodsail Charts
UPDATE `gameobject` SET `spawntimesecs` = 180 WHERE `id` = 2086 AND `guid` = 12154;
-- Bloodsail Orders
UPDATE `gameobject` SET `spawntimesecs` = 180 WHERE `id` = 2087 AND `guid` = 12156;

--
-- END UPDATING QUERIES
--
UPDATE version_db_world SET date = '2021_09_01_18' WHERE sql_rev = '1630172168898330770';
COMMIT;
END //
DELIMITER ;
CALL updateDb();
DROP PROCEDURE IF EXISTS `updateDb`;
