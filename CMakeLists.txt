cmake_minimum_required(VERSION 3.18)

if(APPLE)
    set(CMAKE_C_COMPILER "/usr/local/bin/gcc-11")
    set(CMAKE_CXX_COMPILER "/usr/local/bin/g++-11")
    include_directories("/usr/local/Cellar/argp-standalone/1.3/include")
endif(APPLE)
project(baka_dns)
if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -pedantic)
endif()
include_directories(${PROJECT_SOURCE_DIR}/src)
add_subdirectory(${PROJECT_SOURCE_DIR}/src)
