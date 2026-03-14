cmake_minimum_required(VERSION 3.20)
project(market-data-multicast-analyzer CXX)

# ---------------------------------------------------------------------------
# C++ standard & compiler flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimised production flags matching CLAUDE.md spec
add_compile_options(-O2 -march=native -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Dependencies — libpcap
# ---------------------------------------------------------------------------
# On macOS, libpcap ships with the Xcode SDK and has no pkg-config entry.
# Try pkg-config first (Linux); fall back to find_library (macOS).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PCAP libpcap)
endif()

if(NOT PCAP_FOUND)
    find_library(PCAP_LIBRARIES NAMES pcap REQUIRED)
    find_path(PCAP_INCLUDE_DIRS pcap.h REQUIRED)
    set(PCAP_FOUND TRUE)
    message(STATUS "libpcap found via find_library: ${PCAP_LIBRARIES}")
endif()

# Google Test — optional; tests are built only when found
find_package(GTest QUIET)

# ---------------------------------------------------------------------------
# Core static library  (shared by tools and tests)
# ---------------------------------------------------------------------------
set(MDA_SOURCES
    src/capture/pcap_reader.cpp
    src/capture/live_capture.cpp
    src/decode/feed/synthetic_feed.cpp
    src/decode/feed/itch_lite.cpp
    src/analysis/gap_detector.cpp
    src/analysis/latency_tracker.cpp
    src/analysis/burst_detector.cpp
    src/analysis/book_reconstructor.cpp
    src/output/reporter.cpp
    src/output/csv_writer.cpp
    src/util/timestamp.cpp
)

add_library(mda_core STATIC ${MDA_SOURCES})
target_include_directories(mda_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${PCAP_INCLUDE_DIRS}
)
target_link_libraries(mda_core PUBLIC ${PCAP_LIBRARIES})

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
add_executable(analyzer       tools/analyzer.cpp)
add_executable(feed_generator tools/feed_generator.cpp)
add_executable(book_viewer    tools/book_viewer.cpp)

foreach(tool analyzer feed_generator book_viewer)
    target_link_libraries(${tool} PRIVATE mda_core)
endforeach()

# ---------------------------------------------------------------------------
# Tests  (built only when GTest is available)
# ---------------------------------------------------------------------------
if(GTest_FOUND OR GTest_FOUND)
    # Probe both spellings that find_package may produce
    if(NOT TARGET GTest::GTest AND TARGET GTest::gtest)
        add_library(GTest::GTest ALIAS GTest::gtest)
        add_library(GTest::Main  ALIAS GTest::gtest_main)
    endif()
endif()

if(TARGET GTest::GTest)
    enable_testing()

    add_executable(run_tests
        test/test_gap_detector.cpp
        test/test_latency_tracker.cpp
        test/test_packet_decode.cpp
        test/test_book_reconstructor.cpp
    )

    target_include_directories(run_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(run_tests PRIVATE mda_core GTest::GTest GTest::Main)

    # Address + UB sanitizers on tests (per CLAUDE.md)
    target_compile_options(run_tests PRIVATE -fsanitize=address,undefined)
    target_link_options   (run_tests PRIVATE -fsanitize=address,undefined)

    include(GoogleTest)
    gtest_discover_tests(run_tests)

    message(STATUS "Google Test found — test target 'run_tests' enabled")
else()
    message(STATUS "Google Test not found — skipping test target")
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS analyzer feed_generator book_viewer
        RUNTIME DESTINATION bin)
