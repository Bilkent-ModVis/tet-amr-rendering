#include "viewer.h"

#include <imgui.h>

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
    ImGui::Begin("Transfer Function");
    tfEditor->drawImmediate();

    if (ImGui::BeginCombo("Field", forest->field_names[field_no].c_str())) {
        for (int i = 0; i < forest->field_names.size(); ++i) {
            bool isSelected = field_no == i;
            if (ImGui::Selectable(forest->field_names[i].c_str(), isSelected)) {
                field_no = i;
                renderer->setCellData(forest->cell_data[field_no]);
                renderer->setVertexData(forest->vertex_data[field_no]);
                renderer->setScalarRange({forest->field_ranges[field_no].first, forest->field_ranges[field_no].second});

                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    bool temp_vertexData = vertexData;
    ImGui::Checkbox("Use Vertex Data", &vertexData);
    if (vertexData != temp_vertexData) {
        int mode = vertexData;
        std::cout << "setting mode to: " << mode << std::endl;
        renderer->setDataRenderMode(mode);
        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
    }

    if (ImGui::BeginTabBar("TabBar")) {
        if (ImGui::BeginTabItem("Marcher")) {
            if (renderMode != RayMarcher) {
                renderMode = RayMarcher;
                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
                renderer->setRenderMode(renderMode);
            }
            float temp_stepSize = stepSize;
            ImGui::SliderFloat("Step Size", &stepSize, 0.0001f, 2.0f);
            if (stepSize != temp_stepSize) {
                renderer->setStepSize(stepSize);
                accumulationCount = 0;
                renderer->setAccumulation(accumulationCount);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Woodcock")) {
            if (renderMode != Woodcock) {
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
}

void Viewer::frame_update() {
    setTitle(filename + " - FPS: " + std::to_string(1 / (owl::getCurrentTime() - prevTime)));
    prevTime = owl::getCurrentTime();
    if (tfEditor->cmapUpdated()) {
        auto cmap = tfEditor->getColorMap();
        renderer->setTransferFunction(cmap);
        accumulationCount = 0;
        renderer->setAccumulation(accumulationCount);
    }

    if (tfEditor->rangeUpdated()) {
        auto range = tfEditor->getRange();
        renderer->setScalarRange(range);
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