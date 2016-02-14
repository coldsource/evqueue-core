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
  `log_timestamp` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00' ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`log_id`)
) ENGINE=InnoDB AUTO_INCREMENT=4409 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
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
) ENGINE=InnoDB AUTO_INCREMENT=24 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
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
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
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
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
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
) ENGINE=InnoDB AUTO_INCREMENT=142 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_task`
--

LOCK TABLES `t_task` WRITE;
/*!40000 ALTER TABLE `t_task` DISABLE KEYS */;
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
) ENGINE=InnoDB AUTO_INCREMENT=174 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_workflow`
--

LOCK TABLES `t_workflow` WRITE;
/*!40000 ALTER TABLE `t_workflow` DISABLE KEYS */;
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
) ENGINE=InnoDB AUTO_INCREMENT=19 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_workflow_instance`
--

LOCK TABLES `t_workflow_instance` WRITE;
/*!40000 ALTER TABLE `t_workflow_instance` DISABLE KEYS */;
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
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '',
  `workflow_id` int(10) unsigned NOT NULL,
  `workflow_schedule` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `workflow_schedule_onfailure` enum('CONTINUE','SUSPEND') COLLATE utf8_unicode_ci NOT NULL,
  `workflow_schedule_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL,
  `workflow_schedule_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL,
  `workflow_schedule_active` tinyint(4) NOT NULL,
  `workflow_schedule_comment` text COLLATE utf8_unicode_ci NOT NULL,
  PRIMARY KEY (`workflow_schedule_id`)
) ENGINE=InnoDB AUTO_INCREMENT=47 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v1.5';
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

-- Dump completed on 2016-02-13 19:03:10
