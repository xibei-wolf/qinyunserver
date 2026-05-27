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
  `title` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '活动名称',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT '活动描述/注意事项',
  `location` varchar(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '活动地点',
  `organizer_id` int unsigned DEFAULT NULL COMMENT '发布人用户ID',
  `department_id` smallint unsigned DEFAULT NULL COMMENT '主办部门ID（NULL=全队活动）',
  `activity_week` tinyint unsigned NOT NULL COMMENT '活动所在教学周: 1-20',
  `time_mask` int unsigned NOT NULL COMMENT '当天15位时间掩码',
  `max_participants` smallint unsigned NOT NULL DEFAULT '0' COMMENT '最大参与人数，0=不限',
  `sign_deadline` datetime DEFAULT NULL COMMENT '报名截止时间',
  `status` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1:未开始(招募中), 2:进行中, 3:已结束结算, 4:已取消/释放',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `default_duration` decimal(3,1) NOT NULL DEFAULT '0.0' COMMENT '根据所选时段自动计算的默认标准工时(h)',
  `start_date` date NOT NULL COMMENT '活动开始日期',
  `end_date` date NOT NULL COMMENT '活动结束日期（若是单次活动则与start_date相同）',
  `start_time` time NOT NULL COMMENT '活动具体开始时间（如 14:30:00）',
  `end_time` time NOT NULL COMMENT '活动具体结束时间（如 16:30:00）',
  `period_type` int DEFAULT '0' COMMENT '0:单次活动, 1:每周重复, 2:双周重复, 3:每月重复',
  PRIMARY KEY (`id`),
  KEY `idx_organizer` (`organizer_id`),
  KEY `idx_department_week` (`department_id`,`activity_week`),
  KEY `idx_status_week` (`status`,`activity_week`),
  KEY `idx_week` (`activity_week`),
  CONSTRAINT `fk_activities_department` FOREIGN KEY (`department_id`) REFERENCES `departments` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_activities_organizer` FOREIGN KEY (`organizer_id`) REFERENCES `users` (`id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `chk_activity_week` CHECK (((`activity_week` >= 1) and (`activity_week` <= 20)))
) ENGINE=InnoDB AUTO_INCREMENT=35 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='活动信息表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `activities`
--

LOCK TABLES `activities` WRITE;
/*!40000 ALTER TABLE `activities` DISABLE KEYS */;
INSERT INTO `activities` VALUES (34,'无w','','无',1074,1,13,896,30,NULL,1,'2026-05-27 14:05:15','2026-05-27 14:05:15',3.0,'2026-05-27','2026-05-27','14:00:00','16:30:00',0);
/*!40000 ALTER TABLE `activities` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `activity_applications`
--

DROP TABLE IF EXISTS `activity_applications`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `activity_applications` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `activity_id` int unsigned NOT NULL,
  `user_id` int unsigned NOT NULL,
  `status` tinyint NOT NULL DEFAULT '0' COMMENT '0:待审批, 1:已录用, 2:已拒绝, 3:用户撤销',
  `apply_reason` varchar(255) COLLATE utf8mb4_general_ci DEFAULT NULL,
  `remark` varchar(255) COLLATE utf8mb4_general_ci DEFAULT NULL COMMENT '审批备注',
  `created_at` datetime DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_act_user` (`activity_id`,`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `activity_applications`
--

LOCK TABLES `activity_applications` WRITE;
/*!40000 ALTER TABLE `activity_applications` DISABLE KEYS */;
/*!40000 ALTER TABLE `activity_applications` ENABLE KEYS */;
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
  `remark` varchar(512) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '备注（如请假原因）',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_activity_user` (`activity_id`,`user_id`),
  KEY `idx_user_sign` (`user_id`,`sign_in_status`),
  KEY `idx_activity_sign` (`activity_id`,`sign_in_status`),
  CONSTRAINT `fk_am_activity` FOREIGN KEY (`activity_id`) REFERENCES `activities` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_am_user` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=77 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='活动排班录用表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `activity_members`
