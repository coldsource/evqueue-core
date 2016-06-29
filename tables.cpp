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

#include <string>
#include <map>

std::map<std::string,std::string> evqueue_tables = {
{"t_log",
"CREATE TABLE `t_log` ( \
  `log_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `log_level` int(11) NOT NULL, \
  `log_message` text COLLATE utf8_unicode_ci NOT NULL, \
  `log_timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
  PRIMARY KEY (`log_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_notification",
"CREATE TABLE `t_notification` ( \
  `notification_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `notification_type_id` int(10) unsigned NOT NULL, \
  `notification_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `notification_parameters` text COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`notification_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_notification_type",
"CREATE TABLE `t_notification_type` ( \
  `notification_type_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `notification_type_name` varchar(32) COLLATE utf8_unicode_ci NOT NULL, \
  `notification_type_description` text COLLATE utf8_unicode_ci NOT NULL, \
  `notification_type_binary` varchar(128) COLLATE utf8_unicode_ci NOT NULL, \
  `notification_type_binary_content` longblob, \
  PRIMARY KEY (`notification_type_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_queue",
"CREATE TABLE `t_queue` ( \
  `queue_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `queue_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `queue_concurrency` int(10) unsigned NOT NULL, \
  `queue_scheduler` VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT 'default', \
  PRIMARY KEY (`queue_id`), \
  UNIQUE KEY `queue_name` (`queue_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='v1.5'; \
"},
{"t_schedule",
"CREATE TABLE `t_schedule` ( \
  `schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `schedule_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `schedule_xml` text COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`schedule_id`), \
  UNIQUE KEY `schedule_name` (`schedule_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_task",
"CREATE TABLE `t_task` ( \
  `task_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `task_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `task_binary` varchar(128) COLLATE utf8_unicode_ci NOT NULL, \
  `task_binary_content` longblob, \
  `task_wd` varchar(255) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `task_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `task_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `task_use_agent` tinyint NOT NULL DEFAULT 0, \
  `task_parameters_mode` enum('CMDLINE','PIPE','ENV') CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `task_output_method` enum('XML','TEXT') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'XML', \
  `task_merge_stderr` tinyint NOT NULL DEFAULT 0, \
  `task_xsd` longtext COLLATE utf8_unicode_ci, \
  `task_group` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_id` int(10) DEFAULT NULL, \
  UNIQUE KEY `taskdesc_id` (`task_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_user",
"CREATE TABLE `t_user` ( \
  `user_login` varchar(32) COLLATE utf8_unicode_ci NOT NULL, \
  `user_password` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `user_profile` enum('ADMIN','REGULAR_EVERYDAY_NORMAL_GUY') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'REGULAR_EVERYDAY_NORMAL_GUY', \
  PRIMARY KEY (`user_login`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_user_right",
"CREATE TABLE `t_user_right` ( \
  `user_login` varchar(32) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_id` int(10) NOT NULL, \
  `user_right_edit` tinyint(1) NOT NULL DEFAULT '0', \
  `user_right_read` tinyint(1) NOT NULL DEFAULT '0', \
  `user_right_exec` tinyint(1) NOT NULL DEFAULT '0', \
  `user_right_kill` tinyint(4) NOT NULL DEFAULT '0', \
  `user_right_del` tinyint(4) NOT NULL DEFAULT '0', \
  KEY `user_login` (`user_login`,`workflow_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_workflow",
"CREATE TABLE `t_workflow` ( \
  `workflow_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `workflow_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_xml` longtext COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_group` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_comment` text COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_bound` tinyint(1) NOT NULL DEFAULT '0', \
  UNIQUE KEY `workflow_id` (`workflow_id`), \
  UNIQUE KEY `workflow_name` (`workflow_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_workflow_instance",
"CREATE TABLE `t_workflow_instance` ( \
  `workflow_instance_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `workflow_id` int(10) NOT NULL, \
  `workflow_schedule_id` int(10) unsigned DEFAULT NULL, \
  `workflow_instance_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_instance_start` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, \
  `workflow_instance_end` timestamp NULL, \
  `workflow_instance_status` enum('EXECUTING','TERMINATED','ABORTED') COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_instance_errors` int(10) unsigned NULL, \
  `workflow_instance_savepoint` longtext COLLATE utf8_unicode_ci NULL DEFAULT NULL, \
  UNIQUE KEY `workflow_instance_id` (`workflow_instance_id`), \
  KEY `workflow_instance_status` (`workflow_instance_status`), \
  KEY `workflow_instance_date_end` (`workflow_instance_end`), \
  KEY `t_workflow_instance_errors` (`workflow_instance_errors`), \
  KEY `workflow_instance_date_start` (`workflow_instance_start`), \
  KEY `workflow_schedule_id` (`workflow_schedule_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_workflow_instance_parameters",
"CREATE TABLE `t_workflow_instance_parameters` ( \
  `workflow_instance_id` int(10) unsigned NOT NULL, \
  `workflow_instance_parameter` varchar(35) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_instance_parameter_value` text COLLATE utf8_unicode_ci NOT NULL, \
  KEY `param_and_value` (`workflow_instance_parameter`,`workflow_instance_parameter_value`(255)) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_workflow_notification",
"CREATE TABLE `t_workflow_notification` ( \
  `workflow_id` int(10) unsigned NOT NULL, \
  `notification_id` int(10) unsigned NOT NULL, \
  PRIMARY KEY (`workflow_id`,`notification_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_workflow_schedule",
"CREATE TABLE `t_workflow_schedule` ( \
  `workflow_schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `workflow_id` int(10) unsigned NOT NULL, \
  `workflow_schedule` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `workflow_schedule_onfailure` enum('CONTINUE','SUSPEND') COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_schedule_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_schedule_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_schedule_active` tinyint(4) NOT NULL, \
  `workflow_schedule_comment` text COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`workflow_schedule_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"},
{"t_workflow_schedule_parameters",
"CREATE TABLE `t_workflow_schedule_parameters` ( \
  `workflow_schedule_id` int(10) unsigned NOT NULL, \
  `workflow_schedule_parameter` varchar(35) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_schedule_parameter_value` text COLLATE utf8_unicode_ci NOT NULL, \
  KEY `workflow_schedule_id` (`workflow_schedule_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5'; \
"}
};
