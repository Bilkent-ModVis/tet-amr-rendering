#include "benchmark.h"

void render_time_benchmark(Renderer &renderer, const Renderer_options &ro, uint32_t *fbPointer) {
    unsigned int accumulationCount = 0;

    if (ro.warmup) {
        std::cout << "Warming up" << std::endl;
        for (int i = 0; i < std::max(ro.benchmark_count / 10, 100); ++i) {
            renderer.setAccumulation(accumulationCount);
            renderer.render(fbPointer);
            ++accumulationCount;
        }
        accumulationCount = 0;
    }
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
    for (int i = 0; i < ro.benchmark_count; ++i) {
        renderer.setAccumulation(accumulationCount);
        renderer.render(fbPointer);
        ++accumulationCount;
    }
    std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();

    std::cout << "Benchmark count: " << ro.benchmark_count << std::endl;
    std::cout << "Rendering took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms / " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us"
              << std::endl;
}
