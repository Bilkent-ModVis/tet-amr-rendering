#pragma once

#include "common_code.h"
#include "parse_options.h"
#include "render_mode_enums.h"

#include <TFEditor.h>
#include <TetForest.h>
#include <owl/common.h>
#include <owl/owl.h>

struct Renderer {
    void init(TetForest &forest, bool split_bvh);
    void render(uint32_t *fbPtr);
    void resize(const owl::vec2i &newSize);
    void setCamera(const BasicCamera &camera_);
    void setTransferFunction(const std::vector<vec4f> &cmap);
    void setScalarRange(const interval<float> &range) const;
    void setOpacityScale(float scale) const;
    void setStepSize(float stepSize) const;
    void setEpsilon(float epsilon) const;
    void setRefinementCriteria(float refinementCriteria) const;
    void setDataRenderMode(int mode) const;
    void setDataFieldNo(int fieldNo) const;
    void setBackgroundColor(vec3f color) const;
    void setAccumulation(unsigned int accumulationCount) const;
    void setRenderMode(RenderMode renderMode) const;
    void setCellData(const std::vector<float> &cell_data) const;
    void setVertexData(const std::vector<float> &vertex_data) const;

    bool sbtDirty{true};

    OWLContext context{};
    OWLModule module{};
    OWLRayGen rayGen{};
    OWLLaunchParams launchParams{};
    OWLGroup world{};
    OWLGeom tetGeom{};
    OWLGeomType tetGeomType{};
    size_t cmapSize{0};
    OWLBuffer tfBuffer{};
    OWLTexture cmapTexture{};

    OWLBuffer cellDataBuffer{};
    OWLBuffer vertexDataBuffer{};
    OWLBuffer treeRootIDsBuffer{};
    OWLBuffer treeElementCountsBuffer{};
    OWLBuffer treeIdsBuffer{};
    OWLBuffer vertexBuffer{};
    OWLBuffer indexBuffer{};

    OWLBuffer accumulationBuffer{};

    BasicCamera camera;
    unsigned long long numElements{0};

    struct {
        owl::vec2i size;
    } frame;
};
