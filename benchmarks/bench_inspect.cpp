/*
 *  benchmarks/bench_inspect.cpp
 *
 *  Micro-benchmark for metacall_inspect() throughput that is just a base for what's next.
 */

#include <benchmark/benchmark.h>
#include <metacall/metacall.h>
#include <metacall/metacall_allocator.h>

#include <cstdlib>
#include <cstring>
#include <string>

static void setup_bench_env()
{
#ifdef METACALL_BUILD_DIR
	const char *build_dir = METACALL_BUILD_DIR;

	if (!getenv("LOADER_LIBRARY_PATH"))
		setenv("LOADER_LIBRARY_PATH", build_dir, 1);

	if (!getenv("SERIAL_LIBRARY_PATH"))
		setenv("SERIAL_LIBRARY_PATH", build_dir, 1);

	if (!getenv("DETOUR_LIBRARY_PATH"))
		setenv("DETOUR_LIBRARY_PATH", build_dir, 1);

	if (!getenv("CONFIGURATION_PATH"))
	{
		std::string cfg = std::string(build_dir) + "/configurations/global.json";
		setenv("CONFIGURATION_PATH", cfg.c_str(), 1);
	}
#endif

#ifdef SCRIPTS_PATH
	if (!getenv("LOADER_SCRIPT_PATH"))
		setenv("LOADER_SCRIPT_PATH", SCRIPTS_PATH, 1);
#endif
}

/* ---------------------------------------------------------------------------
 * Fixture: initialises MetaCall once per benchmark binary run.
 * --------------------------------------------------------------------------- */
class InspectFixture : public benchmark::Fixture
{
public:
	void SetUp(benchmark::State & /*state*/) override
	{
		setup_bench_env();

		metacall_initialize();

		/* Non-fatal: loaders may not be built in every configuration. */
		const char *py_scripts[] = { "test.py" };
		metacall_load_from_file("py", py_scripts, 1, nullptr);

		const char *node_scripts[] = { "test.js" };
		metacall_load_from_file("node", node_scripts, 1, nullptr);
	}

	void TearDown(benchmark::State & /*state*/) override
	{
		metacall_destroy();
	}
};

/* ---------------------------------------------------------------------------
 * BM_MetacallInspect
 *
 * Measures the round-trip cost of:
 *   metacall_inspect()          – serialise runtime state to JSON
 *   metacall_allocator_free()   – release the buffer
 * --------------------------------------------------------------------------- */
BENCHMARK_F(InspectFixture, BM_MetacallInspect)(benchmark::State &state)
{
	struct metacall_allocator_std_type std_ctx = {
		&std::malloc,
		&std::realloc,
		&std::free
	};

	void *allocator = metacall_allocator_create(
		METACALL_ALLOCATOR_STD, static_cast<void *>(&std_ctx));

	if (!allocator)
	{
		state.SkipWithError("metacall_allocator_create() returned null");
		return;
	}

	for (auto _ : state)
	{
		size_t size = 0;
		char *buffer = metacall_inspect(&size, allocator);

		benchmark::DoNotOptimize(buffer);
		benchmark::DoNotOptimize(size);

		if (buffer)
		{
			/* Correct release path: through the same allocator object. */
			metacall_allocator_free(allocator, buffer);
		}
	}

	metacall_allocator_destroy(allocator);
}
