cmake_minimum_required(VERSION 3.10)
project(MotionServerLite)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio)

add_executable(motion_detect motion_detect.cpp httplib.h)
target_link_libraries(motion_detect ${OpenCV_LIBS})
