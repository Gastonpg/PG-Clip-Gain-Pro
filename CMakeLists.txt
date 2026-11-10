cmake_minimum_required(VERSION 3.20)

project(PGClipGainPro VERSION 0.1.0 LANGUAGES CXX)

option(PGCLIP_BUILD_TESTS "Build the host-independent test suite" ON)

add_library(pgclip_core STATIC
    src/core/types.cpp
    src/core/capture_accumulator.cpp
    src/core/window_analysis.cpp
    src/core/shared_analysis_snapshot.cpp
)
target_include_directories(pgclip_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(pgclip_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(pgclip_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(pgclip_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(PGCLIP_BUILD_TESTS)
    enable_testing()
    add_executable(pgclip_core_tests tests/core/test_types.cpp)
    target_link_libraries(pgclip_core_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_core_tests COMMAND pgclip_core_tests)

    add_executable(pgclip_fixture_tests tests/fixtures/test_synthetic_signals.cpp)
    target_link_libraries(pgclip_fixture_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_fixture_tests COMMAND pgclip_fixture_tests)

    add_executable(pgclip_capture_tests tests/core/test_capture_accumulator.cpp)
    target_link_libraries(pgclip_capture_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_capture_tests COMMAND pgclip_capture_tests)

    add_executable(pgclip_window_tests tests/core/test_window_analysis.cpp)
    target_link_libraries(pgclip_window_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_window_tests COMMAND pgclip_window_tests)

    add_executable(pgclip_snapshot_tests tests/core/test_shared_analysis_snapshot.cpp)
    target_link_libraries(pgclip_snapshot_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_snapshot_tests COMMAND pgclip_snapshot_tests)
endif()
