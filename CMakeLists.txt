cmake_minimum_required(VERSION 3.11.0)
project(imageShenanegans VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)


file(GLOB_RECURSE APP_SOURCES "src/*.c*")

add_executable(imageShenanegans ${APP_SOURCES})

target_link_libraries(imageShenanegans glfw OpenGL::GL)

set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "C++ compiler" FORCE)
