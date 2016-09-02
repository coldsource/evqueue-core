ALTER TABLE t_workflow_instance ADD COLUMN node_name VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT '' AFTER workflow_instance_id;

ALTER TABLE t_workflow_schedule ADD COLUMN node_name VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT '' AFTER workflow_schedule_id;

ALTER TABLE t_workflow ADD COLUMN workflow_lastcommit VARCHAR(40) COLLATE 'ascii_general_ci' NULL DEFAULT NULL AFTER workflow_bound;

ALTER TABLE t_log ADD COLUMN node_name VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT '' AFTER log_id;

ALTER TABLE t_task ADD COLUMN task_comment TEXT COLLATE utf8_unicode_ci NOT NULL DEFAULT '' AFTER task_group;
ALTER TABLE t_task ADD COLUMN task_use_agent TINYINT NOT NULL DEFAULT 0 AFTER task_host;
ALTER TABLE t_task ADD COLUMN task_merge_stderr TINYINT NOT NULL DEFAULT 0 AFTER task_output_method;
ALTER TABLE t_task ADD UNIQUE KEY `task_name` (`task_name`);
ALTER TABLE t_task ADD COLUMN task_lastcommit VARCHAR(40) COLLATE 'ascii_general_ci' NULL DEFAULT NULL AFTER workflow_id;

ALTER TABLE t_notification_type DROP COLUMN notification_type_binary;
ALTER TABLE t_notification_type ADD COLUMN notification_type_binary_content LONGBLOB NULL DEFAULT NULL AFTER notification_type_description;
ALTER TABLE t_notification_type ADD COLUMN notification_type_conf_content LONGBLOB NULL DEFAULT NULL AFTER notification_type_binary_content;
ALTER TABLE t_notification_type ADD UNIQUE KEY `task_name` (`task_name`);

ALTER TABLE t_queue ADD COLUMN queue_scheduler VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT 'default' AFTER queue_concurrency;

ALTER TABLE t_workflow_schedule_parameters MODIFY workflow_schedule_parameter varchar(64) COLLATE utf8_unicode_ci NOT NULL;
ALTER TABLE t_workflow_instance_parameters MODIFY workflow_instance_parameter varchar(64) COLLATE utf8_unicode_ci NOT NULL;

ALTER TABLE t_user MODIFY user_password VARCHAR(40) COLLATE 'ascii_general_ci' NOT NULL;
ALTER TABLE t_user ADD COLUMN user_password_salt VARCHAR(40) COLLATE 'ascii_general_ci' NULL DEFAULT NULL AFTER user_password;
ALTER TABLE t_user ADD COLUMN user_password_iterations INT NOT NULL DEFAULT 0 AFTER user_password_salt;
ALTER TABLE t_user MODIFY user_profile ENUM('ADMIN','USER') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'USER';
UPDATE t_user SET user_profile='USER' WHERE user_profile='';
ALTER TABLE t_user_right DROP COLUMN user_right_del;

-- Alter version number --
ALTER TABLE t_log COMMENT 'v1.5';
ALTER TABLE t_notification COMMENT 'v1.5';
ALTER TABLE t_notification_type COMMENT 'v1.5';
ALTER TABLE t_queue COMMENT 'v1.5';
ALTER TABLE t_schedule COMMENT 'v1.5';
ALTER TABLE t_task COMMENT 'v1.5';
ALTER TABLE t_user COMMENT 'v1.5';
ALTER TABLE t_user_right COMMENT 'v1.5';
ALTER TABLE t_workflow COMMENT 'v1.5';
ALTER TABLE t_workflow_instance COMMENT 'v1.5';
ALTER TABLE t_workflow_instance_parameters COMMENT 'v1.5';
ALTER TABLE t_workflow_notification COMMENT 'v1.5';
ALTER TABLE t_workflow_schedule COMMENT 'v1.5';
ALTER TABLE t_workflow_schedule_parameters COMMENT 'v1.5';