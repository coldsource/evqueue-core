-- MySQL dump 10.13  Distrib 5.5.43, for debian-linux-gnu (x86_64)
--
-- Host: localhost    Database: evqueue
-- ------------------------------------------------------
-- Server version	5.5.43-0ubuntu0.14.10.1

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
  `log_level` int(11) NOT NULL,
  `log_message` text COLLATE utf8_unicode_ci NOT NULL,
  `log_timestamp` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`log_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_log`
--

LOCK TABLES `t_log` WRITE;
/*!40000 ALTER TABLE `t_log` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_log` ENABLE KEYS */;
UNLOCK TABLES;

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
-- Dumping data for table `t_notification`
--

LOCK TABLES `t_notification` WRITE;
/*!40000 ALTER TABLE `t_notification` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_notification` ENABLE KEYS */;
UNLOCK TABLES;

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
  PRIMARY KEY (`notification_type_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_notification_type`
--

LOCK TABLES `t_notification_type` WRITE;
/*!40000 ALTER TABLE `t_notification_type` DISABLE KEYS */;
INSERT INTO `t_notification_type` VALUES (1,'email','Sends an email','email.php');
/*!40000 ALTER TABLE `t_notification_type` ENABLE KEYS */;
UNLOCK TABLES;

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
) ENGINE=InnoDB AUTO_INCREMENT=25 DEFAULT CHARSET=utf8 COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_queue`
--

LOCK TABLES `t_queue` WRITE;
/*!40000 ALTER TABLE `t_queue` DISABLE KEYS */;
INSERT INTO `t_queue` VALUES (24,'default',1);
/*!40000 ALTER TABLE `t_queue` ENABLE KEYS */;
UNLOCK TABLES;

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
-- Dumping data for table `t_schedule`
--

LOCK TABLES `t_schedule` WRITE;
/*!40000 ALTER TABLE `t_schedule` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_schedule` ENABLE KEYS */;
UNLOCK TABLES;

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
  `task_wd` varchar(255) COLLATE utf8_unicode_ci DEFAULT NULL,
  `task_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL,
  `task_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL,
  `task_parameters_mode` enum('CMDLINE','PIPE','ENV') CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `task_output_method` enum('XML','TEXT') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'XML',
  `task_xsd` longtext COLLATE utf8_unicode_ci,
  `task_group` varchar(64) COLLATE utf8_unicode_ci NOT NULL,
  `workflow_id` int(10) DEFAULT NULL,
  UNIQUE KEY `taskdesc_id` (`task_id`)
) ENGINE=InnoDB AUTO_INCREMENT=144 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_task`
--

LOCK TABLES `t_task` WRITE;
/*!40000 ALTER TABLE `t_task` DISABLE KEYS */;
INSERT INTO `t_task` VALUES (142,'ls','/bin/ls',NULL,NULL,NULL,'CMDLINE','TEXT',NULL,'',NULL),(143,'sleep','/bin/sleep',NULL,NULL,NULL,'CMDLINE','TEXT',NULL,'',NULL);
/*!40000 ALTER TABLE `t_task` ENABLE KEYS */;
UNLOCK TABLES;

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
-- Dumping data for table `t_user`
--

LOCK TABLES `t_user` WRITE;
/*!40000 ALTER TABLE `t_user` DISABLE KEYS */;
INSERT INTO `t_user` VALUES ('admin','d033e22ae348aeb5660fc2140aec35850c4da997','ADMIN');
/*!40000 ALTER TABLE `t_user` ENABLE KEYS */;
UNLOCK TABLES;

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
-- Dumping data for table `t_user_right`
--

LOCK TABLES `t_user_right` WRITE;
/*!40000 ALTER TABLE `t_user_right` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_user_right` ENABLE KEYS */;
UNLOCK TABLES;

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
) ENGINE=InnoDB AUTO_INCREMENT=176 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_workflow`
--

LOCK TABLES `t_workflow` WRITE;
/*!40000 ALTER TABLE `t_workflow` DISABLE KEYS */;
INSERT INTO `t_workflow` VALUES (174,'test','<workflow><parameters/><subjobs><job><tasks><task name=\"ls\" queue=\"default\"><input name=\"filename\">/</input></task></tasks></job></subjobs></workflow>','','',0),(175,'test_sleep','<workflow><parameters><parameter name=\"seconds\"/></parameters><subjobs><job><tasks><task name=\"sleep\" queue=\"default\"><input name=\"duration\"><value select=\"/workflow/parameters/parameter[@name=\'seconds\']\"/></input></task></tasks></job></subjobs></workflow>','','',0);
/*!40000 ALTER TABLE `t_workflow` ENABLE KEYS */;
UNLOCK TABLES;

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
  `workflow_instance_end` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
  `workflow_instance_status` enum('EXECUTING','TERMINATED','ABORTED') COLLATE utf8_unicode_ci NOT NULL,
  `workflow_instance_errors` int(10) unsigned NOT NULL,
  `workflow_instance_savepoint` longtext COLLATE utf8_unicode_ci NOT NULL,
  UNIQUE KEY `workflow_instance_id` (`workflow_instance_id`),
  KEY `workflow_instance_status` (`workflow_instance_status`),
  KEY `workflow_instance_date_end` (`workflow_instance_end`),
  KEY `t_workflow_instance_errors` (`workflow_instance_errors`),
  KEY `workflow_instance_date_start` (`workflow_instance_start`),
  KEY `workflow_schedule_id` (`workflow_schedule_id`)
) ENGINE=InnoDB AUTO_INCREMENT=33 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_workflow_instance`
--

