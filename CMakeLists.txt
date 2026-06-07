cmake_minimum_required(VERSION 3.15)
project(ARCTIC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if no build type specified.
# Timing Debug builds is meaningless — the optimizer eliminates dead stores,
# inlines functions, and vectorizes loops that dominate execution time.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "No build type specified. Defaulting to Release.")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
endif()

include_directories(src)

# ── Core library ─────────────────────────────────────────────────────────
add_library(arctic_core
    src/ou_sampler.cpp
    src/single_agent.cpp
    src/live_latency.cpp
    src/math_utils.cpp
    src/replay_engine.cpp
)

if(WIN32)
    target_link_libraries(arctic_core PRIVATE ws2_32)
endif()

# ── Main executable ──────────────────────────────────────────────────────
add_executable(arctic src/main.cpp)
target_link_libraries(arctic PRIVATE arctic_core)

# ── Tests ────────────────────────────────────────────────────────────────
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(arctic_tests 
    tests/test_math.cpp 
    tests/test_lob.cpp
    tests/test_itch.cpp
)
target_link_libraries(arctic_tests PRIVATE arctic_core gtest_main)
add_test(NAME arctic_tests COMMAND arctic_tests)

# ── Sweep Generator ─────────────────────────────────────────────────────
add_executable(arctic_sweep src/sweep_generator.cpp)
target_link_libraries(arctic_sweep PRIVATE arctic_core)

# ── KS Test ──────────────────────────────────────────────────────────────
add_executable(test_ks src/test_ks.cpp)
target_link_libraries(test_ks PRIVATE arctic_core)

# ── Live Adaptation Logger ───────────────────────────────────────────────
add_executable(live_adaptation_logger src/live_adaptation_logger.cpp)
target_link_libraries(live_adaptation_logger PRIVATE arctic_core)

# ── ITCH Replay Engine ───────────────────────────────────────────────────
add_executable(arctic_replay src/replay_main.cpp)
target_link_libraries(arctic_replay PRIVATE arctic_core)

if(WIN32)
    target_link_libraries(arctic PRIVATE ws2_32)
    target_link_libraries(arctic_sweep PRIVATE ws2_32)
    target_link_libraries(test_ks PRIVATE ws2_32)
    target_link_libraries(live_adaptation_logger PRIVATE ws2_32)
    target_link_libraries(arctic_replay PRIVATE ws2_32)
endif()

# ── Google Benchmark (optional) ──────────────────────────────────────────
# Build with: cmake -DARCTIC_BUILD_BENCHMARKS=ON ..
# Requires internet access on first build (FetchContent downloads Google Benchmark)
option(ARCTIC_BUILD_BENCHMARKS "Build microbenchmarks (requires Google Benchmark)" OFF)

if(ARCTIC_BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(bench_spsc benchmarks/bench_spsc.cpp)
    target_link_libraries(bench_spsc PRIVATE arctic_core benchmark::benchmark)
    if(WIN32)
        target_link_libraries(bench_spsc PRIVATE ws2_32 shlwapi)
    endif()
endif()
