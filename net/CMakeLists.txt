# 收集网络库的源文件
file(GLOB_RECURSE NET_SRC "src/*.cpp")

# 创建静态库
add_library(net STATIC ${NET_SRC})

# 公开头文件目录（方便 server 等模块 include）
target_include_directories(net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 可设置 C++ 标准（继承根项目的）
target_compile_features(net PUBLIC cxx_std_20)