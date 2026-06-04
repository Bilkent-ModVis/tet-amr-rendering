#include "renderer.h"

#include <queue>
#include <ranges>
#include <unordered_map>

extern "C" char device_code_ptx[];
float signed_volume(const vec3f &v0, const vec3f &v1, const vec3f &v2, const vec3f &v3) {
    float volume = dot(v0 - v3, cross(v2 - v1, v3 - v1));
    return volume;
}

vec3f to_vec3f(const Vec3 &v) {
    return vec3f{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
};

void add_tree_to_coarse_mesh(const TetForest::TetID_t root_id, int coarse_mesh_level, const TetForest &forest,
                             std::vector<vec3f> &coarse_vertices, std::vector<vec3i> &coarse_indices,
                             std::unordered_map<TetForest::TetID_t, TetForest::TetID_t> &index_table,
                             std::vector<TetForest::TetID_t> &tree_root_ids, TetForest::TetID_t &num_coarse_tets) {
    std::priority_queue<TetForest::TetID_t> dfs;
    dfs.push(root_id);
    while (!dfs.empty()) {
        TetForest::TetID_t id = dfs.top();
        dfs.pop();
        if (forest.refinement_levels[id] == coarse_mesh_level || forest.child_counts[id] == 0) {
            const auto idx = forest.get_indices(id);

            for (int vi = 0; vi < 4; ++vi) {
                if (!index_table.contains(idx[vi])) {
                    index_table[idx[vi]] = coarse_vertices.size();
                    coarse_vertices.emplace_back(to_vec3f(forest.vertices[idx[vi]]));
                }
            }
            TetForest::TetID_t i0 = index_table[idx[0]];
            TetForest::TetID_t i1 = index_table[idx[1]];
            TetForest::TetID_t i2 = index_table[idx[2]];
            TetForest::TetID_t i3 = index_table[idx[3]];
            coarse_indices.emplace_back(i0, i1, i2);
            coarse_indices.emplace_back(i0, i2, i3);
            coarse_indices.emplace_back(i0, i3, i1);
            coarse_indices.emplace_back(i1, i3, i2);

            tree_root_ids.push_back(id);

            ++num_coarse_tets;
        } else {
            for (const auto c_id : forest.get_child_ids(id) | std::views::reverse) {
                dfs.push(c_id);
            }
        }
    }
}

std::vector<TetForest::Float> get_vertex_data(const TetForest &forest, const int field_no) {
    std::vector<TetForest::Float> vtx_data(4 * forest.num_elements);
    for (TetForest::TetID_t i = 0; i < forest.num_elements; ++i) {
        const auto idx = forest.get_indices(i);
        vtx_data[4 * i + 0] = forest.vertex_data[field_no][idx[0]];
        vtx_data[4 * i + 1] = forest.vertex_data[field_no][idx[1]];
        vtx_data[4 * i + 2] = forest.vertex_data[field_no][idx[2]];
        vtx_data[4 * i + 3] = forest.vertex_data[field_no][idx[3]];
    }

    return vtx_data;
}

void traverse_tree(const TetForest::TetID_t root_id, const int root_no, const int max_depth, const TetForest &forest,
                   std::vector<float2> &cell_scalar_range, std::vector<float2> &vertex_scalar_range) {
    std::priority_queue<TetForest::TetID_t> dfs;
    dfs.push(root_id);
    while (!dfs.empty()) {
        TetForest::TetID_t id = dfs.top();
        dfs.pop();
        cell_scalar_range[root_no].x = std::min(cell_scalar_range[root_no].x, forest.cell_data[0][id]);
        cell_scalar_range[root_no].y = std::max(cell_scalar_range[root_no].y, forest.cell_data[0][id]);
        for (const TetForest::TetID_t vtx_id : forest.get_indices(id)) {
            vertex_scalar_range[root_no].x = std::min(vertex_scalar_range[root_no].x, forest.vertex_data[0][vtx_id]);
            vertex_scalar_range[root_no].y = std::max(vertex_scalar_range[root_no].y, forest.vertex_data[0][vtx_id]);
        }
        if (!(forest.refinement_levels[id] == max_depth || forest.child_counts[id] == 0)) {
            for (const auto c_id : forest.get_child_ids(id) | std::views::reverse) {
                dfs.push(c_id);
            }
        }
    }
}

void Renderer::init(TetForest &forest, const int coarse_mesh_level) {
    this->forest = &forest;

    context = owlContextCreate(nullptr, 1);
    module = owlModuleCreate(context, device_code_ptx);
    rayGen = owlRayGenCreate(context, module, "rayGen", 0, nullptr, -1);

    OWLVarDecl launchParamsVars[] = {
        {"fbPtr", OWL_RAW_POINTER, OWL_OFFSETOF(LaunchParams, fbPtr)},
        {"accumulationBuffer", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, accumulationBuffer)},
        {"fbSize", OWL_INT2, OWL_OFFSETOF(LaunchParams, fbSize)},
        {"accumulateCount", OWL_UINT, OWL_OFFSETOF(LaunchParams, accumulateCount)},
        {"world", OWL_GROUP, OWL_OFFSETOF(LaunchParams, world)},
        {"tf.tfbPtr", OWL_BUFFER_POINTER, OWL_OFFSETOF(LaunchParams, tf.tfbPtr)},
        {"tf.tfTex", OWL_TEXTURE, OWL_OFFSETOF(LaunchParams, tf.tfTex)},
        {"tf.range", OWL_FLOAT2, OWL_OFFSETOF(LaunchParams, tf.range)},
        {"tf.opacity", OWL_FLOAT, OWL_OFFSETOF(LaunchParams, tf.opacity)},
        {"camera", OWL_USER_TYPE(BasicCamera), OWL_OFFSETOF(LaunchParams, camera)},
        {"stepSize", OWL_FLOAT, OWL_OFFSETOF(LaunchParams, stepSize)},
        {"refinementCriteria", OWL_FLOAT, OWL_OFFSETOF(LaunchParams, refinementCriteria)},
        {"dataRenderMode", OWL_INT, OWL_OFFSETOF(LaunchParams, dataRenderMode)},
        {"dataFieldNo", OWL_INT, OWL_OFFSETOF(LaunchParams, dataFieldNo)},
        {"bgColor", OWL_FLOAT3, OWL_OFFSETOF(LaunchParams, bgColor)},
        {"tetData.index", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.index)},
        {"tetData.vertex", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.vertex)},
        {"tetData.cell_data", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.cell_data)},
        {"tetData.vertex_data", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.vertex_data)},
        {"tetData.tree_root_ids", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.tree_root_ids)},
        {"tetData.neighbors", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.neighbors)},
        {"tetData.num_tree_roots", OWL_ULONG, OWL_OFFSETOF(LaunchParams, tetData.num_tree_roots)},
        {"tetData.child_nums", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.child_nums)},
        {"tetData.cell_density", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.cell_density)},
        {"tetData.vertex_density", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.vertex_density)},
        {"tetData.cell_scalar_range", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.cell_scalar_range)},
        {"tetData.vertex_scalar_range", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.vertex_scalar_range)},
        {"renderMode", OWL_UINT, OWL_OFFSETOF(LaunchParams, renderMode)},
        {nullptr}
    };

    launchParams = owlParamsCreate(context, sizeof(LaunchParams), launchParamsVars, -1);

    accumulationBuffer = owlDeviceBufferCreate(context, OWL_FLOAT4, frame.size.x * frame.size.y, nullptr);
    owlParamsSetBuffer(launchParams, "accumulationBuffer", accumulationBuffer);

    OWLVarDecl missProgVars[] = {{/* sentinel */}};
    OWLMissProg missProg = owlMissProgCreate(context, module, "miss", sizeof(MissProgData), missProgVars, -1);
    owlMissProgSet(context, 0, missProg);

    OWLVarDecl tetVars[] = {
        {"index", OWL_BUFPTR, OWL_OFFSETOF(TetData, index)},
        {"vertex", OWL_BUFPTR, OWL_OFFSETOF(TetData, vertex)},
        {"cell_data", OWL_BUFPTR, OWL_OFFSETOF(TetData, cell_data)},
        {"vertex_data", OWL_BUFPTR, OWL_OFFSETOF(TetData, vertex_data)},
        {"tree_root_ids", OWL_BUFPTR, OWL_OFFSETOF(TetData, tree_root_ids)},
        {"neighbors", OWL_BUFPTR, OWL_OFFSETOF(TetData, neighbors)},
        {"child_nums", OWL_BUFPTR, OWL_OFFSETOF(TetData, child_nums)},
        {nullptr}
    };

    tetGeomType = owlGeomTypeCreate(context, OWL_TRIANGLES, sizeof(TetData), tetVars, -1);
    owlGeomTypeSetClosestHit(tetGeomType, 0, module, "Tet");

    std::vector<vec3f> coarse_vertices;
    std::vector<vec3i> coarse_indices;
    TetForest::TetID_t num_coarse_tets = 0;
    std::cout << "Number of elements: " << forest.num_elements << std::endl;

    std::unordered_map<TetForest::TetID_t, TetForest::TetID_t> index_table;
    std::vector<TetForest::TetID_t> tree_root_ids;

    for (const TetForest::TetID_t root_id : forest.get_root_ids()) {
        add_tree_to_coarse_mesh(root_id, coarse_mesh_level, forest, coarse_vertices, coarse_indices, index_table,
                                tree_root_ids, num_coarse_tets);
    }

    std::cout << "Number of coarse tets: " << num_coarse_tets << std::endl;
    std::cout << "Number of trees: " << tree_root_ids.size() << std::endl;
    unsigned long long num_leaf = 0;
    for (auto c_count : forest.child_counts) {
        if (c_count == 0) {
            ++num_leaf;
        }
    }
    std::cout << "Number of leaf elements: " << num_leaf << std::endl;

    vertexBuffer = owlDeviceBufferCreate(context, OWL_FLOAT3, coarse_vertices.size(), coarse_vertices.data());
    indexBuffer = owlDeviceBufferCreate(context, OWL_INT3, coarse_indices.size(), coarse_indices.data());

    cellDataBuffer = owlDeviceBufferCreate(context, OWL_FLOAT, forest.cell_data[0].size(), forest.cell_data[0].data());
    std::vector<TetForest::Float> vtx_data = get_vertex_data(forest, 0);
    vertexDataBuffer = owlDeviceBufferCreate(context, OWL_FLOAT, vtx_data.size(), vtx_data.data());

    childCountsBuffer =
        owlDeviceBufferCreate(context, OWL_ULONG, forest.child_counts.size(), forest.child_counts.data());
    treeRootIDsBuffer = owlDeviceBufferCreate(context, OWL_ULONG, tree_root_ids.size(), tree_root_ids.data());
    numTrees = tree_root_ids.size();

    std::vector<unsigned long long> neighbors_flat;
    neighbors_flat.reserve(forest.num_elements * 4);
    for (unsigned long long i = 0; i < forest.num_elements; ++i) {
        neighbors_flat.insert(neighbors_flat.end(), forest.neighbors[i].begin(), forest.neighbors[i].end());
    }

    OWLBuffer neighborsBuffer =
        owlDeviceBufferCreate(context, OWL_ULONG, forest.num_elements * 4, neighbors_flat.data());

    std::vector<float2> cell_density_buffer(numTrees, {0.0f, 1.0f});
    std::vector<float2> vertex_density_buffer(numTrees, {0.0f, 1.0f});
    cellDensityBuffer = owlDeviceBufferCreate(context, OWL_FLOAT2, numTrees, cell_density_buffer.data());
    vertexDensityBuffer = owlDeviceBufferCreate(context, OWL_FLOAT2, numTrees, vertex_density_buffer.data());

    std::vector<float2> cell_scalar_range(numTrees,
                                          {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()});
    std::vector<float2> vertex_scalar_range(numTrees,
                                            {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()});
    // calculate scalar range for each coarse
    int root_no = 0;
    for (const auto root_id : tree_root_ids) {
        std::vector<TetForest::TetID_t> coarse_mesh_ids;
        traverse_tree(root_id, root_no++, -1, forest, cell_scalar_range, vertex_scalar_range);
    }

    vertexScalarRangeBuffer = owlDeviceBufferCreate(context, OWL_FLOAT2, numTrees, vertex_scalar_range.data());
    cellScalarRangeBuffer = owlDeviceBufferCreate(context, OWL_FLOAT2, numTrees, cell_scalar_range.data());

    tetGeom = owlGeomCreate(context, tetGeomType);
    owlTrianglesSetVertices(tetGeom, vertexBuffer, coarse_vertices.size(), sizeof(vec3f), 0);
    owlTrianglesSetIndices(tetGeom, indexBuffer, coarse_indices.size(), sizeof(vec3i), 0);
    owlGeomSetBuffer(tetGeom, "vertex", vertexBuffer);
    owlGeomSetBuffer(tetGeom, "index", indexBuffer);
    owlGeomSetBuffer(tetGeom, "cell_data", cellDataBuffer);
    owlGeomSetBuffer(tetGeom, "vertex_data", vertexDataBuffer);
    owlGeomSetBuffer(tetGeom, "child_nums", childCountsBuffer);
    owlGeomSetBuffer(tetGeom, "tree_root_ids", treeRootIDsBuffer);
    owlGeomSetBuffer(tetGeom, "neighbors", neighborsBuffer);

    owlParamsSetBuffer(launchParams, "tetData.vertex", vertexBuffer);
    owlParamsSetBuffer(launchParams, "tetData.index", indexBuffer);
    owlParamsSetBuffer(launchParams, "tetData.cell_data", cellDataBuffer);
    owlParamsSetBuffer(launchParams, "tetData.vertex_data", vertexDataBuffer);
    owlParamsSetBuffer(launchParams, "tetData.child_nums", childCountsBuffer);
    owlParamsSetBuffer(launchParams, "tetData.tree_root_ids", treeRootIDsBuffer);
    owlParamsSetBuffer(launchParams, "tetData.neighbors", neighborsBuffer);
    owlParamsSet1ul(launchParams, "tetData.num_tree_roots", num_coarse_tets);
    owlParamsSetBuffer(launchParams, "tetData.cell_density", cellDensityBuffer);
    owlParamsSetBuffer(launchParams, "tetData.vertex_density", vertexDensityBuffer);
    owlParamsSetBuffer(launchParams, "tetData.cell_scalar_range", cellScalarRangeBuffer);
    owlParamsSetBuffer(launchParams, "tetData.vertex_scalar_range", vertexScalarRangeBuffer);

    OWLGroup tetGroup = owlTrianglesGeomGroupCreate(context, 1, &tetGeom);
    owlGroupBuildAccel(tetGroup);
    world = owlInstanceGroupCreate(context, 1, &tetGroup);
    owlGroupBuildAccel(world);

    // real size is determined later when tf update func is called
    tfBuffer = owlDeviceBufferCreate(context, OWL_FLOAT4, 1, nullptr);
    owlParamsSetBuffer(launchParams, "tf.tfbPtr", tfBuffer);

    owlParamsSetGroup(launchParams, "world", world);
    owlBuildPrograms(context);
    owlBuildPipeline(context);
    owlBuildSBT(context);
    sbtDirty = true;
}

