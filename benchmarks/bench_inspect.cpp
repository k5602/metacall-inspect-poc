#include <benchmark/benchmark.h>
#include <metacall/metacall.h>
#include <cstdlib>

class InspectFixture : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) override {
        metacall_initialize();

        #ifdef SCRIPTS_PATH
        setenv("LOADER_SCRIPT_PATH", SCRIPTS_PATH, 1);
        #endif

        const char* py_scripts[] = { "test.py" };
        metacall_load_from_file("py", py_scripts, 1, NULL);

        const char* node_scripts[] = { "test.js" };
        metacall_load_from_file("node", node_scripts, 1, NULL);
    }

    void TearDown(const benchmark::State& state) override {
        metacall_destroy();
    }
};

BENCHMARK_F(InspectFixture, BM_MetacallInspect)(benchmark::State& state) {
    for (auto _ : state) {
        size_t size = 0;
        char* inspect_str = metacall_inspect(&size, NULL);
        benchmark::DoNotOptimize(inspect_str);
        if (inspect_str) {
            free(inspect_str);
        }
    }
}
