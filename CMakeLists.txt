cmake_minimum_required(VERSION 3.10.0)
project(imageShenanegans VERSION 0.1.0 LANGUAGES C CXX)

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/*.cpp")

add_executable(imageShenanegans main.cpp threadPool.cpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)