void Renderer::render(uint32_t *fbPtr) {
    if (sbtDirty) {
        owlBuildSBT(context);
        sbtDirty = false;
    }
    owlParamsSetPointer(launchParams, "fbPtr", fbPtr);
    owlLaunch2D(rayGen, frame.size.x, frame.size.y, launchParams);
}

void Renderer::resize(const owl::vec2i &newSize) {
    frame.size = newSize;
    owlParamsSet2i(launchParams, "fbSize", newSize.x, newSize.y);
    owlBufferResize(accumulationBuffer, newSize.x * newSize.y);
    owlParamsSetBuffer(launchParams, "accumulationBuffer", accumulationBuffer);

    // update camera projection constants to use the new fb size
    setCamera(camera);
}

void Renderer::setCamera(const BasicCamera &camera_) {
    camera = camera_;
    camera.n = cross(camera.horiz, camera.vert);
    camera.dot_llc_n = dot(camera.llc, camera.n);
    camera.horiz_inv_horiz2_fbsize =
        camera.horiz * (1 / dot(camera.horiz, camera.horiz)) * static_cast<float>(frame.size.x);
    camera.vert_inv_vert2_fbsize = camera.vert * (1 / dot(camera.vert, camera.vert)) * static_cast<float>(frame.size.y);

    owlParamsSetRaw(launchParams, "camera", &camera);
}

