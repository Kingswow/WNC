DROP TABLE IF EXISTS `anticheat_logs`;
CREATE TABLE `anticheat_logs`  (
  `account` INT(11) NOT NULL DEFAULT '0',
  `guid` INT(11) NOT NULL DEFAULT '0',
  `player` VARCHAR(96) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
  `flyHacks` INT(11) NOT NULL DEFAULT '0',
  `speedHacks` INT(11) NOT NULL DEFAULT '0',
  `doubleJumps` INT(11) NOT NULL DEFAULT '0',
  `fakeJumps` INT(11) NOT NULL DEFAULT '0',
  `fakeFly` INT(11) NOT NULL DEFAULT '0',
  `ignoreControls` INT(11) NOT NULL DEFAULT '0',
  `climbHacks` INT(11) NOT NULL DEFAULT '0',
  `noFalling` INT(11) NOT NULL DEFAULT '0',
  `waterwalk` INT(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE = INNODB AUTO_INCREMENT = 0 CHARACTER SET = utf8;
