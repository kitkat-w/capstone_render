
# # Transfer openpose/models dir into binary dir
# set(SOURCE_MODELS_DIR "${CMAKE_SOURCE_DIR}/deps/openpose/models")
# file(COPY ${SOURCE_MODELS_DIR} DESTINATION ${CMAKE_BINARY_DIR})

cmake_minimum_required(VERSION 3.20)
project(UsARMirror)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(OpenCV REQUIRED)
find_package(realsense2 REQUIRED)
find_package(dlib REQUIRED)
find_package(Boost REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(glfw3 REQUIRED)

find_package(fmt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(Freetype CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
# find_package(glfw3 CONFIG REQUIRED)
find_package(GFlags)
find_package(Glog)
find_package(spdlog CONFIG REQUIRED)
# find_package(OpenCV)
# find_package(OpenPose REQUIRED)

include_directories(
        ${OpenPose_INCLUDE_DIRS}
        ${GFLAGS_INCLUDE_DIR}
        ${GLOG_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

set(Boost_NO_WARN_NEW_VERSIONS ON) # Supress "New Boost version may have incorrect dependencies or import targets" warning
find_package(Boost 1.71.0 REQUIRED COMPONENTS filesystem program_options)

add_subdirectory(external/eos)


# Locate GLES and EGL if needed (for rendering on embedded systems)
find_library(GLESv2_LIB GLESv2 REQUIRED)
find_library(EGL_LIB EGL REQUIRED)

# Add eos as a subdirectory or use preinstalled headers
# If eos is cloned as a submodule or downloaded in 'external/eos', use:
# add_subdirectory(external/eos)


file(GLOB SOURCES
        "src/depth_camera.cpp"
        "src/main.cpp"
)

file(GLOB HEADERS
        "src/depth_camera.hpp"
)

add_executable(UsARMirror
        ${SOURCES}
        ${HEADERS}
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        imgui::imgui
        Freetype::Freetype
        Fontconfig::Fontconfig
        glad::glad
        glfw
        spdlog::spdlog
        ${OpenPose_LIBS}
        ${GFLAGS_LIBRARY}
        ${GLOG_LIBRARY}
        ${OpenCV_LIBS}
        realsense2
)


# Or manually set eos includes:
# include_directories(${PROJECT_SOURCE_DIR}/external/eos/include) # Change if installed elsewhere

# cereal (used by eos)
include_directories(${PROJECT_SOURCE_DIR}/external/cereal/include) # Change if cereal is separate

# Add includes
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${realsense2_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/external/eos/include
    ${PROJECT_SOURCE_DIR}/external/cereal/include 
)

# Source files
set(SOURCES
    realsense_to_3d.cpp
    additive_blend.cpp
)

# Executable
add_executable(facelandmark realsense_to_3d.cpp)


# Link libraries
target_link_libraries(facelandmark
    ${OpenCV_LIBS}
    realsense2
    dlib::dlib
    ${Boost_LIBRARIES}
    ${GLESv2_LIB}
    ${EGL_LIB}
    eos
)


