#pragma once
#include "render_mode_enums.h"

#include <owl/common.h>

#include <array>
#include <optional>
#include <string>

struct Renderer_options {
    std::string data_path;
    float scalar_low{std::numeric_limits<float>::lowest()};
    float scalar_high{std::numeric_limits<float>::max()};
    float opacity{100.0f};
    std::string field;
    float refinement_criteria{.0f};
    bool use_vertex_data{false};
    float scale{1.0f};
    RenderMode render_mode{RayMarcher};
    float step_size{1.0f};
    owl::vec3f background_color{0.3215f, 0.3411f, 0.4313f};
    bool offline{false};
    bool warmup{false};
    bool benchmark{false};
    bool refinement_benchmark{false};
    int benchmark_count{0};
    int non_benchmark_woodcock_count{0};
    owl::vec2i render_resolution{1920, 1080};
    std::string save_filename;
    int coarse_mesh_level{0};
};

struct Viewer_options {
    std::optional<owl::vec3f> camera_orig = std::nullopt;
    std::optional<owl::vec3f> camera_interest = std::nullopt;
    std::optional<owl::vec3f> camera_up = std::nullopt;
    std::optional<float> fov_y = std::nullopt;
    std::string cmap;
    std::string tf_path;
};

int parse_options(int argc, char **argv, Renderer_options &ro, Viewer_options &vo);
