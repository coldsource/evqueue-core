ALTER TABLE t_workflow_instance ADD COLUMN node_name VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT '' AFTER workflow_instance_id;

ALTER TABLE t_workflow_schedule ADD COLUMN node_name VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT '' AFTER workflow_schedule_id;

ALTER TABLE t_log ADD COLUMN node_name VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT '' AFTER log_id;

ALTER TABLE t_task ADD COLUMN task_binary_content LONGBLOB NULL DEFAULT NULL AFTER task_binary;

ALTER TABLE t_notification_type ADD COLUMN notification_type_binary_content LONGBLOB NULL DEFAULT NULL AFTER notification_type_binary;

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