void Renderer::setTransferFunction(const std::vector<vec4f> &cmap) {
    if (cmap.size() != cmapSize) {
        owlBufferResize(tfBuffer, cmap.size());
        cmapSize = cmap.size();
    }
    owlBufferUpload(tfBuffer, cmap.data());

    if (cmapTexture) {
        owlTexture2DDestroy(cmapTexture);
    }
    cmapTexture = owlTexture2DCreate(context, OWL_TEXEL_FORMAT_RGBA32F, cmap.size(), 1, cmap.data());
    owlParamsSetTexture(launchParams, "tf.tfTex", cmapTexture);
}

void Renderer::setScalarRange(const interval<float> &range) const {
    owlParamsSet2f(launchParams, "tf.range", range.begin, range.end);
}

void Renderer::setOpacityScale(float scale) const { owlParamsSet1f(launchParams, "tf.opacity", scale / 100); }

void Renderer::setStepSize(float stepSize) const { owlParamsSet1f(launchParams, "stepSize", stepSize); }

void Renderer::setRefinementCriteria(float refinementCriteria) const {
    owlParamsSet1f(launchParams, "refinementCriteria", refinementCriteria);
}

// can be either 0 for cell, or 1 for vertex
void Renderer::setDataRenderMode(int mode) const { owlParamsSet1i(launchParams, "dataRenderMode", mode); }

void Renderer::setDataFieldNo(int fieldNo) const { owlParamsSet1i(launchParams, "dataFieldNo", fieldNo); }

void Renderer::setBackgroundColor(vec3f color) const {
    owlParamsSet3f(launchParams, "bgColor", color.x, color.y, color.z);
}

void Renderer::setAccumulation(unsigned int accumulationCount) const {
    owlParamsSet1ui(launchParams, "accumulateCount", accumulationCount);
}

void Renderer::setRenderMode(RenderMode renderMode) const { owlParamsSet1ui(launchParams, "renderMode", renderMode); }

void Renderer::setCellDensityRange(std::vector<float2> &cell_range) const {
    owlBufferUpload(cellDensityBuffer, cell_range.data());
}
void Renderer::setVertexDensityRange(std::vector<float2> &vertex_range) const {
    owlBufferUpload(vertexDensityBuffer, vertex_range.data());
}

void Renderer::setCellData(const std::vector<float> &cell_data) const {
    owlBufferUpload(cellDataBuffer, cell_data.data());
}
void Renderer::setVertexData(const std::vector<float> &vertex_data) const {
    owlBufferUpload(vertexDataBuffer, vertex_data.data());
}
