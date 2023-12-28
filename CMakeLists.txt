cmake_minimum_required(VERSION 3.19)
project(hyprlang
    VERSION "0.1"
    DESCRIPTION "A library to parse hypr config files"
)

include(CTest)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "include/hyprlang.hpp")

add_library(hyprlang SHARED ${SRCFILES})
target_include_directories( hyprlang
    PUBLIC "./include"
    PRIVATE "./src"
)

install(TARGETS hyprlang)

add_executable(hyprlang_test "tests/main.cpp")
target_link_libraries(hyprlang_test PRIVATE hyprlang)
add_test(NAME "Parsing" WORKING_DIRECTORY "../tests/" COMMAND hyprlang_test "parse")
