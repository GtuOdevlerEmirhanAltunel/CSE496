cmake_minimum_required(VERSION 3.10)
project(MotionServerLite)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio highgui)

add_executable(motion_detect
	handleHttpClient.cpp
	motion_detect.cpp
	motionDetectionLoop.cpp
	sanitizeFilename.cpp
	startHttpServer.cpp
)
target_link_libraries(motion_detect ${OpenCV_LIBS} pthread)
