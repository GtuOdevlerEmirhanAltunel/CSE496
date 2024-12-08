cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(ChunkManager)

include_directories(include)

add_library(ChunkManager STATIC
	src/ChunkManager.cpp
	src/DeviceChunkManager.cpp
	src/FileChunkManager.cpp
	src/MemoryChunkManager.cpp
)

target_include_directories(ChunkManager PUBLIC
	include
)

set_target_properties(ChunkManager PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)
