cmake_minimum_required(VERSION 2.8)
project(detection)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE Main.cpp Frame.cpp Capture.cpp)
set(HEADER Frame.h Capture.h)
find_package(OpenCV REQUIRED )
add_executable(detection ${SOURCE} ${HEADER})

#add_executable(detection Main.cpp)

target_link_libraries(detection ${OpenCV_LIBS} )
