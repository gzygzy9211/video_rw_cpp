
cmake_minimum_required(VERSION 3.8)
project(video_test CXX)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -rdynamic -D_GLIBCXX_USE_CXX11_ABI=0")

# remove 'imgcodecs' if you are using opencv 2.4.x or lower
find_package(OpenCV REQUIRED core imgproc highgui imgcodecs)
add_executable(video_test video_test.cpp)
target_link_libraries(video_test opencv_highgui opencv_imgcodecs opencv_imgproc opencv_core)

