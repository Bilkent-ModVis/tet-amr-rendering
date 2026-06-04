#include "viewer.h"
#include "density.cuh"

#include <imgui.h>

#include <format>

void Viewer::render() {
    renderer->setAccumulation(accumulationCount);
    renderer->render(fbPointer);
    ++accumulationCount;
};

void Viewer::resize(const vec2i &newSize) {
    OWLViewerImgui::resize(newSize);
    renderer->resize(newSize);
};

void Viewer::cameraChanged() {
    const auto camera = getSimplifiedCamera();
    BasicCamera cam;
    cam.org = camera.lens.center;
    cam.llc = camera.screen.lower_left;
    cam.horiz = camera.screen.horizontal;
    cam.vert = camera.screen.vertical;
    renderer->setCamera(cam);
    accumulationCount = 0;
    renderer->setAccumulation(accumulationCount);
};

void Viewer::imgui_ui_func() {
    static bool firstTime = true;

    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoBackground;
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoResize;
    flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    flags |= ImGuiWindowFlags_NoInputs;

    bool openPtr = true;
    if (ImGui::Begin("invisible", &openPtr, flags)) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const auto clipMin = drawList->GetClipRectMin();

        const auto toWindowCoord = [clipMin](const ImVec2 v) { return ImVec2{v.x + clipMin.x, v.y + clipMin.y}; };
        const auto drawText = [&](const ImVec2 pos, const std::string &str,
                                  ImVec4 color = {255.f, 255.f, 255.f, 255.f}) {
            drawList->AddText(font, 25.f, toWindowCoord(pos), ImGui::ColorConvertFloat4ToU32(color), str.c_str());
        };

        std::string tmp_filename = filename;
        if (filename == "large_large_engine.vtu") {
            tmp_filename = "engine.vtu";
        }

        drawText({10.f, 10.f}, std::format("File path: {}", tmp_filename));

        drawText({10.f, 35.f}, std::format("Resolution: {}x{}", renderer->frame.size.x, renderer->frame.size.y));

        double avg_frame_time = 0;
        for (auto t : frame_times) {
            avg_frame_time += t;
        }
        avg_frame_time /= frame_times.size();

        drawText({10.f, 60.f}, std::format("Avg. FPS: {:.2f}", 1.f / avg_frame_time));

        drawText({10.f, 85.f}, std::format("Renderer: {}", renderMode == Woodcock ? "Woodcock" : "Ray marcher"));
        float offset = 110.f;
        if (refinementCriteria > 0) {
            drawText({10.f, offset}, std::format("Refinement threshold: {:.1f}", refinementCriteria));
            offset += 25;
        }
        if (!vertexData)
            drawText({10.f, offset}, "Using cell scalars");
    }
    ImGui::End();
    ImGui::PopStyleVar(1);

    ImGui::Begin("Transfer Function");
    tfEditor->drawImmediate();

    if (ImGui::BeginCombo("Field", forest->field_names[field_no].c_str())) {
        for (int i = 0; i < forest->field_names.size(); ++i) {
            bool isSelected = field_no == i;
            if (ImGui::Selectable(forest->field_names[i].c_str(), isSelected)) {
                field_no = i;
                renderer->setCellData(forest->cell_data[field_no]);

                std::vector<TetForest::Float> tmp_vtx_data(4 * forest->num_elements);
                for (TetForest::TetID_t j = 0; j < forest->num_elements; ++j) {
                    const auto idx = forest->get_indices(i);
                    tmp_vtx_data[4 * j + 0] = forest->vertex_data[field_no][idx[0]];
                    tmp_vtx_data[4 * j + 1] = forest->vertex_data[field_no][idx[1]];
                    tmp_vtx_data[4 * j + 2] = forest->vertex_data[field_no][idx[2]];
                    tmp_vtx_data[4 * j + 3] = forest->vertex_data[field_no][idx[3]];
                }
                renderer->setVertexData(tmp_vtx_data);
                renderer->setScalarRange({forest->field_ranges[field_no].first, forest->field_ranges[field_no].second});

                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    float temp_refinementCriteria = refinementCriteria;
    ImGui::SliderFloat("Refinement C.", &refinementCriteria, .0f, 256.f);
    if (refinementCriteria != temp_refinementCriteria) {
        renderer->setRefinementCriteria(refinementCriteria);
        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
    }

    bool temp_vertexData = vertexData;
    ImGui::Checkbox("Use Vertex Data", &vertexData);
    if (vertexData != temp_vertexData) {
        int mode = vertexData;
        renderer->setDataRenderMode(mode);
        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
    }

    if (ImGui::BeginTabBar("TabBar")) {
        if (ImGui::BeginTabItem("Marcher", nullptr,
                                (firstTime && renderMode == RayMarcher) ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (renderMode != RayMarcher && !firstTime) {
                renderMode = RayMarcher;
                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
                renderer->setRenderMode(renderMode);
            }
            float temp_stepSize = stepSize;
            ImGui::SliderFloat("Step Size", &stepSize, 0.01f, 2.5f);
            if (stepSize != temp_stepSize) {
                renderer->setStepSize(stepSize);
                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Woodcock", nullptr,
                                (firstTime && renderMode == Woodcock) ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (renderMode != Woodcock && !firstTime) {
                renderMode = Woodcock;
                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
                renderer->setRenderMode(renderMode);
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
    firstTime = false;
}

void camera_move(const float step, viewer::OWLViewer &viewer) {
    viewer::Camera &fc = viewer.camera;

    const vec3f poi = fc.getPOI();
    fc.poiDistance = min(std::numeric_limits<float>::infinity(), max(0.001f, fc.poiDistance - step * fc.motionSpeed));
    fc.position = poi + fc.poiDistance * fc.frame.vz;
    viewer.updateCamera();
}

void camera_rotate(const float deg_u, const float deg_v, viewer::OWLViewer &viewer) {
    float rad_u = -(float)M_PI / 180.f * deg_u;
    float rad_v = -(float)M_PI / 180.f * deg_v;

    viewer::Camera &fc = viewer.camera;

    const vec3f poi = fc.getPOI();
    fc.frame = linear3f::rotate(fc.frame.vy, rad_u) * linear3f::rotate(fc.frame.vx, rad_v) * fc.frame;

    if (fc.forceUp)
        fc.forceUpFrame();

    fc.position = poi + fc.poiDistance * fc.frame.vz;

    viewer.updateCamera();
}

void Viewer::frame_update() {
    double render_time = owl::getCurrentTime() - prevTime;
    if (continue_rotate_left) {
        camera_rotate(25 * render_time, 0, *this);
    } else if (continue_rotate_right) {
        camera_rotate(-25 * render_time, 0, *this);
    }

    if (continue_move_forward) {
        camera_move(3.75 * render_time, *this);
    } else if (continue_move_back) {
        camera_move(-3.75 * render_time, *this);
    }

    setTitle(filename + " - FPS: " + std::to_string(1 / render_time));
    frame_times[frame_time_pos++] = render_time;
    frame_time_pos %= frame_times.size();
    prevTime = owl::getCurrentTime();
    if (tfEditor->cmapUpdated()) {
        auto cmap = tfEditor->getColorMap();
        renderer->setTransferFunction(cmap);
        Density_CUDA::calculate_density_cuda(tfEditor->getRange(), renderer->cellDensityBuffer,
                                             renderer->vertexDensityBuffer, renderer->cellScalarRangeBuffer,
                                             renderer->vertexScalarRangeBuffer, renderer->cmapTexture,
                                             renderer->numTrees);

        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
    }

    if (tfEditor->rangeUpdated()) {
        auto range = tfEditor->getRange();
        renderer->setScalarRange(range);
        Density_CUDA::calculate_density_cuda(tfEditor->getRange(), renderer->cellDensityBuffer,
                                             renderer->vertexDensityBuffer, renderer->cellScalarRangeBuffer,
                                             renderer->vertexScalarRangeBuffer, renderer->cmapTexture,
                                             renderer->numTrees);
        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
    }

    if (tfEditor->opacityUpdated()) {
        float scale = tfEditor->getOpacityScale();
        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
        renderer->setOpacityScale(scale);
    }
}

void Viewer::set_camera_location_and_speed() {
    box3d bbox;
    for (const auto &v : forest->vertices) {
        bbox.extend({v[0], v[1], v[2]});
    }
    const auto bbox_size = bbox.size();
    const float fovy_deg = viewer::toDegrees(acosf(0.66));
    const auto center_to_cam_distance =
        2 * static_cast<float>((bbox_size[2] / 2) + cos(viewer::toRadian(fovy_deg / 2)) * 0.6 * bbox_size[1]);

    setCameraOrientation(vec3f{bbox.center()} - vec3f{0, 0, center_to_cam_distance}, // origin
                         vec3f{bbox.center()},                                       // interest
                         vec3f{0.f, 1.f, 0.f},                                       // up
                         fovy_deg);                                                  // fov-y
    const double diagonal =
        sqrt(bbox_size[0] * bbox_size[0] + bbox_size[1] * bbox_size[1] + bbox_size[2] * bbox_size[2]);
    camera.motionSpeed = static_cast<float>(0.05035 * diagonal + 0.607);
}