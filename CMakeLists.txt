cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(lightingguardrisk)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

option(ENABLE_DEV "Enable tests and other dev related features" OFF)
option(USE_LINKER "Use custom linker ld.bfd" "")

if (NOT USE_LINKER STREQUAL "")
  add_link_options(-fuse-ld=${USE_LINKER})
endif()


include(cmake/CPM.cmake)

add_library(policycb INTERFACE)
target_include_directories(policycb INTERFACE include/)
target_sources(policycb INTERFACE include/PolicyCB.hpp)

if (${ENABLE_DEV})
CPMAddPackage("gh:catchorg/Catch2@3.4.0")
CPMAddPackage(
  NAME Format.cmake
  VERSION 1.7.3
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS 
      # set to yes skip cmake formatting
      "FORMAT_SKIP_CMAKE YES"
)

add_executable(runner test/runner.cpp)
target_link_libraries(runner policycb)

add_executable(benchmark test/benchmark.cpp)
target_link_libraries(benchmark policycb Catch2::Catch2WithMain)
enable_testing()

endif()