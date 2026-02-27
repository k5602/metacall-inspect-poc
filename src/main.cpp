#include <metacall/metacall.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>

void setup_metacall() {
    metacall_initialize();

    // Set script paths if provided
    #ifdef SCRIPTS_PATH
    setenv("LOADER_SCRIPT_PATH", SCRIPTS_PATH, 1);
    #endif

    const char* py_scripts[] = { "test.py" };
    if (metacall_load_from_file("py", py_scripts, 1, NULL) != 0) {
        std::cerr << "Warning: Failed to load Python script." << std::endl;
    }

    const char* node_scripts[] = { "test.js" };
    if (metacall_load_from_file("node", node_scripts, 1, NULL) != 0) {
        std::cerr << "Warning: Failed to load NodeJS script." << std::endl;
    }
}

void teardown_metacall() {
    metacall_destroy();
}

int main() {
    std::cout << "Initializing MetaCall..." << std::endl;
    setup_metacall();

    size_t size = 0;
    char* inspect_data = metacall_inspect(&size, NULL);

    if (inspect_data) {
        std::cout << "Inspection data generated successfully (" << size << " bytes)." << std::endl;

        std::ofstream out("inspect_output.json");
        out << inspect_data;
        out.close();

        std::cout << "Saved to inspect_output.json." << std::endl;
        free(inspect_data); // metacall_inspect returns malloc'd buffer when allocator is NULL
    } else {
        std::cerr << "Failed to generate inspect data." << std::endl;
        teardown_metacall();
        return 1;
    }

    teardown_metacall();
    return 0;
}