--

LOCK TABLES `activity_members` WRITE;
/*!40000 ALTER TABLE `activity_members` DISABLE KEYS */;
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
  `course_name` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '课程名称，如"高等数学"',
  `teacher_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '任课教师',
  `classroom` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '上课地点',
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
  `user_id` int unsigned DEFAULT NULL COMMENT '用户ID（公共课表模板此列为NULL）',
  `class_identifier` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '班级标识(若为公共课表则填班级名，个人课表留空)',
  `course_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '课程名称',
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
) ENGINE=InnoDB AUTO_INCREMENT=147 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='原始课表规则模板表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `course_templates`
--

LOCK TABLES `course_templates` WRITE;
/*!40000 ALTER TABLE `course_templates` DISABLE KEYS */;
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
  `name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '部门名称',
  `code` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '部门代号，如 planning / liaison / office / publicity / cloud_classroom',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT '部门职能描述',
  `sort_order` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '前端展示排序权重',
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `code` (`code`)
) ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='部门表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `departments`
--

LOCK TABLES `departments` WRITE;
/*!40000 ALTER TABLE `departments` DISABLE KEYS */;
INSERT INTO `departments` VALUES (0,'无部门','none','不属于任何部门',0,'2026-05-27 13:12:56','2026-05-27 13:13:51'),(1,'策划部','planning','负责活动策划、方案设计与审核',1,'2026-05-27 13:07:31','2026-05-27 13:11:33'),(2,'外联部','liaison','负责对外联络、资源对接与合作洽谈',2,'2026-05-27 13:07:31','2026-05-27 13:11:33'),(3,'办公室','office','负责行政事务、档案管理与物资统筹',3,'2026-05-27 13:07:31','2026-05-27 13:11:33'),(4,'宣传部','publicity','负责活动宣传、新媒体运营与物料设计',4,'2026-05-27 13:07:31','2026-05-27 13:11:33'),(5,'云教室','cloud_classroom','负责线上支教、课程录制与远程教学',5,'2026-05-27 13:07:31','2026-05-27 13:12:34'),(6,'待定','pending','暂未分配部门的人员',6,'2026-05-27 13:12:56','2026-05-27 13:12:56');
/*!40000 ALTER TABLE `departments` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `operation_logs`
--

DROP TABLE IF EXISTS `operation_logs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `operation_logs` (
  `id` int NOT NULL AUTO_INCREMENT,
  `user_id` int DEFAULT NULL,
  `action_type` varchar(50) COLLATE utf8mb4_general_ci DEFAULT NULL,
  `target_id` int DEFAULT NULL,
  `details` text COLLATE utf8mb4_general_ci,
  `created_at` datetime DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `operation_logs`
--

LOCK TABLES `operation_logs` WRITE;
/*!40000 ALTER TABLE `operation_logs` DISABLE KEYS */;
/*!40000 ALTER TABLE `operation_logs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `roles`
--

DROP TABLE IF EXISTS `roles`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `roles` (
  `id` smallint unsigned NOT NULL COMMENT '角色ID: 10=带队老师, 20=队长, 30=部长, 40=普通队员',
  `name` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '角色名称',
  `level` tinyint unsigned NOT NULL COMMENT '权限层级，数值越小权限越高（10 > 20 > 30 > 40）',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT '角色职责描述',
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
INSERT INTO `roles` VALUES (10,'带队老师',10,'系统最高权限，可查看所有部门和人员数据','{\"export_data\": true, \"auto_schedule\": true, \"view_all_dept\": true, \"audit_activity\": true, \"manage_all_users\": true, \"publish_activity\": true}','2026-05-27 13:08:50','2026-05-27 13:08:50'),(20,'队长',20,'全队统筹权限，活动发布和一键排班','{\"auto_schedule\": true, \"view_all_dept\": true, \"audit_activity\": true, \"publish_activity\": true, \"manage_team_members\": true}','2026-05-27 13:08:50','2026-05-27 13:08:50'),(30,'部长',30,'部门管理权限，管理本部门成员和排班','{\"view_own_dept\": true, \"view_dept_schedule\": true, \"manage_dept_members\": true, \"publish_dept_activity\": true}','2026-05-27 13:08:50','2026-05-27 13:08:50'),(40,'普通队员',40,'基础权限，维护个人信息和课表，查看被录用活动','{\"edit_profile\": true, \"apply_activity\": true, \"upload_schedule\": true, \"view_own_activities\": true}','2026-05-27 13:08:50','2026-05-27 13:08:50');
/*!40000 ALTER TABLE `roles` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `schedules`
--

DROP TABLE IF EXISTS `schedules`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `schedules` (
  `id` int NOT NULL AUTO_INCREMENT,
  `user_id` int NOT NULL,
  `week_number` int NOT NULL,
  `day_of_week` int NOT NULL,
  `day_bitmask` int unsigned NOT NULL,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_user_week_day` (`user_id`,`week_number`,`day_of_week`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `schedules`
--

LOCK TABLES `schedules` WRITE;
/*!40000 ALTER TABLE `schedules` DISABLE KEYS */;
/*!40000 ALTER TABLE `schedules` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_config`
--

DROP TABLE IF EXISTS `sys_config`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_config` (
  `config_key` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `config_value` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`config_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_config`
--

LOCK TABLES `sys_config` WRITE;
/*!40000 ALTER TABLE `sys_config` DISABLE KEYS */;
INSERT INTO `sys_config` VALUES ('term_start_date','2026-03-01','2026-05-27 06:04:57');
/*!40000 ALTER TABLE `sys_config` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `users`
--

DROP TABLE IF EXISTS `users`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `users` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `student_id` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '学号，唯一标识',
  `name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '真实姓名',
  `gender` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '性别: 0=未知, 1=男, 2=女',
  `password_hash` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'BCrypt 密码哈希',
  `phone` varchar(20) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '手机号',
  `email` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '邮箱',
  `college` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '信息学院',
  `major` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '专业，如：软件工程',
  `class_name` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '完整班级，如：软件工程2303',
  `avatar_url` varchar(512) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '头像地址',
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
) ENGINE=InnoDB AUTO_INCREMENT=1077 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `users`
--

LOCK TABLES `users` WRITE;
/*!40000 ALTER TABLE `users` DISABLE KEYS */;
INSERT INTO `users` VALUES (1056,'2331051707','测试队长',1,'123456',NULL,NULL,'信息学院','软件工程','软件工程2303',NULL,3,20,1,NULL,'2026-05-27 13:24:07','2026-05-27 13:24:07'),(1057,'2025001','测试成员1',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2501',NULL,1,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1058,'2025002','测试成员2',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2502',NULL,2,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1059,'2025003','测试成员3',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2503',NULL,3,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1060,'2025004','测试成员4',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2504',NULL,4,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1061,'2025005','测试成员5',0,'123456',NULL,NULL,'信息学院','软件工程','软件工程2501',NULL,5,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1062,'2025006','测试成员6',0,'123456',NULL,NULL,'信息学院','软件工程','软件工程2502',NULL,1,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1063,'2025007','测试成员7',0,'123456',NULL,NULL,'信息学院','软件工程','软件工程2503',NULL,2,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1064,'2025008','测试成员8',0,'123456',NULL,NULL,'信息学院','软件工程','软件工程2504',NULL,3,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1065,'2025009','测试成员9',0,'123456',NULL,NULL,'信息学院','网络工程','网络工程2501',NULL,4,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1066,'2025010','测试成员10',0,'123456',NULL,NULL,'信息学院','网络工程','网络工程2502',NULL,5,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1067,'2025011','测试成员11',0,'123456',NULL,NULL,'信息学院','电子商务','电子商务2501',NULL,1,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1068,'2025012','测试成员12',0,'123456',NULL,NULL,'信息学院','电子商务','电子商务2502',NULL,2,40,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1069,'2024001','部长1',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2401',NULL,1,30,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1070,'2024002','部长2',0,'123456',NULL,NULL,'信息学院','软件工程','软件工程2402',NULL,2,30,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1071,'2024003','部长3',0,'123456',NULL,NULL,'信息学院','网络工程','网络工程2401',NULL,3,30,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1072,'2024004','部长4',0,'123456',NULL,NULL,'信息学院','电子商务','电子商务2402',NULL,4,30,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1073,'2024005','部长5',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2404',NULL,5,30,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1074,'2023001','队长1',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2304',NULL,1,20,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1075,'2023002','队长2',0,'123456',NULL,NULL,'信息学院','软件工程','软件工程2303',NULL,1,20,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50'),(1076,'2023003','队长3',0,'123456',NULL,NULL,'信息学院','计算机科学与技术','计算机科学与技术2302',NULL,1,20,1,NULL,'2026-05-27 13:31:50','2026-05-27 13:31:50');
/*!40000 ALTER TABLE `users` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Temporary view structure for view `view_major_class_stats`
--

DROP TABLE IF EXISTS `view_major_class_stats`;
/*!50001 DROP VIEW IF EXISTS `view_major_class_stats`*/;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `view_major_class_stats` AS SELECT 
 1 AS `major`,
 1 AS `class_name`,
 1 AS `student_count`*/;
SET character_set_client = @saved_cs_client;

--
-- Temporary view structure for view `view_member_service_stats`
--

DROP TABLE IF EXISTS `view_member_service_stats`;
/*!50001 DROP VIEW IF EXISTS `view_member_service_stats`*/;
SET @saved_cs_client     = @@character_set_client;
/*!50503 SET character_set_client = utf8mb4 */;
/*!50001 CREATE VIEW `view_member_service_stats` AS SELECT 
 1 AS `user_id`,
 1 AS `total_hours`,
 1 AS `activity_count`*/;
SET character_set_client = @saved_cs_client;

--
-- Final view structure for view `view_major_class_stats`
--

/*!50001 DROP VIEW IF EXISTS `view_major_class_stats`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `view_major_class_stats` AS select `users`.`major` AS `major`,`users`.`class_name` AS `class_name`,count(0) AS `student_count` from `users` where ((`users`.`class_name` is not null) and (`users`.`class_name` <> '')) group by `users`.`major`,`users`.`class_name` order by `users`.`major`,`users`.`class_name` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;

--
-- Final view structure for view `view_member_service_stats`
--

/*!50001 DROP VIEW IF EXISTS `view_member_service_stats`*/;
/*!50001 SET @saved_cs_client          = @@character_set_client */;
/*!50001 SET @saved_cs_results         = @@character_set_results */;
/*!50001 SET @saved_col_connection     = @@collation_connection */;
/*!50001 SET character_set_client      = utf8mb4 */;
/*!50001 SET character_set_results     = utf8mb4 */;
/*!50001 SET collation_connection      = utf8mb4_0900_ai_ci */;
/*!50001 CREATE ALGORITHM=UNDEFINED */
/*!50013 DEFINER=`root`@`localhost` SQL SECURITY DEFINER */
/*!50001 VIEW `view_member_service_stats` AS select `activity_members`.`user_id` AS `user_id`,sum(`activity_members`.`duration_hours`) AS `total_hours`,count(0) AS `activity_count` from `activity_members` where (`activity_members`.`is_attended` = 1) group by `activity_members`.`user_id` */;
/*!50001 SET character_set_client      = @saved_cs_client */;
/*!50001 SET character_set_results     = @saved_cs_results */;
/*!50001 SET collation_connection      = @saved_col_connection */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-05-27 17:30:56
