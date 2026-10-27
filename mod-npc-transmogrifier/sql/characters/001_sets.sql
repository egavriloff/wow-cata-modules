-- Characters only. Appearance remains in native item_instance.enchantments.
CREATE TABLE IF NOT EXISTS `custom_transmogrification_sets` (
  `Owner` INT UNSIGNED NOT NULL,
  `PresetID` TINYINT UNSIGNED NOT NULL,
  `SetName` VARCHAR(64) NOT NULL DEFAULT '',
  `SetData` TEXT NOT NULL,
  PRIMARY KEY (`Owner`,`PresetID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
