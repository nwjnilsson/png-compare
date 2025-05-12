cmake_minimum_required(VERSION 3.5)

project(PNGCompare)

# Set OpenCV build flags
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs")
set(BUILD_opencv_world OFF CACHE BOOL "Disable monolithic build")
set(BUILD_opencv_apps OFF CACHE BOOL "")
set(OPENCV_FORCE_3RDPARTY_BUILD ON CACHE BOOL "")
set(BUILD_TESTS OFF CACHE BOOL "")
set(BUILD_PERF_TESTS OFF CACHE BOOL "")
set(BUILD_EXAMPLES OFF CACHE BOOL "")

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(opencv-4.11.0)

add_executable(png-compare "png-compare.cpp")

# Ran into issues getting the include dirs from OpenCV automatically with cmake
target_include_directories(png-compare PRIVATE
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/core/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/imgproc/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/calib3d/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/features2d/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/flann/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/dnn/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/highgui/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/imgcodecs/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/videoio/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/ml/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/stitching/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/objdetect/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/photo/include"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/video/include"
    "${CMAKE_BINARY_DIR}/opencv-4.11.0/modules/core"
    "${CMAKE_BINARY_DIR}/opencv-4.11.0/modules/imgproc"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/calib3d"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/features2d"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/flann"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/dnn"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/highgui"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/imgcodecs"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/videoio"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/ml"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/stitching"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/objdetect"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/photo"
    "${CMAKE_SOURCE_DIR}/opencv-4.11.0/modules/video"
)
target_link_libraries(png-compare PRIVATE opencv_core opencv_imgproc opencv_imgcodecs)



add_subdirectory(cxxopts-3.2.0)

add_executable(aggregate "aggregate.cpp")

target_include_directories(aggregate PRIVATE 
    "${CMAKE_BINARY_DIR}/cxxopts-3.2.0/include"
)
target_link_libraries(aggregate PRIVATE cxxopts)
