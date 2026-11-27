cmake_minimum_required(VERSION 3.25)

project(PGClipGainPro VERSION 0.1.0 LANGUAGES CXX)

option(PGCLIP_BUILD_TESTS "Build the host-independent test suite" ON)
option(PGCLIP_BUILD_VST3 "Build the VST3 plug-in target" ON)

find_package(Threads REQUIRED)

add_library(pgclip_core STATIC
    src/core/types.cpp
    src/core/capture_accumulator.cpp
    src/core/window_analysis.cpp
    src/core/shared_analysis_snapshot.cpp
    src/core/window_context.cpp
    src/core/spsc_audio_block_queue.cpp
    src/core/queued_capture_assembler.cpp
    src/core/capture_session_control.cpp
    src/core/planar_audio_queue_writer.cpp
    src/core/transport_continuity_tracker.cpp
    src/core/window_activity.cpp
    src/core/clip_gain_foundation.cpp
    src/core/gain_curve_processor.cpp
    src/core/clip_peak_foundation.cpp
)
target_include_directories(pgclip_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(pgclip_core PUBLIC cxx_std_17)
set_target_properties(pgclip_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

    add_executable(pgclip_context_tests tests/core/test_window_context.cpp)
    target_link_libraries(pgclip_context_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_context_tests COMMAND pgclip_context_tests)

    add_executable(pgclip_spsc_queue_tests tests/core/test_spsc_audio_block_queue.cpp)
    target_link_libraries(pgclip_spsc_queue_tests PRIVATE pgclip_core Threads::Threads)
    add_test(NAME pgclip_spsc_queue_tests COMMAND pgclip_spsc_queue_tests)

    add_executable(pgclip_queued_capture_tests tests/core/test_queued_capture_assembler.cpp)
    target_link_libraries(pgclip_queued_capture_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_queued_capture_tests COMMAND pgclip_queued_capture_tests)

    add_executable(pgclip_capture_session_tests tests/core/test_capture_session_control.cpp)
    target_link_libraries(pgclip_capture_session_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_capture_session_tests COMMAND pgclip_capture_session_tests)

    add_executable(pgclip_planar_queue_writer_tests tests/core/test_planar_audio_queue_writer.cpp)
    target_link_libraries(pgclip_planar_queue_writer_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_planar_queue_writer_tests COMMAND pgclip_planar_queue_writer_tests)

    add_executable(pgclip_transport_continuity_tests tests/core/test_transport_continuity_tracker.cpp)
    target_link_libraries(pgclip_transport_continuity_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_transport_continuity_tests COMMAND pgclip_transport_continuity_tests)

    add_executable(pgclip_activity_tests tests/core/test_window_activity.cpp)
    target_link_libraries(pgclip_activity_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_activity_tests COMMAND pgclip_activity_tests)

    add_executable(pgclip_clip_gain_tests tests/core/test_clip_gain_foundation.cpp)
    target_link_libraries(pgclip_clip_gain_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_clip_gain_tests COMMAND pgclip_clip_gain_tests)

    add_executable(pgclip_gain_curve_tests tests/core/test_gain_curve_processor.cpp)
    target_link_libraries(pgclip_gain_curve_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_gain_curve_tests COMMAND pgclip_gain_curve_tests)

    add_executable(pgclip_clip_peak_tests tests/core/test_clip_peak_foundation.cpp)
    target_link_libraries(pgclip_clip_peak_tests PRIVATE pgclip_core)
    add_test(NAME pgclip_clip_peak_tests COMMAND pgclip_clip_peak_tests)
endif()

if(PGCLIP_BUILD_VST3)
    include(FetchContent)

    # Keep the SDK build lean: disable example plug-ins and VSTGUI; retain the official validator.
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
    set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)
    set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
    set(SMTG_CREATE_MODULE_INFO ON CACHE BOOL "" FORCE)

    FetchContent_Declare(vst3sdk
        GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
        GIT_TAG 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
        GIT_SUBMODULES_RECURSE TRUE
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(vst3sdk)
    smtg_enable_vst3_sdk()

    set(PGCLIP_VST3_SOURCES
        src/vst3/pgclip_processor.cpp
        src/vst3/vst3_capture_runtime.cpp
        src/vst3/vst3_capture_runtime.h
        src/vst3/pgclip_controller.cpp
        src/vst3/pgclip_entry.cpp
        src/vst3/pgclip_cids.h
        src/vst3/pgclip_processor.h
        src/vst3/pgclip_controller.h
        src/vst3/version.h
    )

    smtg_add_vst3plugin(PGClipGainPro ${PGCLIP_VST3_SOURCES})
    smtg_target_configure_version_file(PGClipGainPro)

    target_compile_features(PGClipGainPro PRIVATE cxx_std_17)
    target_include_directories(PGClipGainPro PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vst3
    )
    target_link_libraries(PGClipGainPro PRIVATE sdk pgclip_core Threads::Threads)

    if(MSVC)
        target_compile_options(PGClipGainPro PRIVATE /W4 /permissive-)
    else()
        target_compile_options(PGClipGainPro PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(SMTG_MAC)
        smtg_target_set_bundle(PGClipGainPro
            BUNDLE_IDENTIFIER "com.gastonpuig.pgclipgainpro"
            COMPANY_NAME "Gastón Puig"
        )
    endif()
endif()
