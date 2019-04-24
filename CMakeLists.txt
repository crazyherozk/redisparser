# first use `mkdir build && cd build`
# then use `cmake .. -DBUILD_SHARED_LIBRARIES=ON/OFF` to make a library
# or use `cmake .. -DBUILD_SHARED_LIBRARIES=ON/OFF -GXcode` to build a project for xcode
# or use `cmake .. -DBUILD_SHARED_LIBRARIES=ON/OFF -G"Visual Studio 14"` to build a project for visual studio 2015
# 检查版本
if (WIN32)
    cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
else()
    cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
endif()

project(RedisParser CXX C)

# 判断是否有定义编译类型（基本指命令行是否传入该变量）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release
        CACHE STRING "Set build type to Debug o Release (default Release)" FORCE)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

# 工具脚本路径
list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# 包含工具脚本
include(CheckSymbolExists)
include(CheckIncludeFiles)
include(cotire)
include(addframework)

# 是否打开任务的Debug信息

option(BUILD_SHARED_LIBRARIES "Define if dss should be build to the shared libraries" OFF)

# Put the libaries and binaries that get built into directories at the
# top of the build tree rather than in hard-to-find leaf directories.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

include_directories(BEFORE ${PROJECT_SOURCE_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/include)

add_definitions(-DUSE_DEFAULT_STD_LIB)

if (WIN32)
    add_definitions(-D__Win32__=1)
elseif (APPLE)
    add_definitions(-D__MacOSX__=1)
elseif (LINUX)
    add_definitions(-D__linux__=1)
endif()

# 判断是否开启任务的Debug信息
if (BUILD_SHARED_LIBRARIES)
    set(LIB_BUILD_TYPE SHARED)
else ()
    set(LIB_BUILD_TYPE STATIC)
endif()

# 判断编译的版本Debug/Release
if ("${CMAKE_BUILD_TYPE_LOWER}" STREQUAL "debug")
    #add_definitions(-g -O0)
elseif ("${CMAKE_BUILD_TYPE_LOWER}" STREQUAL "release")
    #add_definitions(-O2)
#else()
   
endif()

if (CMAKE_COMPILER_IS_GNUCC)
    list(APPEND __FLAGS -Wall -Wswitch)
    list(APPEND __FLAGS -fno-strict-aliasing)
    # 添加编译选项
    add_definitions(${__FLAGS})
endif()

if (APPLE)
    # Get rid of deprecated warnings for OpenSSL on OSX 10.7 and greater.
    add_definitions(
        -Wno-error=deprecated-declarations
        -Qunused-arguments
    )
    set(CMAKE_MACOSX_RPATH 1)
endif()

# Check if _GNU_SOURCE is available.
CHECK_SYMBOL_EXISTS(__GNU_LIBRARY__ "features.h" _GNU_SOURCE)

if (_GNU_SOURCE)
    add_definitions(-D_GNU_SOURCE)
endif()

add_subdirectory(src)
add_subdirectory(test)


message(STATUS "CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR})
message(STATUS "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS "CMAKE_SOURCE_DIR:         " ${CMAKE_SOURCE_DIR})
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "PROJECT_BINARY_DIR:       " ${PROJECT_BINARY_DIR})
message(STATUS "PROJECT_SOURCE_DIR:       " ${PROJECT_SOURCE_DIR})
message(STATUS "CMAKE_MODULE_PATH:        " ${CMAKE_MODULE_PATH})
message(STATUS "CMAKE_COMMAND:            " ${CMAKE_COMMAND})
message(STATUS "CMAKE_ROOT:               " ${CMAKE_ROOT} )
message(STATUS "CMAKE_SYSTEM:             " ${CMAKE_SYSTEM} )
message(STATUS "CMAKE_SYSTEM_NAME:        " ${CMAKE_SYSTEM_NAME} )
message(STATUS "CMAKE_SYSTEM_VERSION:     " ${CMAKE_SYSTEM_VERSION} )
message(STATUS "CMAKE_SYSTEM_PROCESSOR:   " ${CMAKE_SYSTEM_PROCESSOR} )
message(STATUS "CMAKE_SKIP_RPATH:         " ${CMAKE_SKIP_RPATH} )
message(STATUS "CMAKE_VERBOSE_MAKEFILE:   " ${CMAKE_VERBOSE_MAKEFILE} )
message(STATUS "CMAKE_CXX_FLAGS:          " ${CMAKE_CXX_FLAGS} )
message(STATUS "CMAKE_BUILD_TYPE:         " ${CMAKE_BUILD_TYPE} )
message(STATUS "CMAKE_CXX_COMPILER:       " ${CMAKE_CXX_COMPILER} )
message(STATUS "CMAKE_AR:                 " ${CMAKE_AR} )
message(STATUS "CMAKE_RANLIB:             " ${CMAKE_RANLIB} )
message(STATUS "")