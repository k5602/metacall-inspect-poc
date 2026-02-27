#include <gtest/gtest.h>
#include <metacall/metacall.h>
#include <string>
#include <cstdlib>

class InspectTest : public ::testing::Test {
protected:
    void SetUp() override {
        metacall_initialize();

        #ifdef SCRIPTS_PATH
        setenv("LOADER_SCRIPT_PATH", SCRIPTS_PATH, 1);
        #endif

        const char* py_scripts[] = { "test.py" };
        metacall_load_from_file("py", py_scripts, 1, NULL);

        const char* node_scripts[] = { "test.js" };
        metacall_load_from_file("node", node_scripts, 1, NULL);
    }

    void TearDown() override {
        metacall_destroy();
    }
};

TEST_F(InspectTest, HasPythonAndNodeFunctions) {
    size_t size = 0;
    char* inspect_str = metacall_inspect(&size, NULL);

    ASSERT_NE(inspect_str, nullptr);
    ASSERT_GT(size, 0);

    std::string data(inspect_str);
    free(inspect_str);

    // Check if python function exists
    EXPECT_NE(data.find("python_test_function"), std::string::npos);
    EXPECT_NE(data.find("PythonTestClass"), std::string::npos);

    // Check if nodejs function exists
    EXPECT_NE(data.find("node_test_function"), std::string::npos);
    EXPECT_NE(data.find("NodeTestClass"), std::string::npos);
}
