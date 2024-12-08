cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(_FILE_OFFSET_BITS=64)

project(Main)

add_subdirectory(ChunkManager)
add_subdirectory(FileSystem)
add_subdirectory(FuseCommands)

include_directories(ChunkManager/include)
include_directories(FileSystem/include)
include_directories(FuseCommands/include)

add_executable(Main main.cpp)

target_link_libraries(Main PRIVATE ChunkManager FileSystem FuseCommands)

set_target_properties(Main PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