LOCK TABLES `t_workflow_instance` WRITE;
/*!40000 ALTER TABLE `t_workflow_instance` DISABLE KEYS */;
INSERT INTO `t_workflow_instance` VALUES (19,'',174,NULL,NULL,'2015-09-06 20:25:39','2015-09-06 20:25:40','TERMINATED',0,'<workflow end_time=\"2015-09-06 22:25:40\" errors=\"0\" id=\"19\" name=\"test\" start_time=\"2015-09-06 22:25:40\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 22:25:40\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 22:25:40\" exit_time=\"2015-09-06 22:25:40\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(20,'',174,NULL,NULL,'2015-09-06 20:27:11','2015-09-06 20:27:12','TERMINATED',0,'<workflow end_time=\"2015-09-06 22:27:12\" errors=\"0\" id=\"20\" name=\"test\" start_time=\"2015-09-06 22:27:11\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 22:27:12\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 22:27:12\" exit_time=\"2015-09-06 22:27:12\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(21,'node1',174,NULL,NULL,'2015-09-06 20:38:10','2015-09-06 20:38:11','TERMINATED',0,'<workflow end_time=\"2015-09-06 22:38:11\" errors=\"0\" id=\"21\" name=\"test\" start_time=\"2015-09-06 22:38:10\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 22:38:10\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 22:38:10\" exit_time=\"2015-09-06 22:38:10\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(22,'node1',174,NULL,NULL,'2015-09-06 21:12:36','2015-09-06 21:12:37','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:37\" errors=\"0\" id=\"22\" name=\"test\" start_time=\"2015-09-06 23:12:36\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:37\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:37\" exit_time=\"2015-09-06 23:12:37\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(23,'node1',174,NULL,NULL,'2015-09-06 21:12:38','2015-09-06 21:12:38','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:38\" errors=\"0\" id=\"23\" name=\"test\" start_time=\"2015-09-06 23:12:38\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:38\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:38\" exit_time=\"2015-09-06 23:12:38\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(24,'node1',174,NULL,NULL,'2015-09-06 21:12:40','2015-09-06 21:12:40','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:40\" errors=\"0\" id=\"24\" name=\"test\" start_time=\"2015-09-06 23:12:40\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:40\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:40\" exit_time=\"2015-09-06 23:12:40\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(25,'node1',174,NULL,NULL,'2015-09-06 21:12:44','2015-09-06 21:12:45','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:45\" errors=\"0\" id=\"25\" name=\"test\" start_time=\"2015-09-06 23:12:44\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:45\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:45\" exit_time=\"2015-09-06 23:12:45\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(26,'node1',174,NULL,NULL,'2015-09-06 21:12:52','2015-09-06 21:12:52','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:52\" errors=\"0\" id=\"26\" name=\"test\" start_time=\"2015-09-06 23:12:52\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:52\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:52\" exit_time=\"2015-09-06 23:12:52\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(27,'node1',174,NULL,NULL,'2015-09-06 21:12:54','2015-09-06 21:12:54','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:54\" errors=\"0\" id=\"27\" name=\"test\" start_time=\"2015-09-06 23:12:54\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:54\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:54\" exit_time=\"2015-09-06 23:12:54\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(28,'node1',174,NULL,NULL,'2015-09-06 21:12:55','2015-09-06 21:12:55','TERMINATED',0,'<workflow end_time=\"2015-09-06 23:12:55\" errors=\"0\" id=\"28\" name=\"test\" start_time=\"2015-09-06 23:12:55\" status=\"TERMINATED\"><parameters/><subjobs><job><tasks><task execution_time=\"2015-09-06 23:12:55\" name=\"ls\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"filename\">/</input><output execution_time=\"2015-09-06 23:12:55\" exit_time=\"2015-09-06 23:12:55\" method=\"text\" retval=\"0\">bin\nboot\ncdrom\ndev\netc\nhome\ninitrd.img\ninitrd.img.old\nlib\nlib32\nlib64\nmedia\nmnt\nopt\nproc\nroot\nrun\nsbin\nsrv\nsys\ntmp\nusr\nvar\nvmlinuz\nvmlinuz.old\n</output></task></tasks></job></subjobs></workflow>'),(29,'node1',175,NULL,NULL,'2015-09-06 22:09:07','2015-09-06 22:09:13','TERMINATED',0,'<workflow end_time=\"2015-09-07 00:09:13\" errors=\"0\" id=\"29\" name=\"test_sleep\" start_time=\"2015-09-07 00:09:08\" status=\"TERMINATED\"><parameters><parameter name=\"seconds\">5</parameter></parameters><subjobs><job><tasks><task execution_time=\"2015-09-07 00:09:08\" name=\"sleep\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"duration\">5</input><output execution_time=\"2015-09-07 00:09:08\" exit_time=\"2015-09-07 00:09:13\" method=\"text\" retval=\"0\"></output></task></tasks></job></subjobs></workflow>'),(30,'node1',175,NULL,NULL,'2015-09-09 23:08:20','2015-09-09 23:08:25','TERMINATED',0,'<workflow end_time=\"2015-09-10 01:08:25\" errors=\"0\" id=\"30\" name=\"test_sleep\" start_time=\"2015-09-10 01:08:20\" status=\"TERMINATED\"><parameters><parameter name=\"seconds\">5</parameter></parameters><subjobs><job><tasks><task execution_time=\"2015-09-10 01:08:20\" name=\"sleep\" queue=\"default\" retval=\"0\" status=\"TERMINATED\"><input name=\"duration\">5</input><output execution_time=\"2015-09-10 01:08:20\" exit_time=\"2015-09-10 01:08:25\" method=\"text\" retval=\"0\"></output></task></tasks></job></subjobs></workflow>'),(31,'node2',175,NULL,NULL,'2015-09-09 23:09:13','2015-09-09 23:16:54','TERMINATED',1,'<workflow end_time=\"2015-09-10 01:16:54\" errors=\"1\" id=\"31\" name=\"test_sleep\" start_time=\"2015-09-10 01:09:13\" status=\"TERMINATED\"><parameters><parameter name=\"seconds\">5</parameter></parameters><subjobs><job><tasks><task execution_time=\"2015-09-10 01:09:13\" name=\"sleep\" queue=\"default\" retval=\"-1\" status=\"TERMINATED\"><input name=\"duration\">5</input><output execution_time=\"2015-09-10 01:09:13\" exit_time=\"2015-09-10 01:16:00\" method=\"text\" retval=\"-1\">Task migrated</output></task></tasks></job></subjobs></workflow>'),(32,'node2',175,NULL,NULL,'2015-09-09 23:18:20','2015-09-09 23:18:55','TERMINATED',1,'<workflow end_time=\"2015-09-10 01:18:55\" errors=\"1\" id=\"32\" name=\"test_sleep\" start_time=\"2015-09-10 01:18:20\" status=\"TERMINATED\"><parameters><parameter name=\"seconds\">5</parameter></parameters><subjobs><job><tasks><task execution_time=\"2015-09-10 01:18:20\" name=\"sleep\" queue=\"default\" retval=\"-1\" status=\"TERMINATED\"><input name=\"duration\">5</input><output execution_time=\"2015-09-10 01:18:20\" exit_time=\"2015-09-10 01:18:55\" method=\"text\" retval=\"-1\">Task migrated</output></task></tasks></job></subjobs></workflow>');
/*!40000 ALTER TABLE `t_workflow_instance` ENABLE KEYS */;
UNLOCK TABLES;

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
-- Dumping data for table `t_workflow_instance_parameters`
--

LOCK TABLES `t_workflow_instance_parameters` WRITE;
/*!40000 ALTER TABLE `t_workflow_instance_parameters` DISABLE KEYS */;
INSERT INTO `t_workflow_instance_parameters` VALUES (29,'seconds','5'),(30,'seconds','5'),(31,'seconds','5'),(32,'seconds','5');
/*!40000 ALTER TABLE `t_workflow_instance_parameters` ENABLE KEYS */;
UNLOCK TABLES;

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
-- Dumping data for table `t_workflow_notification`
--

LOCK TABLES `t_workflow_notification` WRITE;
/*!40000 ALTER TABLE `t_workflow_notification` DISABLE KEYS */;
INSERT INTO `t_workflow_notification` VALUES (174,0),(175,0);
/*!40000 ALTER TABLE `t_workflow_notification` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_workflow_schedule`
--

DROP TABLE IF EXISTS `t_workflow_schedule`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `t_workflow_schedule` (
  `workflow_schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
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
-- Dumping data for table `t_workflow_schedule`
--

LOCK TABLES `t_workflow_schedule` WRITE;
/*!40000 ALTER TABLE `t_workflow_schedule` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_workflow_schedule` ENABLE KEYS */;
UNLOCK TABLES;

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

--
-- Dumping data for table `t_workflow_schedule_parameters`
--

LOCK TABLES `t_workflow_schedule_parameters` WRITE;
/*!40000 ALTER TABLE `t_workflow_schedule_parameters` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_workflow_schedule_parameters` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-11-19 23:09:37
