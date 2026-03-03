cmake_minimum_required(VERSION 3.16)
project(metacall_inspect_poc C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(METACALL_INSPECT_POC_BUILD_RUNTIME "Build runtime inspect demo and runtime-linked targets" ON)
option(METACALL_INSPECT_POC_BUILD_RUNTIME_TESTS "Build runtime inspect gtest target" ON)
option(METACALL_INSPECT_POC_BUILD_BENCHMARKS "Build benchmark target" ON)
option(METACALL_INSPECT_POC_BUILD_AST_TESTS "Enable CTest checks for ast_inspect output" ON)

add_compile_definitions(SCRIPTS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/scripts")

include(FetchContent)

cmake_policy(SET CMP0169 OLD)

FetchContent_Declare(tree_sitter
	URL https://github.com/tree-sitter/tree-sitter/archive/refs/tags/v0.22.6.zip
	DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_GetProperties(tree_sitter)
if(NOT tree_sitter_POPULATED)
	FetchContent_Populate(tree_sitter)
endif()

add_library(ts_runtime STATIC ${tree_sitter_SOURCE_DIR}/lib/src/lib.c)
target_include_directories(ts_runtime
	PUBLIC ${tree_sitter_SOURCE_DIR}/lib/include
	PRIVATE ${tree_sitter_SOURCE_DIR}/lib/src)
set_target_properties(ts_runtime PROPERTIES C_STANDARD 11)

macro(add_ts_grammar target_name content_name repo_url repo_tag)
	FetchContent_Declare(${content_name}
		URL ${repo_url}/archive/refs/tags/${repo_tag}.zip
		DOWNLOAD_EXTRACT_TIMESTAMP ON)
	FetchContent_GetProperties(${content_name})
	if(NOT ${content_name}_POPULATED)
		FetchContent_Populate(${content_name})
	endif()

	add_library(${target_name} STATIC
		${${content_name}_SOURCE_DIR}/src/parser.c)

	if(EXISTS ${${content_name}_SOURCE_DIR}/src/scanner.c)
		target_sources(${target_name} PRIVATE ${${content_name}_SOURCE_DIR}/src/scanner.c)
	endif()

	target_include_directories(${target_name} PUBLIC ${${content_name}_SOURCE_DIR}/src)
	target_link_libraries(${target_name} PRIVATE ts_runtime)
	set_target_properties(${target_name} PROPERTIES C_STANDARD 11)
endmacro()

add_ts_grammar(ts_python tree_sitter_python https://github.com/tree-sitter/tree-sitter-python v0.23.6)
add_ts_grammar(ts_javascript tree_sitter_javascript https://github.com/tree-sitter/tree-sitter-javascript v0.23.1)

add_executable(ast_inspect
	src/ast_main.cpp
	src/ast_cli.cpp
	src/ast_extract.cpp
	src/ast_serialize.cpp)
target_link_libraries(ast_inspect PRIVATE ts_runtime ts_python ts_javascript)
target_include_directories(ast_inspect PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${tree_sitter_SOURCE_DIR}/lib/include)

if(METACALL_INSPECT_POC_BUILD_AST_TESTS)
	enable_testing()

	add_test(
		NAME ast_inspect_python
		COMMAND ast_inspect ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.py)
	set_tests_properties(ast_inspect_python PROPERTIES
		PASS_REGULAR_EXPRESSION "\"language\":\"python\""
		FAIL_REGULAR_EXPRESSION "error:")

	add_test(
		NAME ast_inspect_python_symbol
		COMMAND ast_inspect ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.py)
	set_tests_properties(ast_inspect_python_symbol PROPERTIES
		PASS_REGULAR_EXPRESSION "python_test_function")

	add_test(
		NAME ast_inspect_javascript
		COMMAND ast_inspect ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.js)
	set_tests_properties(ast_inspect_javascript PROPERTIES
		PASS_REGULAR_EXPRESSION "\"language\":\"javascript\""
		FAIL_REGULAR_EXPRESSION "error:")

	add_test(
		NAME ast_inspect_javascript_symbol
		COMMAND ast_inspect ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.js)
	set_tests_properties(ast_inspect_javascript_symbol PROPERTIES
		PASS_REGULAR_EXPRESSION "node_test_function")
endif()

if(METACALL_INSPECT_POC_BUILD_RUNTIME)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
	find_package(MetaCall REQUIRED)

	if(METACALL_ROOT_DIR)
		add_compile_definitions(METACALL_BUILD_DIR="${METACALL_ROOT_DIR}")
	endif()

	function(target_link_metacall target)
		if(TARGET MetaCall::MetaCall)
			target_link_libraries(${target} PRIVATE MetaCall::MetaCall)
		else()
			target_link_libraries(${target} PRIVATE ${METACALL_LIBRARY})
			target_include_directories(${target} PRIVATE ${METACALL_INCLUDE_DIRS})
		endif()
		target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
	endfunction()

	if(METACALL_ROOT_DIR)
		set(METACALL_RUNTIME_ENV
			"LOADER_LIBRARY_PATH=${METACALL_ROOT_DIR}"
			"SERIAL_LIBRARY_PATH=${METACALL_ROOT_DIR}"
			"DETOUR_LIBRARY_PATH=${METACALL_ROOT_DIR}"
			"CONFIGURATION_PATH=${METACALL_ROOT_DIR}/configurations/global.json"
			"LOADER_SCRIPT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/scripts")
	else()
		set(METACALL_RUNTIME_ENV
			"LOADER_SCRIPT_PATH=${CMAKE_CURRENT_SOURCE_DIR}/scripts")
	endif()

	add_executable(inspect_poc src/main.cpp)
	target_link_metacall(inspect_poc)

	if(METACALL_INSPECT_POC_BUILD_RUNTIME_TESTS)
		FetchContent_Declare(googletest
			URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
			DOWNLOAD_EXTRACT_TIMESTAMP ON)
		set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
		FetchContent_MakeAvailable(googletest)

		add_executable(inspect_test tests/test_inspect.cpp)
		target_link_metacall(inspect_test)
		target_link_libraries(inspect_test PRIVATE gtest_main)

		include(GoogleTest)
		gtest_discover_tests(inspect_test PROPERTIES ENVIRONMENT "${METACALL_RUNTIME_ENV}")
	endif()

	if(METACALL_INSPECT_POC_BUILD_BENCHMARKS)
		FetchContent_Declare(googlebenchmark
			URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
			DOWNLOAD_EXTRACT_TIMESTAMP ON)
		set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
		set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
		FetchContent_MakeAvailable(googlebenchmark)

		add_executable(inspect_bench benchmarks/bench_inspect.cpp)
		target_link_metacall(inspect_bench)
		target_link_libraries(inspect_bench PRIVATE benchmark::benchmark benchmark::benchmark_main)
	endif()
endif()
