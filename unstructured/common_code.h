#pragma once

#include "owl/common/math/box.h"
#include "render_mode_enums.h"

#include <owl/common/math/vec.h>
#include <owl/owl.h>
#include <texture_types.h>

using namespace owl;

struct TetData {
    vec3i *index;
    vec3f *vertex;
    float *cell_data;
    float *vertex_data;
    unsigned long long *tree_root_ids;
    unsigned long long *tree_element_counts;
    unsigned long long *neighbors;
    unsigned long long num_tree_roots;
    unsigned long long *child_nums;
    unsigned long long *tree_ids;
    unsigned int base_primitive;
};

struct BasicCamera {
    vec3f org;
    vec3f llc;
    vec3f horiz;
    vec3f vert;
};

struct TransferFunction {
    uint32_t *tfbPtr;
    cudaTextureObject_t tfTex;
    vec2f range;
    float opacity = 1.0f;
};

struct LaunchParams {
    uint32_t *fbPtr;
    vec4f *accumulationBuffer;
    vec2i fbSize;
    uint32_t accumulateCount;
    OptixTraversableHandle world;
    TransferFunction tf;
    BasicCamera camera;
    float stepSize;
    float epsilon;
    float refinementCriteria;
    int dataRenderMode;
    int dataFieldNo;
    vec3f bgColor;
    TetData tetData;
    RenderMode renderMode;
};

struct MissProgData {};

struct PerRayData {
    bool missed;
    vec3f hit_point;
    unsigned long long tet_id;
    float scalar;
    float hit_t;
    interval<float> element_t_range;
};
