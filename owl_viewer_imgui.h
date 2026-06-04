#pragma once
#include "owlViewer/OWLViewer.h"
#include <functional>

struct OWLViewerImgui : public owl::viewer::OWLViewer {
    inline static double framerateCap = 60;
    inline static bool continue_rotate_right{false};
    inline static bool continue_rotate_left{false};
    inline static bool continue_move_forward{false};
    inline static bool continue_move_back{false};
    struct ImFont *font{nullptr};

    OWLViewerImgui();
    OWLViewerImgui(const std::string &title);
    void showAndRun();
    void showAndRun(std::function<bool()> keepgoing);
    virtual void imgui_ui_func();
    virtual void frame_update();

    virtual ~OWLViewerImgui() = default;

  private:
    std::function<void()> imgui_menu_func;
};