-- MySQL dump 10.13  Distrib 5.6.28, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: evqueue
-- ------------------------------------------------------
-- Server version	5.6.28-0ubuntu0.15.10.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `t_log`
--

DROP TABLE IF EXISTS `t_log`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_log` (
  `log_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '',
  `log_level` int(11) NOT NULL,
  `log_message` text COLLATE utf8_unicode_ci NOT NULL,
  `log_timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`log_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_notification`
--

DROP TABLE IF EXISTS `t_notification`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_notification` (
  `notification_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `notification_type_id` int(10) unsigned NOT NULL,
  `notification_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `notification_parameters` text COLLATE utf8_unicode_ci NOT NULL,
  PRIMARY KEY (`notification_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_notification_type`
--

DROP TABLE IF EXISTS `t_notification_type`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_notification_type` (
  `notification_type_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `notification_type_name` varchar(32) COLLATE utf8_unicode_ci NOT NULL,
  `notification_type_description` text COLLATE utf8_unicode_ci NOT NULL,
  `notification_type_binary` varchar(128) COLLATE utf8_unicode_ci NOT NULL,
  `notification_type_binary_content` longblob,
  PRIMARY KEY (`notification_type_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_queue`
--

DROP TABLE IF EXISTS `t_queue`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_queue` (
  `queue_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `queue_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `queue_concurrency` int(10) unsigned NOT NULL,
  PRIMARY KEY (`queue_id`),
  UNIQUE KEY `queue_name` (`queue_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_schedule`
--

DROP TABLE IF EXISTS `t_schedule`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_schedule` (
  `schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `schedule_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `schedule_xml` text COLLATE utf8_unicode_ci NOT NULL,
  PRIMARY KEY (`schedule_id`),
  UNIQUE KEY `schedule_name` (`schedule_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_task`
--

DROP TABLE IF EXISTS `t_task`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_task` (
  `task_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `task_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `task_binary` varchar(128) COLLATE utf8_unicode_ci NOT NULL,
  `task_binary_content` longblob,
  `task_wd` varchar(255) COLLATE utf8_unicode_ci DEFAULT NULL,
  `task_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL,
  `task_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL,
  `task_parameters_mode` enum('CMDLINE','PIPE','ENV') CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `task_output_method` enum('XML','TEXT') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'XML',
  `task_xsd` longtext COLLATE utf8_unicode_ci,
  `task_group` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_id` int(10) DEFAULT NULL,
  UNIQUE KEY `taskdesc_id` (`task_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_user`
--

DROP TABLE IF EXISTS `t_user`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_user` (
  `user_login` varchar(32) COLLATE utf8_unicode_ci NOT NULL,
  `user_password` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `user_profile` enum('ADMIN','REGULAR_EVERYDAY_NORMAL_GUY') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'REGULAR_EVERYDAY_NORMAL_GUY',
  PRIMARY KEY (`user_login`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_user_right`
--

DROP TABLE IF EXISTS `t_user_right`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_user_right` (
  `user_login` varchar(32) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_id` int(10) NOT NULL,
  `user_right_edit` tinyint(1) NOT NULL DEFAULT '0',
  `user_right_read` tinyint(1) NOT NULL DEFAULT '0',
  `user_right_exec` tinyint(1) NOT NULL DEFAULT '0',
  `user_right_kill` tinyint(4) NOT NULL DEFAULT '0',
  `user_right_del` tinyint(4) NOT NULL DEFAULT '0',
  KEY `user_login` (`user_login`,`workflow_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_workflow`
--

DROP TABLE IF EXISTS `t_workflow`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow` (
  `workflow_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `workflow_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_xml` longtext COLLATE utf8_unicode_ci NOT NULL,
  `workflow_group` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_comment` text COLLATE utf8_unicode_ci NOT NULL,
  `workflow_bound` tinyint(1) NOT NULL DEFAULT '0',
  UNIQUE KEY `workflow_id` (`workflow_id`),
  UNIQUE KEY `workflow_name` (`workflow_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_workflow_instance`
--

DROP TABLE IF EXISTS `t_workflow_instance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow_instance` (
  `workflow_instance_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '',
  `workflow_id` int(10) NOT NULL,
  `workflow_schedule_id` int(10) unsigned DEFAULT NULL,
  `workflow_instance_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL,
  `workflow_instance_start` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `workflow_instance_end` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `workflow_instance_status` enum('EXECUTING','TERMINATED','ABORTED') COLLATE utf8_unicode_ci NOT NULL,
  `workflow_instance_errors` int(10) unsigned NOT NULL DEFAULT 0,
  `workflow_instance_savepoint` longtext COLLATE utf8_unicode_ci NULL DEFAULT NULL,
  UNIQUE KEY `workflow_instance_id` (`workflow_instance_id`),
  KEY `workflow_instance_status` (`workflow_instance_status`),
  KEY `workflow_instance_date_end` (`workflow_instance_end`),
  KEY `t_workflow_instance_errors` (`workflow_instance_errors`),
  KEY `workflow_instance_date_start` (`workflow_instance_start`),
  KEY `workflow_schedule_id` (`workflow_schedule_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_workflow_instance_parameters`
--

DROP TABLE IF EXISTS `t_workflow_instance_parameters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow_instance_parameters` (
  `workflow_instance_id` int(10) unsigned NOT NULL,
  `workflow_instance_parameter` varchar(35) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_instance_parameter_value` text COLLATE utf8_unicode_ci NOT NULL,
  KEY `param_and_value` (`workflow_instance_parameter`,`workflow_instance_parameter_value`(255))
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_workflow_notification`
--

DROP TABLE IF EXISTS `t_workflow_notification`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow_notification` (
  `workflow_id` int(10) unsigned NOT NULL,
  `notification_id` int(10) unsigned NOT NULL,
  PRIMARY KEY (`workflow_id`,`notification_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_workflow_schedule`
--

DROP TABLE IF EXISTS `t_workflow_schedule`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow_schedule` (
  `workflow_schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '',
  `workflow_id` int(10) unsigned NOT NULL,
  `workflow_schedule` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `workflow_schedule_onfailure` enum('CONTINUE','SUSPEND') COLLATE utf8_unicode_ci NOT NULL,
  `workflow_schedule_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL,
  `workflow_schedule_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL,
  `workflow_schedule_active` tinyint(4) NOT NULL,
  `workflow_schedule_comment` text COLLATE utf8_unicode_ci NOT NULL,
  PRIMARY KEY (`workflow_schedule_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_workflow_schedule_parameters`
--

DROP TABLE IF EXISTS `t_workflow_schedule_parameters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow_schedule_parameters` (
  `workflow_schedule_id` int(10) unsigned NOT NULL,
  `workflow_schedule_parameter` varchar(35) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_schedule_parameter_value` text COLLATE utf8_unicode_ci NOT NULL,
  KEY `workflow_schedule_id` (`workflow_schedule_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2016-02-20 16:40:26
INSERT INTO t_queue(queue_name,queue_concurrency) VALUES('default',1);
INSERT INTO t_user VALUES('admin',SHA1('admin'),'ADMIN');
INSERT INTO `t_notification_type` VALUES (1,'email','Send an email to one or several recipients','email.php','#!/usr/bin/php\n<?php\n// Sanity check\nif($argc!=4)\n{\n error_log(\"This process should only be called as an evQueue plugin\\n\");\n    die(5);\n}\n\n// Read plugin configuration\nrequire_once \'conf/email.conf.php\';\n\n// Read configuration\n$stdin = fopen(\'php://stdin\',\'r\');\n\n$config_str = stream_get_contents($stdin);\nif($config_str==false)\n{\n   error_log(\"No configuration could be read on stdin\\n\");\n    die(1);\n}\n\n// Decode configuration\n$config = json_decode($config_str,true);\nif($config===null)\n{\n        error_log(\"Unable to decode json data\\n\");\n die(2);\n}\n\nif(!isset($config[\'subject\']) || !isset($config[\'when\']) || !isset($config[\'to\']) || !isset($config[\'body\']))\n{\n        error_log(\"Invalid configuration\\n\");\n      die(3);\n}\n\n// Read workflow instance informations from evQueue engine\n$vars = array(\'#ID#\'=>$argv[1]);\nif($argv[3])\n{\n    $s = fsockopen(\"unix://{$argv[3]}\");\n    \n    fwrite($s,\"<workflow id=\'{$argv[1]}\' />\");\n    $xml = stream_get_contents($s);\n    fclose($s);\n    \n    $xml = simplexml_load_string($xml);\n    $workflow_attributes = $xml->attributes();\n    $vars[\'#NAME#\'] = (string)$workflow_attributes[\'name\'];\n    $vars[\'#START_TIME#\'] = (string)$workflow_attributes[\'start_time\'];\n    $vars[\'#END_TIME#\'] = (string)$workflow_attributes[\'end_time\'];\n}\n\n// Extract mail informations from config\n$to = $config[\'to\'];\n$subject = $config[\'subject\'];\n$body = $config[\'body\'];\n$when = $config[\'when\'];\n$cc = isset($config[\'cc\'])?$config[\'cc\']:false;\n\nif($when!=\'ON_SUCCESS\' && $when!=\'ON_ERROR\' && $when!=\'ON_BOTH\')\n{\n    error_log(\"Invalid value for \'when\' parameter\\n\");\n       die(6);\n}\n\n// When should be trigger alert\nif($when==\'ON_SUCCESS\' && $argv[2]!=0)\n       die();\n\nif($when==\'ON_ERROR\' && $argv[2]==0)\n      die();\n\n// Do variables substitution\n$values = array_values($vars);\n$vars = array_keys($vars);\n\n$subject = str_replace($vars,$values,$subject);\n$body = str_replace($vars,$values,$body);\n\n// Send email\n$cmdline = \'/usr/bin/mail\';\n$cmdline .= \" -s \'\".addslashes($subject).\"\'\";\nif($cc)\n       $cmdline .= \" -c \'\".addslashes($cc).\"\'\";\n$cmdline .= \" -a \'\".addslashes(\'From: \'.$EMAIL_CONFIG[\'from\']).\"\'\";\n$cmdline .= \' \'.addslashes($to);\n\n$fd = array(0 => array(\'pipe\', \'r\'));\n$proc = proc_open($cmdline, $fd, $pipes);\n\nif(!is_resource($proc))\n   die(4);\n\nfwrite($pipes[0],$body);\nfclose($pipes[0]);\n$return_value = proc_close($proc);\n\ndie($return_value);\n?>');