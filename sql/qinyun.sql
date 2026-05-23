-- MySQL dump 10.13  Distrib 8.0.45, for Linux (x86_64)
--
-- Host: localhost    Database: qinyun
-- ------------------------------------------------------
-- Server version	8.0.45-0ubuntu0.24.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `activities`
--

DROP TABLE IF EXISTS `activities`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `activities` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `title` varchar(256) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '活动名称',
  `description` text COLLATE utf8mb4_unicode_ci COMMENT '活动描述/注意事项',
  `location` varchar(256) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '活动地点',
  `organizer_id` int unsigned NOT NULL COMMENT '发布人用户ID',
  `department_id` smallint unsigned DEFAULT NULL COMMENT '主办部门ID（NULL=全队活动）',
  `activity_week` tinyint unsigned NOT NULL COMMENT '活动所在教学周: 1-20',
  `time_mask` int unsigned NOT NULL COMMENT '活动占用时段位图: 1=该时段需要人手, 0=不需要',
  `max_participants` smallint unsigned NOT NULL DEFAULT '0' COMMENT '最大参与人数，0=不限',
  `sign_deadline` datetime DEFAULT NULL COMMENT '报名截止时间',
  `status` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1:未开始(招募中), 2:进行中, 3:已结束结算, 4:已取消/释放',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `default_duration` decimal(3,1) NOT NULL DEFAULT '0.0' COMMENT '根据所选时段自动计算的默认标准工时(h)',
  PRIMARY KEY (`id`),
  KEY `idx_organizer` (`organizer_id`),
  KEY `idx_department_week` (`department_id`,`activity_week`),
  KEY `idx_status_week` (`status`,`activity_week`),
  KEY `idx_week` (`activity_week`),
  CONSTRAINT `fk_activities_department` FOREIGN KEY (`department_id`) REFERENCES `departments` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_activities_organizer` FOREIGN KEY (`organizer_id`) REFERENCES `users` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE,
  CONSTRAINT `chk_activity_week` CHECK (((`activity_week` >= 1) and (`activity_week` <= 20)))
) ENGINE=InnoDB AUTO_INCREMENT=26 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='活动信息表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `activities`
--

LOCK TABLES `activities` WRITE;
/*!40000 ALTER TABLE `activities` DISABLE KEYS */;
INSERT INTO `activities` VALUES (3,'周四第3-5节 外联会议','外联部赞助商对接及方案研讨会','服务队办公室',1001,2,12,917504,15,NULL,1,'2026-05-22 18:15:29','2026-05-22 18:15:29',0.0),(8,'周三 曙光村小学支教','','曙光村小学',1001,1,12,4096,30,NULL,1,'2026-05-23 10:12:36','2026-05-23 10:12:36',0.0),(17,'部长任务','','线上',1011,1,10,4096,30,NULL,3,'2026-05-23 12:07:39','2026-05-23 14:03:46',0.0),(21,'周三测试','','线上',1009,1,13,4096,30,NULL,1,'2026-05-23 14:17:49','2026-05-23 14:17:49',0.0);
/*!40000 ALTER TABLE `activities` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `activity_members`
--

DROP TABLE IF EXISTS `activity_members`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `activity_members` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `activity_id` int unsigned NOT NULL COMMENT '活动ID',
  `user_id` int unsigned NOT NULL COMMENT '队员用户ID',
  `assign_type` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '录用方式: 0=手动指定, 1=智能排班, 2=自主报名, 3=替补递补',
  `sign_in_status` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '签到状态: 0=未签到, 1=已签到, 2=迟到, 3=请假',
  `duration_hours` decimal(4,1) DEFAULT '0.0',
  `is_attended` tinyint DEFAULT '0',
  `sign_in_time` datetime DEFAULT NULL COMMENT '签到时间',
  `remark` varchar(512) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '备注（如请假原因）',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_activity_user` (`activity_id`,`user_id`),
  KEY `idx_user_sign` (`user_id`,`sign_in_status`),
  KEY `idx_activity_sign` (`activity_id`,`sign_in_status`),
  CONSTRAINT `fk_am_activity` FOREIGN KEY (`activity_id`) REFERENCES `activities` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_am_user` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=71 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='活动排班录用表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `activity_members`
--

LOCK TABLES `activity_members` WRITE;
/*!40000 ALTER TABLE `activity_members` DISABLE KEYS */;
INSERT INTO `activity_members` VALUES (59,17,1003,0,1,2.0,1,NULL,NULL,'2026-05-23 12:07:46','2026-05-23 14:03:46'),(60,17,1005,1,1,2.0,1,NULL,NULL,'2026-05-23 12:07:46','2026-05-23 14:03:46'),(61,17,1006,1,1,2.0,1,NULL,NULL,'2026-05-23 12:07:46','2026-05-23 14:03:46'),(62,17,1007,1,1,2.0,1,NULL,NULL,'2026-05-23 12:07:46','2026-05-23 14:03:46'),(63,17,1008,1,1,2.0,1,NULL,NULL,'2026-05-23 12:07:46','2026-05-23 14:03:46'),(66,3,1003,0,0,0.0,0,NULL,NULL,'2026-05-23 13:23:24','2026-05-23 13:23:24'),(67,8,1003,0,0,0.0,0,NULL,NULL,'2026-05-23 13:23:25','2026-05-23 13:23:25');
/*!40000 ALTER TABLE `activity_members` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `course_records`
--

DROP TABLE IF EXISTS `course_records`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `course_records` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `user_id` int unsigned NOT NULL COMMENT '所属用户',
  `course_name` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '课程名称，如"高等数学"',
  `teacher_name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '任课教师',
  `classroom` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '上课地点',
  `day_of_week` tinyint unsigned NOT NULL COMMENT '星期几: 1=周一, 2=周二, 3=周三, 4=周四, 5=周五',
  `period_start` tinyint unsigned NOT NULL COMMENT '开始大节号: 1-5',
  `period_count` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '连续大节数: 通常1或2',
  `start_week` tinyint unsigned NOT NULL COMMENT '起始教学周: 1-20',
  `end_week` tinyint unsigned NOT NULL COMMENT '结束教学周: 1-20',
  `week_type` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '单双周类型: 0=每周, 1=仅单周, 2=仅双周',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_user_id` (`user_id`),
  KEY `idx_day_period` (`day_of_week`,`period_start`),
  KEY `idx_week_range` (`start_week`,`end_week`),
  CONSTRAINT `fk_course_records_user` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `chk_day_range` CHECK (((`day_of_week` >= 1) and (`day_of_week` <= 5))),
  CONSTRAINT `chk_period_range` CHECK (((`period_start` >= 1) and (`period_start` <= 5))),
  CONSTRAINT `chk_week_range` CHECK (((`start_week` >= 1) and (`start_week` <= 20) and (`end_week` >= `start_week`) and (`end_week` <= 20)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='课程记录表（结构化存储，供QML展示课程详情）';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `course_records`
--

LOCK TABLES `course_records` WRITE;
/*!40000 ALTER TABLE `course_records` DISABLE KEYS */;
/*!40000 ALTER TABLE `course_records` ENABLE KEYS */;
UNLOCK TABLES;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `trg_course_records_after_insert` AFTER INSERT ON `course_records` FOR EACH ROW BEGIN
    CALL sp_recompute_user_schedule(NEW.user_id, 20);
END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `trg_course_records_after_update` AFTER UPDATE ON `course_records` FOR EACH ROW BEGIN
    CALL sp_recompute_user_schedule(NEW.user_id, 20);
END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `trg_course_records_after_delete` AFTER DELETE ON `course_records` FOR EACH ROW BEGIN
    CALL sp_recompute_user_schedule(OLD.user_id, 20);
END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;

--
-- Table structure for table `course_templates`
--

DROP TABLE IF EXISTS `course_templates`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `course_templates` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `user_id` int unsigned NOT NULL COMMENT '用户ID',
  `class_identifier` varchar(64) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '班级标识(若为公共课表则填班级名，个人课表留空)',
  `course_name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '课程名称',
  `day_of_week` tinyint unsigned NOT NULL COMMENT '星期几: 1-5',
  `period` tinyint unsigned NOT NULL COMMENT '标准大节: 1-6',
  `start_week` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '起始周: 1',
  `end_week` tinyint unsigned NOT NULL DEFAULT '16' COMMENT '结束周: 16',
  `week_type` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '周类型: 0=每周都上, 1=仅单周, 2=仅双周',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `fk_templates_user` (`user_id`),
  KEY `idx_class_template` (`class_identifier`),
  CONSTRAINT `fk_templates_user` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=79 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='原始课表规则模板表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `course_templates`
