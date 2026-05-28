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
)

# ========== 使用 3rdparty 中的 MySQL 库 ==========
# 指定头文件搜索路径
target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/mysql/include
)

# 指定库文件搜索路径
target_link_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/3rdparty/mysql/lib
)

# 链接 MySQL 客户端库
target_link_libraries(main PRIVATE mysqlclient)
