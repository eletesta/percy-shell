cmake_minimum_required(VERSION 3.1)
project(alice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# some specific compiler definitions
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fcolor-diagnostics" HAS_FCOLOR_DIAGNOSTICS)
if (HAS_FCOLOR_DIAGNOSTICS)
  add_definitions(-fcolor-diagnostics)
endif()

add_subdirectory(alice)
add_subdirectory(percy)
add_subdirectory(src)