cmake_minimum_required(VERSION 3.10)
project(myserver)
set(CMAKE_CXX_STANDARD 17)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#include_directories(${PROJECT_SOURCE_DIR}/LogSys/include)
#aux_source_directory(${PROJECT_SOURCE_DIR}/LogSys/src SRC_LOG)

include_directories(${PROJECT_SOURCE_DIR}/net/include)
aux_source_directory(${PROJECT_SOURCE_DIR}/net/src SRC_SERVER)

add_subdirectory(example)
