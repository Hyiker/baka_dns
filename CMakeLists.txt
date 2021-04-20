cmake_minimum_required(VERSION 3.18)

project(baka_dns)
if(APPLE)
    set(CMAKE_C_COMPILER "/usr/local/bin/gcc-10")
    include_directories("/usr/local/Cellar/argp-standalone/1.3/include")
endif(APPLE)

include_directories(${PROJECT_SOURCE_DIR}/src)
add_subdirectory(${PROJECT_SOURCE_DIR}/src)
