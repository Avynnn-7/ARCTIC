cmake_minimum_required(VERSION 3.16)
project(arctic_wasm)

set(CMAKE_CXX_STANDARD 17)

# We compile the core WASM bindings
add_executable(arctic_wasm ../src/wasm_core.cpp)

# Emscripten specific flags for lock-free concurrency and WebGL memory matching
target_link_options(arctic_wasm PRIVATE
    "-lembind"
    "-s" "EXPORT_ES6=1"
    "-s" "MODULARIZE=1"
    "-s" "USE_PTHREADS=1"
    "-s" "PTHREAD_POOL_SIZE=2"
    "-s" "SHARED_MEMORY=1"
    "-s" "ALLOW_MEMORY_GROWTH=1"
    "-s" "WASM_BIGINT=1"
    "-s" "ENVIRONMENT=web,worker"
)

# Output directly to the public folder of the Vite app
set_target_properties(arctic_wasm PROPERTIES
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/public"
)
