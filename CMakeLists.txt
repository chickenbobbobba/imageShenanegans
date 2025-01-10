cmake_minimum_required(VERSION 3.11.0)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "C++ compiler" FORCE)
project(mandelExplorer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
cmake_policy(SET CMP0072 NEW)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MAKE_PROGRAM mandelExplorer)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB_RECURSE APP_SOURCES "src/*.c*")

add_executable(mandelExplorer ${APP_SOURCES})
target_link_libraries(mandelExplorer glfw OpenGL::GL mpfr gmp)
