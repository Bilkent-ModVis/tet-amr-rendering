#include "parse_options.h"
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>

int parse_options(int argc, char **argv, Renderer_options &ro, Viewer_options &vo) {
    CLI::App app{"Tet AMR Volume Renderer"};
    argv = app.ensure_utf8(argv);

    std::string render_mode;
    app.add_option("--render_mode", render_mode, "Render mode: \"raymarcher\" or \"woodcock\"");
    app.add_option("--step_size", ro.step_size, "Raymarcher step size");
    app.add_option("--field", ro.field, "The VTK scalar field to be rendered");
    app.add_flag("--vertex_data", ro.use_vertex_data, "Use vertex data samples");

    std::vector<float> camera = {0, 0, -1.0, 0, 0, 0, 0, 1, 0};
    const auto opt_cam =
        app.add_option("--camera", camera,
                       "The camera vectors (x y z): origin interest up, given as 9 space separated floats")
            ->delimiter(' ')
            ->expected(9);
    app.add_option("--fovy", vo.fov_y, "Vertical FOV");

    app.add_option("--transfer_function", vo.tf_path, "The path to the saved transfer function file");
    app.add_option("--opacity", ro.opacity, "Opacity scale applied on top of the transfer function");

    app.add_option("--low", ro.scalar_low, "Scalar range lower value");
    app.add_option("--high", ro.scalar_high, "Scalar range higher value");
    app.add_option("--scale", ro.scale,
                   "Scales the volume, values less than 1 scales down, values greater than 1 scales up");

    app.add_option("--refinement_criteria", ro.refinement_criteria,
                   "The screenspace refinement threshold to be used for view-dependent early tree-termination");
    app.add_option("--coarse_mesh_level", ro.coarse_mesh_level,
                   "The maximum refinement level of elements contained in the forest as tree roots, default is 0, "
                   "higher levels create more roots");

    app.add_flag("--offline", ro.offline, "Render without using the interactive viewer");
    auto *offline_mode = app.add_option_group(
        "Offline mode options/flags", "These options are only meaningful when offline rendering with --offline");
    offline_mode->add_option(
        "--non_benchmark_woodcock_count", ro.non_benchmark_woodcock_count,
        "Accumulated frame count for the woodcock renderer in offline renderer non-benchmark mode");
    offline_mode->add_flag("--warmup", ro.warmup, "Warmup before running the benchmark");
    offline_mode->add_flag("--benchmark", ro.benchmark, "Time the render for the frame count given by benchmark_count");
    offline_mode->add_option("--benchmark_count", ro.benchmark_count, "The frame count for the benchmark");
    // offline woodcock non-benchmark frame count
    offline_mode->add_flag("--refinement_threshold_benchmark", ro.refinement_benchmark,
                           "Time the render for the frame count given by benchmark_count, for each refinement "
                           "threshold value from 0 to 264 at 0.5 increments");
    offline_mode->add_option("--save", ro.save_filename,
                             "The path for the resulting image from the offline renderer to be saved at, if not "
                             "provided it is saved in the working directory with a default name");
    std::vector resolution{ro.render_resolution.x, ro.render_resolution.y};
    const auto opt_res =
        offline_mode
            ->add_option("--render_resolution", resolution, "The resolution for the offline renderer, given as \"x y\"")
            ->delimiter(' ')
            ->expected(2);

    app.add_option("filename", ro.data_path, "Path to the VTU file containing the volume data")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &e) {
        std::exit(app.exit(e));
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (render_mode == "raymarcher") {
        ro.render_mode = RayMarcher;
    } else if (render_mode == "woodcock") {
        ro.render_mode = Woodcock;
    }

    if (opt_res->count() == 2) {
        ro.render_resolution.x = resolution[0];
        ro.render_resolution.y = resolution[1];
    }
    if (opt_cam->count() == 9) {
        vo.camera_orig = {camera[0], camera[1], camera[2]};
        vo.camera_interest = {camera[3], camera[4], camera[5]};
        vo.camera_up = {camera[6], camera[7], camera[8]};
    }

    return 0;
}