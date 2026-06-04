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

void refinement_criteria_benchmark(Renderer &renderer, const Renderer_options &ro, uint32_t *fbPointer) {
    unsigned int accumulationCount = 0;

    std::cout << "Warming up" << std::endl;
    for (int i = 0; i < ro.benchmark_count; ++i) {
        renderer.setAccumulation(accumulationCount);
        renderer.render(fbPointer);
        ++accumulationCount;
    }

    std::vector<long long> times;
    std::vector<float> thresholds;

    for (int i = 0; i <= (256 + 8) * 2; ++i) {
        const float ref_crit = .5f * i;
        renderer.setRefinementCriteria(ref_crit);
        accumulationCount = 0;
        std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
        for (int j = 0; j < ro.benchmark_count; ++j) {
            renderer.setAccumulation(accumulationCount);
            renderer.render(fbPointer);
            ++accumulationCount;
        }
        std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();

        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        times.push_back(time);
        thresholds.push_back(ref_crit);
        std::cout << "Ref. crit: " << ref_crit << " took: " << time << " ms" << std::endl;
    }

    std::cout << "Benchmark frame count: " << ro.benchmark_count << std::endl;
    std::cout << "Refinement threshold values:" << std::endl;
    for (size_t i = 0; i < thresholds.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << thresholds[i];
    }
    std::cout << std::endl;
    std::cout << "Rendering times:" << std::endl;
    for (size_t i = 0; i < times.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << times[i];
    }
    std::cout << std::endl;
}
