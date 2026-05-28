# 收集 server 自己的源文件（根据你的实际目录）
file(GLOB_RECURSE SERVER_SRC 
    "*.cpp"                 # 包含 main.cpp
    "business/*.cpp"
    "utils/*.cpp"
)

# 生成可执行文件
add_executable(main ${SERVER_SRC})

# 链接 net 库
target_link_libraries(main PRIVATE net)

# 添加额外的头文件路径（如果 net 的 include 已被 net 目标公开，此处不需要）
# 但 server 内部 utils 等头文件需要自己包含
target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/business
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    # net 的头文件通过链接 net 库自动获得，无需再写
)

# 查找并链接 MySQL 客户端库
find_library(MYSQL_LIB NAMES mysqlclient)
if(NOT MYSQL_LIB)
    message(FATAL_ERROR "❌ 找不到 MySQL 库，请安装: sudo apt install libmysqlclient-dev")
endif()
target_link_libraries(main PRIVATE ${MYSQL_LIB})
