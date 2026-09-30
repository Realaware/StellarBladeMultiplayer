cmake_minimum_required(VERSION 3.20)
project(upstream_startup_guard_tests LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

add_executable(upstream_startup_guard_tests tests.cpp)
target_compile_features(upstream_startup_guard_tests PRIVATE cxx_std_20)
set_target_properties(upstream_startup_guard_tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(upstream_startup_guard_tests PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(upstream_startup_guard_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(upstream_startup_guard_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME upstream_startup_guard COMMAND upstream_startup_guard_tests)
set_tests_properties(upstream_startup_guard PROPERTIES TIMEOUT 30)
