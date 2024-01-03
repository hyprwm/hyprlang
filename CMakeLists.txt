cmake_minimum_required(VERSION 3.19)

set(HYPRLANG_VERSION "0.2.1")
set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(INCLUDE ${CMAKE_INSTALL_INCLUDEDIR})

project(hyprlang
    VERSION ${HYPRLANG_VERSION}
    DESCRIPTION "A library to parse hypr config files"
)

configure_file(hyprlang.pc.in hyprlang.pc @ONLY)

include(CTest)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "include/hyprlang.hpp")

add_library(hyprlang SHARED ${SRCFILES})
target_include_directories( hyprlang
    PUBLIC "./include"
    PRIVATE "./src"
)
set_target_properties(hyprlang PROPERTIES PUBLIC_HEADER include/hyprlang.hpp)

install(TARGETS hyprlang)

# tests
add_custom_target(tests)

add_executable(hyprlang_test "tests/parse/main.cpp")
target_link_libraries(hyprlang_test PRIVATE hyprlang)
add_test(NAME "Parsing" WORKING_DIRECTORY "../tests/" COMMAND hyprlang_test "parse")
add_dependencies(tests hyprlang_test)

add_executable(hyprlang_fuzz "tests/fuzz/main.cpp")
target_link_libraries(hyprlang_fuzz PRIVATE hyprlang)
add_test(NAME "Fuzz" WORKING_DIRECTORY "../tests/" COMMAND hyprlang_fuzz "fuzz")
add_dependencies(tests hyprlang_fuzz)

# Installation
install(TARGETS hyprlang
        PUBLIC_HEADER DESTINATION include
        LIBRARY DESTINATION lib)
install(FILES ${CMAKE_BINARY_DIR}/hyprlang.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pkgconfig)
