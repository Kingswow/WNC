-- DB update 2021_10_15_01 -> 2021_10_15_02
DROP PROCEDURE IF EXISTS `updateDb`;
DELIMITER //
CREATE PROCEDURE updateDb ()
proc:BEGIN DECLARE OK VARCHAR(100) DEFAULT 'FALSE';
SELECT COUNT(*) INTO @COLEXISTS
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'version_db_world' AND COLUMN_NAME = '2021_10_15_01';
IF @COLEXISTS = 0 THEN LEAVE proc; END IF;
START TRANSACTION;
ALTER TABLE version_db_world CHANGE COLUMN 2021_10_15_01 2021_10_15_02 bit;
SELECT sql_rev INTO OK FROM version_db_world WHERE sql_rev = '1634008337760003200'; IF OK <> 'FALSE' THEN LEAVE proc; END IF;
--
-- START UPDATING QUERIES
--

INSERT INTO `version_db_world` (`sql_rev`) VALUES ('1634008337760003200');

-- Remove 'Immunity: Arcane' (34184) from on respawn events
DELETE FROM `smart_scripts` WHERE `entryorguid` = 10485 AND `source_type` = 0 AND `id` = 5;

--
-- END UPDATING QUERIES
--
UPDATE version_db_world SET date = '2021_10_15_02' WHERE sql_rev = '1634008337760003200';
COMMIT;
END //
DELIMITER ;
CALL updateDb();
DROP PROCEDURE IF EXISTS `updateDb`;
