#pragma once

#include "TetForest.h"
#include "viewer.h"

std::array<vec3f, 3> get_centered_camera_and_speed(const TetForest &forest, float fovy, float *speed = nullptr);
viewer::Camera create_camera(const Viewer_options &vo, const TetForest &forest, vec3f &interest);
void set_offline_renderer_camera(Renderer &renderer, const Viewer_options &vo, const TetForest &forest,
                                 const vec2i &size);
int get_field_no(const TetForest &forest, const std::string &field);
void scale_mesh(TetForest &forest, float scale);

void set_renderer_options(const Renderer_options &ro, const TetForest &forest, const Renderer *renderer,
                          hs::TFEditor *tfEditor);
void set_viewer_options(const Viewer_options &vo, const Renderer_options &ro, const TetForest &forest, Viewer *viewer);

void render(Renderer &renderer, const Renderer_options &ro, uint32_t *fbPointer, int target_frame_count);
void save_image(const Renderer_options &ro, const uint32_t *fbPointer, const vec2i &size);
