cmake_minimum_required(VERSION 3.19)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} HYPRLANG_VERSION)

project(
  hyprlang
  VERSION ${HYPRLANG_VERSION}
  DESCRIPTION "A library to parse hypr config files")

include(CTest)
include(GNUInstallDirs)

set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(INCLUDE ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(LIBDIR ${CMAKE_INSTALL_FULL_LIBDIR})

configure_file(hyprlang.pc.in hyprlang.pc @ONLY)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring hyprlang in Debug")
  add_compile_definitions(HYPRLAND_DEBUG)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring hyprlang in Release")
endif()

add_compile_definitions(HYPRLANG_INTERNAL)

set(CMAKE_CXX_STANDARD 23)
add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wno-unused-parameter
  -Wno-missing-field-initializers)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET hyprutils>=0.7.1)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "include/hyprlang.hpp")

add_library(hyprlang SHARED ${SRCFILES})
target_include_directories(
  hyprlang
  PUBLIC "./include"
  PRIVATE "./src")
set_target_properties(
  hyprlang
  PROPERTIES VERSION ${HYPRLANG_VERSION}
             SOVERSION 2
             PUBLIC_HEADER include/hyprlang.hpp)

target_link_libraries(hyprlang PkgConfig::deps)

add_library(hypr::hyprlang ALIAS hyprlang)
install(TARGETS hyprlang)

# tests
add_custom_target(tests)

add_executable(hyprlang_test "tests/parse/main.cpp")
target_link_libraries(hyprlang_test PRIVATE hypr::hyprlang)
add_test(
  NAME "Parsing"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
  COMMAND hyprlang_test "parse")
add_dependencies(tests hyprlang_test)

add_executable(hyprlang_fuzz "tests/fuzz/main.cpp")
target_link_libraries(hyprlang_fuzz PRIVATE hypr::hyprlang)
add_test(
  NAME "Fuzz"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
  COMMAND hyprlang_fuzz "fuzz")
add_dependencies(tests hyprlang_fuzz)

# Installation
install(
  TARGETS hyprlang
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_BINARY_DIR}/hyprlang.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
