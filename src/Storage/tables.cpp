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

// evQueue storage moodule
static map<string, string> storage_tables = {
{"t_storage",
"CREATE TABLE `t_storage` ( \
  `storage_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `storage_path` varchar(255) NOT NULL, \
  `storage_type` enum('INT','STRING','BOOLEAN') CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `storage_structure` enum('NONE','ARRAY','MAP') CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `storage_name` varchar(64) NOT NULL, \
  `storage_value` mediumtext NOT NULL, \
  PRIMARY KEY (`storage_id`), \
  UNIQUE KEY `storage_namespace` (`storage_path`,`storage_name`) \
) ENGINE=InnoDB AUTO_INCREMENT=38 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='v3.3';\
"},
{"t_launcher",
"CREATE TABLE `t_launcher` ( \
  `launcher_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `launcher_name` varchar(64) NOT NULL, \
  `launcher_group` varchar(64) NOT NULL, \
  `launcher_description` text NOT NULL, \
  `workflow_id` int(10) unsigned NOT NULL, \
  `launcher_user` varchar(64) NOT NULL, \
  `launcher_host` varchar(64) NOT NULL, \
  `launcher_parameters` text NOT NULL, \
  PRIMARY KEY (`launcher_id`), \
  UNIQUE KEY `launcher_name` (`launcher_name`) \
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='v3.3';\
"},
{"t_display",
"CREATE TABLE `t_display` ( \
 `display_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
 `display_name` varchar(64) NOT NULL, \
 `storage_path` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
 `display_order` enum('ASC','DESC') CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
 `display_item_title` varchar(128) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL, \
 `display_item_content` text CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL, \
 PRIMARY KEY (`display_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci \
"}
};


static auto init = DBConfig::GetInstance()->RegisterTables("evqueue", storage_tables);
