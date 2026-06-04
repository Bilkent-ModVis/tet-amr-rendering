#pragma once
#include "owl_viewer_imgui.h"
#include "renderer.h"

class Viewer : public OWLViewerImgui {
  public:
    Renderer *renderer;
    hs::TFEditor *tfEditor;
    float stepSize = 1;
    double prevTime;
    bool vertexData;
    float refinementCriteria;
    unsigned int accumulationCount = 0;
    RenderMode renderMode;
    const TetForest *forest;
    int field_no = 0;
    const std::string filename;
    std::array<double, 16> frame_times;
    size_t frame_time_pos{0};

    explicit Viewer(const std::string &title, Renderer *renderer, hs::TFEditor *tfEditor, const std::string &filename,
                    const TetForest *forest)
        : OWLViewerImgui(title), renderer(renderer), tfEditor(tfEditor), prevTime(owl::getCurrentTime()),
          vertexData(false), renderMode(RayMarcher), filename(filename), forest(forest) {}
    void render() override;
    void resize(const vec2i &newSize) override;
    void cameraChanged() override;
    void imgui_ui_func() override;
    void frame_update() override;
    void set_camera_location_and_speed();
};
