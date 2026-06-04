#include "MD5.h"
#include "benchmark.h"
#include "common_code.h"
#include "parse_options.h"
#include "renderer.h"
#include "renderer_common.h"
#include "viewer.h"

#include <TFEditor.h>
#include <TetForest.h>
#include <file_cache.h>

#define USE_FILE_CACHE

void load_data(TetForest &forest, const Renderer_options &ro) {
    std::filesystem::path file_path{ro.data_path};

#ifdef USE_FILE_CACHE
    FileCache cache(".tet_cache");
    if (!cache.cache_exists()) {
        cache.create_cache();
    }

    std::string file_hash;
    if (file_path.extension().string() == ".vtu") {
        file_hash = MD5::hash_file(file_path.string());
    }

    if (file_path.extension().string() == ".tetz") {
        std::cout << "Reading forest from compressed file: " << file_path.string() << std::endl;
        forest = TetForest::load_compressed(file_path);
    } else if (cache.has_file(file_hash)) {
        std::cout << "Reading forest from cached file: " << cache.cache_path / file_hash << std::endl;
        forest = TetForest::load_compressed(cache.cache_path / file_hash);
    } else {
        std::cout << "Reading forest from file: " << file_path.string() << std::endl;
        forest = TetForest::from_vtu(file_path.string(), {"Ana. Solution", "Element Scalar"});
        forest.process();
        std::cout << "Saving forest to cache: " << cache.cache_path / file_hash << std::endl;
        forest.save_compressed(cache.cache_path / file_hash);
    }
#else
    std::cout << "Reading forest from file: " << file_path.string() << std::endl;
    forest = TetForest::from_vtu(file_path.string(), {"Ana. Solution", "Element Scalar"});
    forest.process();
#endif

    if (ro.scale != 1.0f) {
        scale_mesh(forest, ro.scale);
    }
}

int main(int ac, char **av) {
    Renderer_options ro;
    Viewer_options vo;
    if (parse_options(ac, av, ro, vo) != 0) {
        return 1;
    }

    TetForest forest;
    load_data(forest, ro);

    const auto renderer = std::make_unique<Renderer>();
    renderer->init(forest, ro.split_bvh);

    const auto tfEditor = std::make_unique<hs::TFEditor>();
    set_renderer_options(ro, forest, renderer.get(), tfEditor.get());

    if (!ro.offline) {
        const std::string window_name = ro.data_path;
        const auto viewer = std::make_unique<Viewer>("Renderer", renderer.get(), tfEditor.get(), window_name, &forest);
        set_viewer_options(vo, ro, forest, viewer.get());
        viewer->setWindowSize({1920, 1080});

        viewer->showAndRun();
    } else {
        const vec2i size{ro.render_resolution[0], ro.render_resolution[1]};

        set_offline_renderer_camera(*renderer, vo, forest, size);
        renderer->resize(size);

        if (!vo.tf_path.empty()) {
            tfEditor->loadFromFile(vo.tf_path.c_str());
            auto range = tfEditor->getRange();
            float scale = tfEditor->getOpacityScale();
            renderer->setOpacityScale(scale);
            renderer->setScalarRange(range);
        }

        hs::LookupTable table = *(tfEditor->getRGBAEditor()->populate_RGBA_LUT());
        auto cmap = tfEditor->getColorMap();
        renderer->setTransferFunction(cmap);

        const int field_no = get_field_no(forest, ro.field);
        renderer->setCellData(forest.cell_data[field_no]);

        uint32_t *fbPointer{nullptr};
        auto res = cudaMallocManaged(&fbPointer, size.x * size.y * sizeof(uint32_t));
        if (res != cudaSuccess) {
            std::cerr << "cudaMallocManaged failed: " << cudaGetErrorString(res) << std::endl;
            return 1;
        }

        if (ro.benchmark) {
            render_time_benchmark(*renderer, ro, fbPointer);
        } else {
            int target_frame_count = ro.render_mode == Woodcock ? ro.non_benchmark_woodcock_count : 1;
            render(*renderer, ro, fbPointer, target_frame_count);
        }

        save_image(ro, fbPointer, size);
        cudaFree(fbPointer);
    }

    return 0;
}
