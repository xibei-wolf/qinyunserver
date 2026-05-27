cmake_minimum_required(VERSION 3.10)
project(myserver)
set(CMAKE_CXX_STANDARD 17)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#include_directories(${PROJECT_SOURCE_DIR}/LogSys/include)
#aux_source_directory(${PROJECT_SOURCE_DIR}/LogSys/src SRC_LOG)

include_directories(${PROJECT_SOURCE_DIR}/../../net/include)
include_directories(${PROJECT_SOURCE_DIR}/utils)
include_directories(${PROJECT_SOURCE_DIR})

aux_source_directory(${PROJECT_SOURCE_DIR}/../../net/src SRC_SERVER)
aux_source_directory(${PROJECT_SOURCE_DIR}/utils SRC_UTILS)
aux_source_directory(${PROJECT_SOURCE_DIR}  SRC_QYSERVER)

add_executable(main main.cpp ${SRC_SERVER} ${SRC_QYSERVER} ${SRC_UTILS})
# 1. 帮编译器在 Ubuntu 系统路径中搜寻 libmysqlclient.so 的真实物理路径
find_library(MYSQL_LIB NAMES mysqlclient)

if(NOT MYSQL_LIB)
    message(FATAL_ERROR "❌ 找不到 MySQL 客户端开发库！请先在虚拟机运行: sudo apt-get install libmysqlclient-dev")
endif()

# 2. 将搜寻到的安全路径，绑定到你的主程序目标上（假设你的可执行程序名叫 main）
target_link_libraries(main 
    PRIVATE 
        ${MYSQL_LIB}   ## 👈 完美注入系统检测到的绝对路径，彻底杜绝链接期未定义引用报错
)



