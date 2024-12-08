cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config")

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE2 REQUIRED fuse)

project(FuseCommands)

include_directories(include)
include_directories(${FUSE2_INCLUDE_DIRS})

add_library(FuseCommands STATIC
	src/Fuse.cpp
)

target_link_libraries(FuseCommands PRIVATE ${FUSE2_LIBRARIES})

target_include_directories(FuseCommands PUBLIC
	include
)

set_target_properties(FuseCommands PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)
