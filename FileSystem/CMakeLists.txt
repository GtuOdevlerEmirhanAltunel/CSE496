cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(FileSystem)

include_directories(include)

add_library(FileSystem STATIC
	src/FileSystem.cpp
)

target_include_directories(FileSystem PUBLIC
	include
)

set_target_properties(FileSystem PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)
