#include <gtest/gtest.h>

#include <metacall/metacall.h>
#include <metacall/metacall_allocator.h>

#include <cstdlib>
#include <string>

/* ---------------------------------------------------------------------------
 * Runtime environment bootstrap
 *
 * When running from the build tree the loader/serial shared libraries and
 * script directory must be on the appropriate search paths.  Environment
 * variables are set only if the caller has not already overridden them.
 * --------------------------------------------------------------------------- */
static void setup_metacall_env()
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
#endif /* METACALL_BUILD_DIR */

#ifdef SCRIPTS_PATH
	if (getenv("LOADER_SCRIPT_PATH") == nullptr)
		setenv("LOADER_SCRIPT_PATH", SCRIPTS_PATH, 1);
#endif /* SCRIPTS_PATH */
}

/* ---------------------------------------------------------------------------
 * Test fixture
 * --------------------------------------------------------------------------- */
class InspectTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		setup_metacall_env();

		ASSERT_EQ(0, metacall_initialize())
		    << "metacall_initialize() failed — check loader/serial paths";

		/* Load Python script; non-fatal if py loader was not built. */
		const char *py_scripts[] = { "test.py" };
		metacall_load_from_file("py", py_scripts, 1, nullptr);

		/* Load NodeJS script; non-fatal if node loader was not built. */
		const char *node_scripts[] = { "test.js" };
		metacall_load_from_file("node", node_scripts, 1, nullptr);
	}

	void TearDown() override
	{
		metacall_destroy();
	}
};

/* ---------------------------------------------------------------------------
 * TC-1: metacall_inspect returns a non-null, non-empty JSON buffer.
 * --------------------------------------------------------------------------- */
TEST_F(InspectTest, InspectReturnsNonEmptyBuffer)
{
	struct metacall_allocator_std_type std_ctx = {
		&std::malloc,
		&std::realloc,
		&std::free
	};

	void *allocator = metacall_allocator_create(
	    METACALL_ALLOCATOR_STD, static_cast<void *>(&std_ctx));

	ASSERT_NE(allocator, nullptr) << "metacall_allocator_create() returned null";

	size_t size   = 0;
	char  *buffer = metacall_inspect(&size, allocator);

	EXPECT_NE(buffer, nullptr) << "metacall_inspect() returned null buffer";
	EXPECT_GT(size, static_cast<size_t>(0)) << "metacall_inspect() returned zero size";

	if (buffer != nullptr)
	{
		/* Sanity: the output must be a JSON object. */
		std::string data(buffer, size);
		EXPECT_FALSE(data.empty());
		EXPECT_EQ(data.front(), '{') << "Expected JSON object, got: " << data.substr(0, 32);

		/* Release through the allocator that produced the buffer — never bare free(). */
		metacall_allocator_free(allocator, buffer);
	}

	metacall_allocator_destroy(allocator);
}

/* ---------------------------------------------------------------------------
 * TC-2: inspect output contains the Python symbols declared in test.py.
 *
 * Skipped automatically when the 'py' tag is absent from the inspect output
 * (loader not built), so the test suite remains green in partial builds.
 * --------------------------------------------------------------------------- */
TEST_F(InspectTest, HasPythonFunctions)
{
	struct metacall_allocator_std_type std_ctx = {
		&std::malloc,
		&std::realloc,
		&std::free
	};

	void *allocator = metacall_allocator_create(
	    METACALL_ALLOCATOR_STD, static_cast<void *>(&std_ctx));

	ASSERT_NE(allocator, nullptr);

	size_t size   = 0;
	char  *buffer = metacall_inspect(&size, allocator);

	ASSERT_NE(buffer, nullptr);
	ASSERT_GT(size, static_cast<size_t>(0));

	std::string data(buffer, size);
	metacall_allocator_free(allocator, buffer);
	metacall_allocator_destroy(allocator);

	if (data.find("\"py\"") == std::string::npos)
	{
		GTEST_SKIP() << "Python loader not present in this build — skipping Python symbol checks";
	}

	EXPECT_NE(data.find("python_test_function"), std::string::npos)
	    << "python_test_function not found in inspect output:\n"
	    << data;

	EXPECT_NE(data.find("PythonTestClass"), std::string::npos)
	    << "PythonTestClass not found in inspect output:\n"
	    << data;
}

/* ---------------------------------------------------------------------------
 * TC-3: inspect output contains the NodeJS symbols declared in test.js.
 *
 * Same skip-on-absent-loader policy as TC-2.
 * --------------------------------------------------------------------------- */
TEST_F(InspectTest, HasNodeFunctions)
{
	struct metacall_allocator_std_type std_ctx = {
		&std::malloc,
		&std::realloc,
		&std::free
	};

	void *allocator = metacall_allocator_create(
	    METACALL_ALLOCATOR_STD, static_cast<void *>(&std_ctx));

	ASSERT_NE(allocator, nullptr);

	size_t size   = 0;
	char  *buffer = metacall_inspect(&size, allocator);

	ASSERT_NE(buffer, nullptr);
	ASSERT_GT(size, static_cast<size_t>(0));

	std::string data(buffer, size);
	metacall_allocator_free(allocator, buffer);
	metacall_allocator_destroy(allocator);

	if (data.find("\"node\"") == std::string::npos)
	{
		GTEST_SKIP() << "NodeJS loader not present in this build — skipping Node symbol checks";
	}

	EXPECT_NE(data.find("node_test_function"), std::string::npos)
	    << "node_test_function not found in inspect output:\n"
	    << data;

	EXPECT_NE(data.find("NodeTestClass"), std::string::npos)
	    << "NodeTestClass not found in inspect output:\n"
	    << data;
}

/* ---------------------------------------------------------------------------
 * TC-4: calling metacall_inspect() multiple times produces consistent results
 *        and does not leak / corrupt memory.
 * --------------------------------------------------------------------------- */
TEST_F(InspectTest, InspectIsIdempotent)
{
	struct metacall_allocator_std_type std_ctx = {
		&std::malloc,
		&std::realloc,
		&std::free
	};

	void *allocator = metacall_allocator_create(
	    METACALL_ALLOCATOR_STD, static_cast<void *>(&std_ctx));

	ASSERT_NE(allocator, nullptr);

	std::string first_result;

	for (int i = 0; i < 3; ++i)
	{
		size_t size   = 0;
		char  *buffer = metacall_inspect(&size, allocator);

		ASSERT_NE(buffer, nullptr) << "iteration " << i;
		ASSERT_GT(size, static_cast<size_t>(0)) << "iteration " << i;

		std::string result(buffer, size);
		metacall_allocator_free(allocator, buffer);

		if (i == 0)
		{
			first_result = result;
		}
		else
		{
			EXPECT_EQ(result, first_result)
			    << "inspect output differed on iteration " << i;
		}
	}

	metacall_allocator_destroy(allocator);
}
