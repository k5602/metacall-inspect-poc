/*
 * metacall-inspect-poc/src/main.cpp
 *
 * Demonstrates metacall_inspect() using the correct allocator API.
 */

#include <metacall/metacall.h>
#include <metacall/metacall_allocator.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

/* Setup environment for loading plugins from build directory */
static void setup_env()
{
#ifdef METACALL_BUILD_DIR
	const char *build_dir = METACALL_BUILD_DIR;

	if (getenv("LOADER_LIBRARY_PATH") == nullptr)
		setenv("LOADER_LIBRARY_PATH", build_dir, 1);

	if (getenv("SERIAL_LIBRARY_PATH") == nullptr)
		setenv("SERIAL_LIBRARY_PATH", build_dir, 1);

	if (getenv("DETOUR_LIBRARY_PATH") == nullptr)
		setenv("DETOUR_LIBRARY_PATH", build_dir, 1);

	if (getenv("CONFIGURATION_PATH") == nullptr)
	{
		std::string cfg = std::string(build_dir) + "/configurations/global.json";
		setenv("CONFIGURATION_PATH", cfg.c_str(), 1);
	}
#endif

#ifdef SCRIPTS_PATH
	if (getenv("LOADER_SCRIPT_PATH") == nullptr)
		setenv("LOADER_SCRIPT_PATH", SCRIPTS_PATH, 1);
#endif
}

static void load_scripts()
{
	const char *py_scripts[] = { "test.py" };
	metacall_load_from_file("py", py_scripts, 1, nullptr);

	const char *node_scripts[] = { "test.js" };
	metacall_load_from_file("node", node_scripts, 1, nullptr);
}

int main()
{
	setup_env();

	if (metacall_initialize() != 0)
	{
		return 1;
	}

	load_scripts();

	/* Define allocator (use std heap) */
	struct metacall_allocator_std_type std_ctx = { &std::malloc, &std::realloc, &std::free };
	void *allocator = metacall_allocator_create(METACALL_ALLOCATOR_STD, &std_ctx);

	if (allocator)
	{
		size_t size = 0;
		char *buffer = metacall_inspect(&size, allocator);

		if (buffer == nullptr || size == 0)
		{
			std::cerr << "[error] metacall_inspect() returned null / zero-size buffer.\n";
			metacall_allocator_destroy(allocator);
			metacall_destroy();
			return 1;
		}

		std::cout << "Inspection data generated successfully (" << size << " bytes).\n";

		/* Write the JSON blob to disk */
		{
			std::ofstream out("inspect_output.json");
			if (!out)
			{
				std::cerr << "[error] Could not open inspect_output.json for writing.\n";
			}
			else
			{
				out.write(buffer, static_cast<std::streamsize>(size));
				std::cout << "Saved to inspect_output.json.\n";
			}
		}

		/* Also print to stdout for quick verification */
		std::cout << "\n--- inspect output ---\n"
				  << std::string(buffer, size)
				  << "\n----------------------\n";

		/* Release the buffer through the same allocator that produced it */
		metacall_allocator_free(allocator, buffer);
		metacall_allocator_destroy(allocator);

		metacall_destroy();
		return 0;
	}

	metacall_destroy();
	return 1;
}
