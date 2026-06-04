#include "renderer_common.h"
#include <cstring>
#include <stb/stb_image_write.h>

std::array<vec3f, 3> get_centered_camera_and_speed(const TetForest &forest, const float fovy, float *speed) {
    box3d bbox;
    for (const auto &v : forest.vertices) {
        bbox.extend({v[0], v[1], v[2]});
    }
    const auto bbox_size = bbox.size();
    const auto center_to_cam_distance =
        2 * static_cast<float>((bbox_size[2] / 2) + cos(viewer::toRadian(fovy / 2)) * 0.6 * bbox_size[1]);

    std::array camera{
        vec3f{bbox.center()}
        - vec3f{0, 0, center_to_cam_distance}, // origin
        vec3f{bbox.center()}, // interest
        vec3f{0.f, 1.f, 0.f}, // up
    };

    const double diagonal =
        sqrt(bbox_size[0] * bbox_size[0] + bbox_size[1] * bbox_size[1] + bbox_size[2] * bbox_size[2]);
    if (speed) {
        *speed = static_cast<float>(0.05035 * diagonal + 0.607);
    }

    return camera;
}

int get_field_no(const TetForest &forest, const std::string &field) {
    int field_no = 0;
    auto f_it = std::ranges::find(forest.field_names, field);
    if (f_it != forest.field_names.end()) {
        field_no = static_cast<int>(std::distance(forest.field_names.begin(), f_it));
    }
    return field_no;
}

void set_renderer_options(const Renderer_options &ro, const TetForest &forest, const Renderer *renderer,
                          hs::TFEditor *tfEditor) {
    renderer->setStepSize(ro.step_size);
    renderer->setRefinementCriteria(ro.refinement_criteria);
    renderer->setDataRenderMode(ro.use_vertex_data ? 1 : 0);
    renderer->setOpacityScale(ro.opacity);
    renderer->setRenderMode(ro.render_mode);
    renderer->setBackgroundColor(ro.background_color);

    const int field_no = get_field_no(forest, ro.field);
    renderer->setDataFieldNo(field_no);

    const interval range{std::max(ro.scalar_low, forest.field_ranges[field_no].first - 0.001f),
                         std::min(ro.scalar_high, forest.field_ranges[field_no].second)};
    tfEditor->setRange(range);
    renderer->setScalarRange(range);
}

viewer::Camera create_camera(const Viewer_options &vo, const TetForest &forest, vec3f &interest) {
    viewer::Camera cam;
    float speed{0.0f};
    const float fov = vo.fov_y.has_value() ? vo.fov_y.value() : viewer::toDegrees(acosf(0.66));
    auto camera = get_centered_camera_and_speed(forest, fov, &speed);
    if (vo.camera_orig.has_value() || vo.camera_interest.has_value() || vo.camera_up.has_value()) {
        cam.setOrientation(vo.camera_orig.value(),     // origin
                           vo.camera_interest.value(), // interest
                           vo.camera_up.value(),       // up
                           fov);                       // fov-y
        interest = vo.camera_interest.value();
    } else {
        cam.setOrientation(camera[0], // origin
                           camera[1], // interest
                           camera[2], // up
                           fov);      // fov-y
        interest = camera[1];
    }
    cam.motionSpeed = speed;
    return cam;
}

void set_offline_renderer_camera(Renderer &renderer, const Viewer_options &vo, const TetForest &forest,
                                 const vec2i &size) {
    vec3f interest;
    viewer::Camera cam = create_camera(vo, forest, interest);
    cam.aspect = static_cast<float>(size.x) / static_cast<float>(size.y);
    viewer::SimpleCamera simple_cam(cam);
    BasicCamera basic_cam;
    basic_cam.org = simple_cam.lens.center;
    basic_cam.llc = simple_cam.screen.lower_left;
    basic_cam.horiz = simple_cam.screen.horizontal;
    basic_cam.vert = simple_cam.screen.vertical;
    renderer.setCamera(basic_cam);
}

void set_viewer_options(const Viewer_options &vo, const Renderer_options &ro, const TetForest &forest, Viewer *viewer) {
    viewer->refinementCriteria = ro.refinement_criteria;
    viewer->vertexData = ro.use_vertex_data;

    vec3f interest;
    const viewer::Camera cam = create_camera(vo, forest, interest);
    viewer->setCameraOrientation(cam.getFrom(), interest, cam.getUp(), cam.getFovyInDegrees());
    viewer->camera.motionSpeed = cam.motionSpeed;
    viewer->enableFlyMode();
    viewer->enableInspectMode();

    if (!vo.cmap.empty()) {
        viewer->tfEditor->selectCMap(vo.cmap);
    }
    if (!vo.tf_path.empty()) {
        viewer->tfEditor->loadFromFile(vo.tf_path.c_str());
    }
    viewer->tfEditor->setOpacityScale(ro.opacity);

    viewer->field_no = get_field_no(forest, ro.field);

    viewer->renderMode = ro.render_mode;
    viewer->stepSize = ro.step_size;
}

void scale_mesh(TetForest &forest, const float scale) {
    for (auto &v : forest.vertices) {
        v = v * scale;
    }
}

void render(Renderer &renderer, const Renderer_options &ro, uint32_t *fbPointer, const int target_frame_count) {
    int accumulationCount = 0;

    for (int i = 0; i < target_frame_count; ++i) {
        renderer.setAccumulation(accumulationCount);
        renderer.render(fbPointer);
        ++accumulationCount;
    }
}

void save_image(const Renderer_options &ro, const uint32_t *fbPointer, const vec2i &size) {
    std::string filename = ro.save_filename;
    if (filename.empty()) {
        std::time_t time = std::time({});
        char timeString[std::size("yyyy-mm-ddThh:mm:ss")];
        std::strftime(std::data(timeString), std::size(timeString), "%FT%T", std::localtime(&time));
        std::ostringstream oss;
        oss << ro.data_path.substr(0, ro.data_path.find('.')) << "-" << timeString << ".png";
        filename = oss.str();
    }

    // flip the buffer so the output image is consistent with the viewer
    std::vector<uint32_t> flipped_fb(size.x * size.y);
    for (int y = 0; y < size.y; ++y) {
        std::memcpy(flipped_fb.data() + y * size.x, fbPointer + (size.y - y - 1) * size.x, sizeof(uint32_t) * size.x);
    }

    stbi_write_png(filename.c_str(), size.x, size.y, 4, flipped_fb.data(), sizeof(uint32_t) * size.x);
}
