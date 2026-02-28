cmake_minimum_required(VERSION 3.16)
project(metacall_inspect_poc CXX)

# Standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Find MetaCall (Use -DMETACALL_ROOT_DIR=/path/to/core/build)
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(MetaCall REQUIRED)

# ---------------------------------------------------------------------------
# Dependencies (GTest & Benchmark)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip DOWNLOAD_EXTRACT_TIMESTAMP ON)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(googlebenchmark URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip DOWNLOAD_EXTRACT_TIMESTAMP ON)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# ---------------------------------------------------------------------------
# Definitions
# ---------------------------------------------------------------------------
add_compile_definitions(SCRIPTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/scripts")
if(METACALL_ROOT_DIR)
    add_compile_definitions(METACALL_BUILD_DIR="${METACALL_ROOT_DIR}")
endif()

# ---------------------------------------------------------------------------
# ASan auto-detection
# If the MetaCall build has ASan enabled, this POC must also enable it
# to avoid "ASan runtime does not come first" errors on Linux "WARN.
# ---------------------------------------------------------------------------
set(METACALL_ASAN_DETECTED FALSE)
set(METACALL_ASAN_COMPILE_FLAGS)
set(METACALL_ASAN_LINK_FLAGS)
set(METACALL_ASAN_PRELOAD_ENV)
set(METACALL_ASAN_RUNTIME_ENV)

if(METACALL_LIBRARY AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES "Darwin") AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    get_filename_component(_mc_lib_dir "${METACALL_LIBRARY}" DIRECTORY)
    set(_probe_names py_loader libpy_loader node_loader libnode_loader rb_loader librb_loader mock_loader libmock_loader)
    set(_probe_lib)
    foreach(_name ${_probe_names})
        find_library(_found_probe "${_name}" PATHS "${_mc_lib_dir}" NO_DEFAULT_PATH)
        if(_found_probe)
            set(_probe_lib "${_found_probe}")
            unset(_found_probe CACHE)
            break()
        endif()
        unset(_found_probe CACHE)
    endforeach()

    if(_probe_lib)
        file(GET_RUNTIME_DEPENDENCIES LIBRARIES "${_probe_lib}" RESOLVED_DEPENDENCIES_VAR _probe_resolved UNRESOLVED_DEPENDENCIES_VAR _probe_unresolved)
        set(_libasan_path)
        set(_libubsan_path)
        foreach(_dep IN LISTS _probe_resolved)
            if("${_dep}" MATCHES "libasan")
                set(_libasan_path "${_dep}")
            endif()
            if("${_dep}" MATCHES "libubsan")
                set(_libubsan_path "${_dep}")
            endif()
        endforeach()

        if(_libasan_path)
            set(METACALL_ASAN_DETECTED TRUE)
            set(METACALL_ASAN_COMPILE_FLAGS -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow)
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                list(APPEND METACALL_ASAN_COMPILE_FLAGS -fsanitize=pointer-compare -fsanitize=pointer-subtract -fuse-ld=gold -fsanitize=leak)
            endif()
            set(METACALL_ASAN_LINK_FLAGS -fsanitize=address -fsanitize=undefined)
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                if(_libubsan_path)
                    set(METACALL_ASAN_PRELOAD_ENV "LD_PRELOAD=${_libasan_path} ${_libubsan_path}")
                else()
                    set(METACALL_ASAN_PRELOAD_ENV "LD_PRELOAD=${_libasan_path}")
                endif()
            elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
                set(METACALL_ASAN_PRELOAD_ENV "DYLD_INSERT_LIBRARIES=${_libasan_path}" "DYLD_FORCE_FLAT_NAMESPACE=1")
            endif()
            set(METACALL_ASAN_RUNTIME_ENV "ASAN_OPTIONS=use_sigaltstack=0:symbolize=1:alloc_dealloc_mismatch=1:detect_leaks=0:fast_unwind_on_malloc=0" "LSAN_OPTIONS=verbosity=0:log_threads=1:print_suppressions=false")
        endif()
    endif()
endif()

if(METACALL_ASAN_DETECTED)
    add_compile_options(${METACALL_ASAN_COMPILE_FLAGS})
    add_link_options(${METACALL_ASAN_LINK_FLAGS})
endif()

# ---------------------------------------------------------------------------
# Linker Helper
# ---------------------------------------------------------------------------
function(target_link_metacall target)
    if(TARGET MetaCall::MetaCall)
        target_link_libraries(${target} PRIVATE MetaCall::MetaCall)
    else()
        target_link_libraries(${target} PRIVATE ${METACALL_LIBRARY})
        target_include_directories(${target} PRIVATE ${METACALL_INCLUDE_DIRS})
    endif()
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endfunction()

# ---------------------------------------------------------------------------
# Environment Helper
# ---------------------------------------------------------------------------
if(METACALL_ROOT_DIR)
    set(METACALL_RUNTIME_ENV
        "LOADER_LIBRARY_PATH=${METACALL_ROOT_DIR}"
        "SERIAL_LIBRARY_PATH=${METACALL_ROOT_DIR}"
        "DETOUR_LIBRARY_PATH=${METACALL_ROOT_DIR}"
        "CONFIGURATION_PATH=${METACALL_ROOT_DIR}/configurations/global.json"
        "LOADER_SCRIPT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        ${METACALL_ASAN_PRELOAD_ENV}
        ${METACALL_ASAN_RUNTIME_ENV}
    )
else()
    set(METACALL_RUNTIME_ENV
        "LOADER_SCRIPT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        ${METACALL_ASAN_PRELOAD_ENV}
        ${METACALL_ASAN_RUNTIME_ENV}
    )
endif()

# ---------------------------------------------------------------------------
# Main executable: inspect_poc
# ---------------------------------------------------------------------------
add_executable(inspect_poc src/main.cpp)
target_link_metacall(inspect_poc)

# ---------------------------------------------------------------------------
# Test executable: inspect_test
# ---------------------------------------------------------------------------
add_executable(inspect_test tests/test_inspect.cpp)
target_link_metacall(inspect_test)
target_link_libraries(inspect_test PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(inspect_test
    PROPERTIES ENVIRONMENT "${METACALL_RUNTIME_ENV}"
)

# ---------------------------------------------------------------------------
# Benchmark executable: inspect_bench
# ---------------------------------------------------------------------------
add_executable(inspect_bench benchmarks/bench_inspect.cpp)
target_link_metacall(inspect_bench)
target_link_libraries(inspect_bench PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
)
