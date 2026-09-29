cmake_minimum_required(VERSION 3.24)
project(StellarBladeCoop VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

add_library(sbcoop_core
    src/protocol/codec.cpp
    src/replication/entity_registry.cpp
    src/replication/snapshot_buffer.cpp
)
target_include_directories(sbcoop_core PUBLIC include)
target_compile_features(sbcoop_core PUBLIC cxx_std_20)
set_target_properties(sbcoop_core PROPERTIES CXX_EXTENSIONS OFF)

function(sbcoop_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
sbcoop_warnings(sbcoop_core)

add_executable(sbcoop_harness tools/harness/main.cpp)
target_link_libraries(sbcoop_harness PRIVATE sbcoop_core Threads::Threads)
sbcoop_warnings(sbcoop_harness)

if(BUILD_TESTING)
    add_executable(sbcoop_tests tests/core_tests.cpp)
    target_link_libraries(sbcoop_tests PRIVATE sbcoop_core Threads::Threads)
    sbcoop_warnings(sbcoop_tests)
    add_test(NAME core COMMAND sbcoop_tests)
    add_test(NAME harness_clean COMMAND sbcoop_harness)
    add_test(NAME harness_impaired COMMAND sbcoop_harness --impaired)
    set_tests_properties(core harness_clean harness_impaired PROPERTIES TIMEOUT 30)
endif()
