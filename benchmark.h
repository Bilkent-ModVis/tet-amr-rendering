#pragma once

#include "renderer.h"

void render_time_benchmark(Renderer &renderer, const Renderer_options &ro, uint32_t *fbPointer);
void refinement_criteria_benchmark(Renderer &renderer, const Renderer_options &ro, uint32_t *fbPointer);
