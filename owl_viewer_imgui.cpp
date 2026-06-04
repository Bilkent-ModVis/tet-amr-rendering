#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "owl_viewer_imgui.h"

// TODO: add an option to select style
static void init_imgui(GLFWwindow *window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    const char *glsl_version = "#version 130";
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

/*! callback for a window resizing event */
static void glfwindow_reshape_cb(GLFWwindow *window, int width, int height) {
    auto *gw = static_cast<owl::viewer::OWLViewer *>(glfwGetWindowUserPointer(window));
    assert(gw);
    gw->resize(owl::vec2i(width, height));
}

/*! callback for a key press */
static void glfwindow_char_cb(GLFWwindow *window, unsigned int _key) {
    auto *gw = static_cast<owl::viewer::OWLViewer *>(glfwGetWindowUserPointer(window));
    assert(gw);
    ImGuiIO io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        char key = static_cast<char>(_key);
        if (key == 'u' || key == 'U') {
            OWLViewerImgui::framerateCap = OWLViewerImgui::framerateCap == 60 ? std::numeric_limits<double>::max() : 60;
        } else if (key == '[') {
            OWLViewerImgui::continue_move_forward = !OWLViewerImgui::continue_move_forward;
            OWLViewerImgui::continue_move_back = false;
        } else if (key == ']') {
            OWLViewerImgui::continue_move_back = !OWLViewerImgui::continue_move_back;
            OWLViewerImgui::continue_move_forward = false;
        } else if (key == '\'') {
            OWLViewerImgui::continue_rotate_right = !OWLViewerImgui::continue_rotate_right;
            OWLViewerImgui::continue_rotate_left = false;
        } else if (key == ';') {
            OWLViewerImgui::continue_rotate_left = !OWLViewerImgui::continue_rotate_left;
            OWLViewerImgui::continue_rotate_right = false;
        } else {
            gw->key(key, gw->getMousePos());
        }
    }
}

/*! callback for a key press */
static void glfwindow_key_cb(GLFWwindow *window, int key, int scancode, int action, int mods) {
    auto *gw = static_cast<owl::viewer::OWLViewer *>(glfwGetWindowUserPointer(window));
    assert(gw);
    ImGuiIO io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (action == GLFW_PRESS) {
            gw->special(key, mods, gw->getMousePos());
        }
    }
}

/*! callback for _moving_ the mouse to a new position */
static void glfwindow_mouseMotion_cb(GLFWwindow *window, double x, double y) {
    auto *gw = static_cast<owl::viewer::OWLViewer *>(glfwGetWindowUserPointer(window));
    assert(gw);
    ImGuiIO io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        gw->mouseMotion(owl::vec2i((int)x, (int)y));
    }
}

/*! callback for pressing _or_ releasing a mouse button*/
static void glfwindow_mouseButton_cb(GLFWwindow *window, int button, int action, int mods) {
    auto *gw = static_cast<owl::viewer::OWLViewer *>(glfwGetWindowUserPointer(window));
    assert(gw);
    ImGuiIO io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        gw->mouseButton(button, action, mods);
    }
}

OWLViewerImgui::OWLViewerImgui() : OWLViewer("Viewer") {}

OWLViewerImgui::OWLViewerImgui(const std::string &title) : OWLViewer(title) {}

void OWLViewerImgui::showAndRun() {
    showAndRun([]() { return true; }); // run until closed manually
}

void OWLViewerImgui::showAndRun(std::function<bool()> keepgoing) {
    int width, height;
    glfwGetFramebufferSize(handle, &width, &height);
    resize(owl::vec2i(width, height));

    init_imgui(handle);

    ImGui::GetIO().Fonts->AddFontDefault();
    font = ImGui::GetIO().Fonts->AddFontFromFileTTF("../fonts/Roboto-Regular.ttf", 25.0f, nullptr,
                                                    ImGui::GetIO().Fonts->GetGlyphRangesDefault());

    glfwSetFramebufferSizeCallback(handle, glfwindow_reshape_cb);
    glfwSetMouseButtonCallback(handle, glfwindow_mouseButton_cb);
    glfwSetKeyCallback(handle, glfwindow_key_cb);
    glfwSetCharCallback(handle, glfwindow_char_cb);
    glfwSetCursorPosCallback(handle, glfwindow_mouseMotion_cb);

    // During imgui initialization we don't install callbacks
    // instead we do it after owl viewer has set its own callbacks
    // so that imgui callbacks call the other callbacks inside them
    ImGui_ImplGlfw_InstallCallbacks(handle);

    double time = owl::getCurrentTime();
    double prevTime = time;

    while (!glfwWindowShouldClose(handle) && keepgoing()) {
        static double lastCameraUpdate = -1.f;
        if (camera.lastModified != lastCameraUpdate) {
            cameraChanged();
            lastCameraUpdate = camera.lastModified;
        }

        time = owl::getCurrentTime();
        if (time - prevTime < 1.f / framerateCap) {
            continue;
        }
        prevTime = time;

        render();
        draw();

        // imgui frame setup and draw
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // call the virtual imgui_func function
        imgui_ui_func();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(handle);
        glfwPollEvents();
        frame_update();
    }

    // imgui cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(handle);
    glfwTerminate();
}

void OWLViewerImgui::imgui_ui_func() {}
void OWLViewerImgui::frame_update() {}