--

LOCK TABLES `course_templates` WRITE;
/*!40000 ALTER TABLE `course_templates` DISABLE KEYS */;
INSERT INTO `course_templates` VALUES (1,1009,NULL,'体育',3,1,1,16,0,'2026-05-23 12:32:53'),(2,1009,NULL,'高等数学',1,2,1,16,0,'2026-05-23 12:32:53'),(3,1009,NULL,'UML',5,2,1,16,1,'2026-05-23 12:32:53'),(4,1009,NULL,'java',3,4,1,8,0,'2026-05-23 12:32:53'),(19,1003,NULL,'有课',3,1,1,16,0,'2026-05-23 13:24:03'),(20,1003,NULL,'有课',4,2,1,16,0,'2026-05-23 13:24:03'),(21,1003,NULL,'有课',4,3,1,16,0,'2026-05-23 13:24:03'),(22,1003,NULL,'有课',1,4,1,16,0,'2026-05-23 13:24:03'),(23,1003,NULL,'有课',5,5,1,16,0,'2026-05-23 13:24:03'),(41,1009,'软件工程2303','有课',4,1,1,16,0,'2026-05-23 13:37:20'),(42,1009,'软件工程2303','有课',3,2,1,16,0,'2026-05-23 13:37:20'),(43,1009,'软件工程2303','有课',2,3,1,16,0,'2026-05-23 13:37:20'),(44,1009,'软件工程2303','有课',1,4,1,16,0,'2026-05-23 13:37:20'),(45,1009,'软件工程2303','有课',2,5,1,16,0,'2026-05-23 13:37:20'),(46,1009,'软件工程2303','有课',3,6,1,16,0,'2026-05-23 13:37:20'),(52,1002,NULL,'有课',4,1,1,16,0,'2026-05-23 14:18:32'),(53,1002,NULL,'有课',3,2,1,16,0,'2026-05-23 14:18:32'),(54,1002,NULL,'有课',2,3,1,16,0,'2026-05-23 14:18:32'),(55,1002,NULL,'有课',1,4,1,16,0,'2026-05-23 14:18:32'),(56,1002,NULL,'有课',2,5,1,16,0,'2026-05-23 14:18:32'),(57,1002,NULL,'有课',3,6,1,16,0,'2026-05-23 14:18:32'),(58,1009,'计算机科学与技术2301','有课',2,1,1,16,0,'2026-05-23 14:50:32'),(59,1009,'计算机科学与技术2301','有课',4,1,1,16,0,'2026-05-23 14:50:32'),(60,1009,'计算机科学与技术2301','有课',2,2,1,16,0,'2026-05-23 14:50:32'),(61,1009,'计算机科学与技术2301','有课',4,2,1,16,0,'2026-05-23 14:50:32'),(62,1009,'计算机科学与技术2301','有课',1,4,1,16,0,'2026-05-23 14:50:32'),(63,1009,'计算机科学与技术2301','有课',5,4,1,16,0,'2026-05-23 14:50:32'),(64,1009,'计算机科学与技术2302','有课',3,1,1,16,0,'2026-05-23 15:09:31'),(65,1009,'计算机科学与技术2302','有课',4,3,1,16,0,'2026-05-23 15:09:31'),(66,1009,'计算机科学与技术2302','有课',5,4,1,16,0,'2026-05-23 15:09:31'),(67,1009,'计算机科学与技术2302','有课',1,5,1,16,0,'2026-05-23 15:09:31'),(68,1009,'网络工程2301','有课',4,2,1,16,0,'2026-05-23 15:09:42'),(69,1009,'网络工程2301','有课',1,4,1,16,0,'2026-05-23 15:09:42'),(70,1009,'电子商务2301','有课',4,1,1,16,0,'2026-05-23 15:09:48'),(71,1009,'电子商务2301','有课',4,2,1,16,0,'2026-05-23 15:09:48'),(72,1009,'电子商务2301','有课',4,3,1,16,0,'2026-05-23 15:09:48'),(73,1009,'电子商务2301','有课',4,4,1,16,0,'2026-05-23 15:09:48'),(74,1009,'电子商务2301','有课',4,5,1,16,0,'2026-05-23 15:09:48'),(75,1004,NULL,'有课',4,1,1,16,0,'2026-05-23 15:10:54'),(76,1004,NULL,'有课',4,2,1,16,0,'2026-05-23 15:10:54'),(77,1004,NULL,'有课',4,3,1,16,0,'2026-05-23 15:10:54'),(78,1004,NULL,'有课',4,4,1,16,0,'2026-05-23 15:10:54');
/*!40000 ALTER TABLE `course_templates` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `departments`
--

DROP TABLE IF EXISTS `departments`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `departments` (
  `id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '部门名称',
  `code` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '部门代号，如 planning / liaison / office / publicity / cloud_classroom',
  `description` text COLLATE utf8mb4_unicode_ci COMMENT '部门职能描述',
  `sort_order` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '前端展示排序权重',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `code` (`code`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='部门表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `departments`
--

LOCK TABLES `departments` WRITE;
/*!40000 ALTER TABLE `departments` DISABLE KEYS */;
INSERT INTO `departments` VALUES (1,'策划部','planning','负责活动策划、方案设计与审核',1,'2026-05-22 15:23:17','2026-05-22 15:23:17'),(2,'外联部','liaison','负责对外联络、资源对接与合作洽谈',2,'2026-05-22 15:23:17','2026-05-22 15:23:17'),(3,'办公室','office','负责行政事务、档案管理与物资统筹',3,'2026-05-22 15:23:17','2026-05-22 15:23:17'),(4,'宣传部','publicity','负责活动宣传、新媒体运营与物料设计',4,'2026-05-22 15:23:17','2026-05-22 15:23:17'),(5,'云教室','cloud_classroom','负责线上支教、课程录制与远程教学',5,'2026-05-22 15:23:17','2026-05-22 15:23:17');
/*!40000 ALTER TABLE `departments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `roles`
--

DROP TABLE IF EXISTS `roles`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `roles` (
  `id` smallint unsigned NOT NULL COMMENT '角色ID: 10=带队老师, 20=队长, 30=部长, 40=普通队员',
  `name` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '角色名称',
  `level` tinyint unsigned NOT NULL COMMENT '权限层级，数值越小权限越高（10 > 20 > 30 > 40）',
  `description` text COLLATE utf8mb4_unicode_ci COMMENT '角色职责描述',
  `permissions` json NOT NULL COMMENT '细粒度权限位标记，如 {"view_all_dept":true,"publish_activity":true}',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `level` (`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='角色权限表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `roles`
--

LOCK TABLES `roles` WRITE;
/*!40000 ALTER TABLE `roles` DISABLE KEYS */;
INSERT INTO `roles` VALUES (10,'带队老师',10,'系统最高权限，可查看所有部门和人员数据','{\"export_data\": true, \"auto_schedule\": true, \"view_all_dept\": true, \"audit_activity\": true, \"manage_all_users\": true, \"publish_activity\": true}','2026-05-22 15:23:17','2026-05-22 15:23:17'),(20,'队长',20,'全队统筹权限，活动发布和一键排班','{\"auto_schedule\": true, \"view_all_dept\": true, \"audit_activity\": true, \"publish_activity\": true, \"manage_team_members\": true}','2026-05-22 15:23:17','2026-05-22 15:23:17'),(30,'部长',30,'部门管理权限，管理本部门成员和排班','{\"view_own_dept\": true, \"view_dept_schedule\": true, \"manage_dept_members\": true, \"publish_dept_activity\": true}','2026-05-22 15:23:17','2026-05-22 15:23:17'),(40,'普通队员',40,'基础权限，维护个人信息和课表，查看被录用活动','{\"edit_profile\": true, \"apply_activity\": true, \"upload_schedule\": true, \"view_own_activities\": true}','2026-05-22 15:23:17','2026-05-22 15:23:17');
/*!40000 ALTER TABLE `roles` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `schedules`
--

DROP TABLE IF EXISTS `schedules`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `schedules` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `user_id` int unsigned NOT NULL COMMENT '用户ID',
  `week_number` tinyint unsigned NOT NULL COMMENT '具体教学周: 1-16',
  `bitmask` int unsigned NOT NULL DEFAULT '0' COMMENT '32位空闲位图: 1=有课忙碌, 0=空闲',
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_user_week` (`user_id`,`week_number`),
  KEY `idx_query_perf` (`week_number`,`bitmask`)
) ENGINE=InnoDB AUTO_INCREMENT=113 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='课表周次展开缓存表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `schedules`
--

LOCK TABLES `schedules` WRITE;
/*!40000 ALTER TABLE `schedules` DISABLE KEYS */;
INSERT INTO `schedules` VALUES (1,1009,1,33591298,'2026-05-23 12:32:53'),(2,1009,2,36866,'2026-05-23 12:32:53'),(3,1009,3,33591298,'2026-05-23 12:32:53'),(4,1009,4,36866,'2026-05-23 12:32:53'),(5,1009,5,33591298,'2026-05-23 12:32:53'),(6,1009,6,36866,'2026-05-23 12:32:53'),(7,1009,7,33591298,'2026-05-23 12:32:53'),(8,1009,8,36866,'2026-05-23 12:32:53'),(9,1009,9,33558530,'2026-05-23 12:32:53'),(10,1009,10,4098,'2026-05-23 12:32:53'),(11,1009,11,33558530,'2026-05-23 12:32:53'),(12,1009,12,4098,'2026-05-23 12:32:53'),(13,1009,13,33558530,'2026-05-23 12:32:53'),(14,1009,14,4098,'2026-05-23 12:32:53'),(15,1009,15,33558530,'2026-05-23 12:32:53'),(16,1009,16,4098,'2026-05-23 12:32:53'),(65,1003,1,270012424,'2026-05-23 13:24:03'),(66,1003,2,270012424,'2026-05-23 13:24:03'),(67,1003,3,270012424,'2026-05-23 13:24:03'),(68,1003,4,270012424,'2026-05-23 13:24:03'),(69,1003,5,270012424,'2026-05-23 13:24:03'),(70,1003,6,270012424,'2026-05-23 13:24:03'),(71,1003,7,270012424,'2026-05-23 13:24:03'),(72,1003,8,270012424,'2026-05-23 13:24:03'),(73,1003,9,270012424,'2026-05-23 13:24:03'),(74,1003,10,270012424,'2026-05-23 13:24:03'),(75,1003,11,270012424,'2026-05-23 13:24:03'),(76,1003,12,270012424,'2026-05-23 13:24:03'),(77,1003,13,270012424,'2026-05-23 13:24:03'),(78,1003,14,270012424,'2026-05-23 13:24:03'),(79,1003,15,270012424,'2026-05-23 13:24:03'),(80,1003,16,270012424,'2026-05-23 13:24:03'),(81,1002,1,402696,'2026-05-23 14:18:32'),(82,1002,2,402696,'2026-05-23 14:18:32'),(83,1002,3,402696,'2026-05-23 14:18:32'),(84,1002,4,402696,'2026-05-23 14:18:32'),(85,1002,5,402696,'2026-05-23 14:18:32'),(86,1002,6,402696,'2026-05-23 14:18:32'),(87,1002,7,402696,'2026-05-23 14:18:32'),(88,1002,8,402696,'2026-05-23 14:18:32'),(89,1002,9,402696,'2026-05-23 14:18:32'),(90,1002,10,402696,'2026-05-23 14:18:32'),(91,1002,11,402696,'2026-05-23 14:18:32'),(92,1002,12,402696,'2026-05-23 14:18:32'),(93,1002,13,402696,'2026-05-23 14:18:32'),(94,1002,14,402696,'2026-05-23 14:18:32'),(95,1002,15,402696,'2026-05-23 14:18:32'),(96,1002,16,402696,'2026-05-23 14:18:32'),(97,1004,1,7077888,'2026-05-23 15:10:54'),(98,1004,2,7077888,'2026-05-23 15:10:54'),(99,1004,3,7077888,'2026-05-23 15:10:54'),(100,1004,4,7077888,'2026-05-23 15:10:54'),(101,1004,5,7077888,'2026-05-23 15:10:54'),(102,1004,6,7077888,'2026-05-23 15:10:54'),(103,1004,7,7077888,'2026-05-23 15:10:54'),(104,1004,8,7077888,'2026-05-23 15:10:54'),(105,1004,9,7077888,'2026-05-23 15:10:54'),(106,1004,10,7077888,'2026-05-23 15:10:54'),(107,1004,11,7077888,'2026-05-23 15:10:54'),(108,1004,12,7077888,'2026-05-23 15:10:54'),(109,1004,13,7077888,'2026-05-23 15:10:54'),(110,1004,14,7077888,'2026-05-23 15:10:54'),(111,1004,15,7077888,'2026-05-23 15:10:54'),(112,1004,16,7077888,'2026-05-23 15:10:54');
/*!40000 ALTER TABLE `schedules` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `users`
--

DROP TABLE IF EXISTS `users`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `users` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `student_id` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '学号，唯一标识',
  `name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '真实姓名',
  `password_hash` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'BCrypt 密码哈希',
  `phone` varchar(20) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '手机号',
  `email` varchar(128) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '邮箱',
  `college` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '信息学院',
  `major` varchar(64) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '专业，如：软件工程',
  `class_name` varchar(64) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '完整班级，如：软件工程2303',
  `avatar_url` varchar(512) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '头像地址',
  `department_id` smallint unsigned DEFAULT NULL COMMENT '所属部门ID（带队老师、队长可为 NULL）',
  `role_id` smallint unsigned NOT NULL COMMENT '角色ID，决定权限层级',
  `status` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '状态: 1=正常, 0=禁用, 2=已毕业',
  `last_login_at` datetime DEFAULT NULL COMMENT '最后登录时间',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_student_id` (`student_id`),
  KEY `idx_department` (`department_id`),
  KEY `idx_role` (`role_id`),
  KEY `idx_status` (`status`),
  KEY `idx_phone` (`phone`),
  KEY `idx_college_major_class` (`college`,`major`,`class_name`),
  KEY `idx_class` (`class_name`),
  CONSTRAINT `fk_users_department` FOREIGN KEY (`department_id`) REFERENCES `departments` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_users_role` FOREIGN KEY (`role_id`) REFERENCES `roles` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=1034 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `users`
--

LOCK TABLES `users` WRITE;
/*!40000 ALTER TABLE `users` DISABLE KEYS */;
INSERT INTO `users` VALUES (1001,'2023001','西北孤傲的狼','my_secure_password',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2301',NULL,3,20,1,NULL,'2026-05-22 15:25:24','2026-05-22 21:42:44'),(1002,'2023002','伏特加队员','my_secure_password',NULL,NULL,'信息学院','软件工程','软件工程2303',NULL,3,40,1,NULL,'2026-05-22 15:25:24','2026-05-22 21:24:57'),(1003,'2023003','波本队员','123456',NULL,NULL,'信息学院','软件工程','软件工程2303',NULL,1,40,1,NULL,'2026-05-22 17:36:06','2026-05-22 21:24:57'),(1004,'2023004','雪莉队员','123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2303',NULL,2,40,1,NULL,'2026-05-22 17:36:06','2026-05-22 21:42:44'),(1005,'2023005','基尔队员','123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2304',NULL,3,40,1,NULL,'2026-05-22 17:36:06','2026-05-22 21:42:44'),(1006,'2023006','琴酒队员','123456',NULL,NULL,'信息学院','网络工程','网络工程2301',NULL,4,40,1,NULL,'2026-05-22 17:36:06','2026-05-22 21:24:57'),(1007,'2023007','伏特加','123456',NULL,NULL,'信息学院','网络工程','网络工程2302',NULL,5,40,1,NULL,'2026-05-22 17:36:06','2026-05-22 21:24:57'),(1008,'2023008','贝尔摩德','123456',NULL,NULL,'信息学院','电子商务','电子商务2301',NULL,3,40,1,NULL,'2026-05-22 17:36:06','2026-05-22 21:24:57'),(1009,'2023009','柯南老师','123456',NULL,NULL,'信息学院','软件工程','软件工程2301',NULL,3,10,1,NULL,'2026-05-22 18:52:05','2026-05-22 21:24:57'),(1010,'2023010','目暮队长','123456',NULL,NULL,'信息学院','软件工程','软件工程2302',NULL,3,20,1,NULL,'2026-05-22 18:52:05','2026-05-22 21:24:57'),(1011,'2023011','赤井部长','123456',NULL,NULL,'信息学院','软件工程','软件工程2303',NULL,1,30,1,NULL,'2026-05-22 18:52:05','2026-05-22 21:24:57'),(1012,'2023012','安室部长','123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2302',NULL,3,30,1,NULL,'2026-05-22 18:52:05','2026-05-22 21:42:44');
/*!40000 ALTER TABLE `users` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-05-23 17:22:11
