/*
 * This file is part of evQueue
 * 
 * evQueue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * evQueue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with evQueue. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thibault Kummer <bob@coldsource.net>
 */

#include <DB/DBConfig.h>

#include <string>
#include <map>

using namespace std;

// evQueue elogs moodule
static map<string, string> elogs_tables = {
{"t_alert",
"CREATE TABLE `t_alert` ( \
  `alert_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `alert_name` varchar(64) CHARACTER SET utf8 NOT NULL, \
  `alert_description` text CHARACTER SET utf8 NOT NULL, \
  `alert_occurrences` int(10) unsigned NOT NULL, \
  `alert_period` int(10) unsigned NOT NULL, \
  `alert_group` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `alert_filters` text CHARACTER SET utf8 NOT NULL, \
  `alert_active` tinyint(4) NOT NULL DEFAULT 1, \
  PRIMARY KEY (`alert_id`), \
  UNIQUE KEY `alert_name` (`alert_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_alert_notification",
"CREATE TABLE `t_alert_notification` ( \
  `alert_id` int(10) unsigned NOT NULL, \
  `notification_id` int(10) unsigned NOT NULL, \
  PRIMARY KEY (`alert_id`,`notification_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_alert_trigger",
"CREATE TABLE `t_alert_trigger` ( \
  `alert_trigger_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `alert_id` int(10) unsigned NOT NULL, \
  `alert_trigger_start` datetime NOT NULL, \
  `alert_trigger_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `alert_trigger_filters` text CHARACTER SET utf8 NOT NULL, \
  PRIMARY KEY (`alert_trigger_id`), \
  KEY `alert_id` (`alert_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_channel",
"CREATE TABLE `t_channel` ( \
  `channel_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `channel_group_id` int(10) unsigned NOT NULL, \
  `channel_name` varchar(32) CHARACTER SET utf8 NOT NULL, \
  `channel_config` text CHARACTER SET utf8 NOT NULL, \
  PRIMARY KEY (`channel_id`), \
  UNIQUE KEY `log_channel_name` (`channel_name`), \
  KEY `channel_group_id` (`channel_group_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_channel_group",
"CREATE TABLE `t_channel_group` ( \
  `channel_group_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `channel_group_name` varchar(32) CHARACTER SET utf8 NOT NULL, \
  PRIMARY KEY (`channel_group_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_field",
"CREATE TABLE `t_field` ( \
  `field_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `channel_group_id` int(10) unsigned DEFAULT NULL, \
  `channel_id` int(10) unsigned DEFAULT NULL, \
  `field_name` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `field_type` enum('CHAR','INT','IP','PACK','TEXT','ITEXT') CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  PRIMARY KEY (`field_id`), \
  UNIQUE KEY `channel_id` (`channel_id`,`field_name`) USING BTREE, \
  KEY `channel_group_id` (`channel_group_id`,`field_name`) USING BTREE \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_log",
"CREATE TABLE `t_log` ( \
  `log_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, \
  `channel_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `log_crit` int(10) unsigned NOT NULL, \
  PRIMARY KEY (`log_id`,`log_date`) USING BTREE, \
  KEY `log_crit` (`log_crit`), \
  KEY `log_date` (`log_date`) USING BTREE, \
  KEY `channel_id` (`log_date`,`channel_id`) USING BTREE \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3' \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"},
{"t_pack",
"CREATE TABLE `t_pack` ( \
  `pack_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `pack_string` varchar(255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, \
  PRIMARY KEY (`pack_id`), \
  UNIQUE KEY `log_pack_string` (`pack_string`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3'; \
"},
{"t_value_char",
"CREATE TABLE `t_value_char` ( \
  `log_id` bigint(20) unsigned NOT NULL, \
  `field_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `value` varchar(255) NOT NULL, \
  PRIMARY KEY (`log_id`,`field_id`,`log_date`) USING BTREE, \
  KEY `elog_field_id` (`field_id`,`value`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3' \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"},
{"t_value_int",
"CREATE TABLE `t_value_int` ( \
  `log_id` bigint(20) unsigned NOT NULL, \
  `field_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `value` int(11) NOT NULL, \
  PRIMARY KEY (`log_id`,`field_id`,`log_date`) USING BTREE, \
  KEY `elog_field_id` (`field_id`,`value`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3' \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"},
{"t_value_ip",
"CREATE TABLE `t_value_ip` ( \
  `log_id` bigint(20) unsigned NOT NULL, \
  `field_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `value` varbinary(16) NOT NULL, \
  PRIMARY KEY (`log_id`,`field_id`,`log_date`) USING BTREE, \
  KEY `elog_field_id` (`field_id`,`value`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3' \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"},
{"t_value_itext",
"CREATE TABLE `t_value_itext` ( \
  `log_id` bigint(20) unsigned NOT NULL, \
  `field_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `value` text NOT NULL, \
  `value_sha1` binary(20) NOT NULL, \
  PRIMARY KEY (`log_id`,`field_id`,`log_date`) USING BTREE, \
  KEY `field_id` (`field_id`,`value_sha1`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3' \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"},
{"t_value_pack",
"CREATE TABLE `t_value_pack` ( \
  `log_id` bigint(20) unsigned NOT NULL, \
  `field_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `value` int(11) unsigned NOT NULL, \
  PRIMARY KEY (`log_id`,`field_id`,`log_date`) USING BTREE, \
  KEY `elog_field_id` (`field_id`,`value`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='v3.3' \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"},
{"t_value_text",
"CREATE TABLE `t_value_text` ( \
  `log_id` bigint(20) unsigned NOT NULL, \
  `field_id` int(10) unsigned NOT NULL, \
  `log_date` datetime NOT NULL DEFAULT current_timestamp(), \
  `value` text NOT NULL, \
  PRIMARY KEY (`log_id`,`field_id`,`log_date`) USING BTREE, \
  KEY `field_id` (`field_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 \
 PARTITION BY RANGE (to_days(`log_date`)) \
(PARTITION `p0` VALUES LESS THAN (0) ENGINE = InnoDB); \
"}
};


static auto init = DBConfig::GetInstance()->RegisterTables("elog", elogs_tables);
