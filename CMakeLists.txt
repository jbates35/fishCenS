cmake_minimum_required(VERSION 3.25)
project(dehaze)

find_package(OpenCV REQUIRED)
include_directories("include")
set(CMAKE_CXX_STANDARD 17)

add_executable(dehaze main.cpp dehaze.cpp dehaze.h cvui.h LowPassFilter.h functimer.h)

install(TARGETS dehaze
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(dehaze ${OpenCV_LIBS})

