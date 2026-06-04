#include "renderer.h"

#include <unordered_set>

extern "C" char device_code_ptx[];

void Renderer::init(TetForest &forest, bool split_bvh) {
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
        {"camera.org", OWL_FLOAT3, OWL_OFFSETOF(LaunchParams, camera.org)},
        {"camera.llc", OWL_FLOAT3, OWL_OFFSETOF(LaunchParams, camera.llc)},
        {"camera.horiz", OWL_FLOAT3, OWL_OFFSETOF(LaunchParams, camera.horiz)},
        {"camera.vert", OWL_FLOAT3, OWL_OFFSETOF(LaunchParams, camera.vert)},
        {"stepSize", OWL_FLOAT, OWL_OFFSETOF(LaunchParams, stepSize)},
        {"dataRenderMode", OWL_INT, OWL_OFFSETOF(LaunchParams, dataRenderMode)},
        {"dataFieldNo", OWL_INT, OWL_OFFSETOF(LaunchParams, dataFieldNo)},
        {"bgColor", OWL_FLOAT3, OWL_OFFSETOF(LaunchParams, bgColor)},
        {"tetData.index", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.index)},
        {"tetData.vertex", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.vertex)},
        {"tetData.cell_data", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.cell_data)},
        {"tetData.vertex_data", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.vertex_data)},
        {"tetData.neighbors", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.neighbors)},
        {"tetData.num_tree_roots", OWL_ULONG, OWL_OFFSETOF(LaunchParams, tetData.num_tree_roots)},
        {"tetData.tree_root_ids", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.tree_root_ids)},
        {"tetData.tree_element_counts", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.tree_element_counts)},
        {"tetData.tree_ids", OWL_BUFPTR, OWL_OFFSETOF(LaunchParams, tetData.tree_ids)},
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
        {"base_primitive", OWL_UINT, OWL_OFFSETOF(TetData, base_primitive)},
        {nullptr}
    };

    tetGeomType = owlGeomTypeCreate(context, OWL_TRIANGLES, sizeof(TetData), tetVars, -1);
    owlGeomTypeSetClosestHit(tetGeomType, 0, module, "Tet");

    std::cout << "Number of elements " << forest.num_elements << std::endl;
    auto to_vec3f = [](const Vec3 &v) {
        return vec3f{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
    };

    // push all vertices
    std::vector<vec3f> vertices;
    vertices.reserve(forest.vertices.size());
    for (const auto &v : forest.vertices) {
        vertices.push_back(to_vec3f(v));
    }

    const int field_no = 0;
    std::vector<TetForest::Float> vtx_data(forest.vertex_data[field_no]);
    // normal chunked vs split through the mid x and y
    std::vector<OWLGroup> blas;
    if (!split_bvh) {
        std::vector<TetForest::Float> cell_data;
        std::vector<vec3i> indices;
        TetForest::TetID_t num_leaves = 0;
        std::vector<TetForest::TetID_t> tree_leaf_counts(forest.num_trees);
        std::vector<TetForest::TetID_t> tree_ids;
        for (TetForest::TetID_t i = 0; i < forest.num_elements; i++) {
            if (forest.child_counts[i] == 0) { // if iit is a leaf, add it
                const auto idx = forest.get_indices(i);
                indices.emplace_back(idx[0], idx[1], idx[2]);
                indices.emplace_back(idx[0], idx[2], idx[3]);
                indices.emplace_back(idx[0], idx[3], idx[1]);
                indices.emplace_back(idx[1], idx[3], idx[2]);
                cell_data.push_back(forest.cell_data[0][i]);
                tree_ids.push_back(forest.tree_ids[i]);
                ++num_leaves;
                ++tree_leaf_counts[forest.tree_ids[i]];
            }
        }
        std::vector<TetForest::TetID_t> tree_root_ids(forest.num_trees);

        TetForest::TetID_t count = 0;
        for (TetForest::TetID_t i = 0; i < forest.num_trees - 1; ++i) {
            tree_root_ids[i] = count;
            count += tree_leaf_counts[i];
        }

        std::cout << "Number of leaf elements: " << num_leaves << std::endl;
        numElements = num_leaves;

        vertexBuffer = owlDeviceBufferCreate(context, OWL_FLOAT3, vertices.size(), vertices.data());
        indexBuffer = owlDeviceBufferCreate(context, OWL_INT3, indices.size(), indices.data());

        cellDataBuffer = owlDeviceBufferCreate(context, OWL_FLOAT, cell_data.size(), cell_data.data());
        vertexDataBuffer = owlDeviceBufferCreate(context, OWL_FLOAT, vtx_data.size(), vtx_data.data());

        treeRootIDsBuffer = owlDeviceBufferCreate(context, OWL_ULONG, tree_root_ids.size(), tree_root_ids.data());
        treeElementCountsBuffer =
            owlDeviceBufferCreate(context, OWL_ULONG, tree_leaf_counts.size(), tree_leaf_counts.data());
        treeIdsBuffer = owlDeviceBufferCreate(context, OWL_ULONG, tree_ids.size(), tree_ids.data());

        std::vector<unsigned long long> neighbors_flat;
        neighbors_flat.reserve(forest.num_elements * 4);
        for (unsigned long long i = 0; i < forest.num_elements; ++i) {
            neighbors_flat.insert(neighbors_flat.end(), forest.neighbors[i].begin(), forest.neighbors[i].end());
        }

        OWLBuffer neighborsBuffer =
            owlDeviceBufferCreate(context, OWL_ULONG, forest.num_elements * 4, neighbors_flat.data());

        owlParamsSetBuffer(launchParams, "tetData.vertex", vertexBuffer);
        owlParamsSetBuffer(launchParams, "tetData.index", indexBuffer);
        owlParamsSetBuffer(launchParams, "tetData.cell_data", cellDataBuffer);
        owlParamsSetBuffer(launchParams, "tetData.vertex_data", vertexDataBuffer);
        owlParamsSetBuffer(launchParams, "tetData.tree_root_ids", treeRootIDsBuffer);
        owlParamsSetBuffer(launchParams, "tetData.tree_element_counts", treeElementCountsBuffer);
        owlParamsSetBuffer(launchParams, "tetData.tree_ids", treeIdsBuffer);
        owlParamsSetBuffer(launchParams, "tetData.neighbors", neighborsBuffer);

        const int num_chunks = 1;
        const int num_tris = num_leaves * 4;
        const int tris_per_chunk = num_tris / num_chunks;
        for (int i = 0; i < num_chunks; ++i) {
            int begin = i * tris_per_chunk;
            int count = tris_per_chunk;
            if (i == num_chunks - 1) {
                count = num_tris - begin;
            }

            OWLGeom tetGeom = owlGeomCreate(context, tetGeomType);
            owlTrianglesSetVertices(tetGeom, vertexBuffer, vertices.size(), sizeof(vec3f), 0);
            owlTrianglesSetIndices(tetGeom, indexBuffer, count, sizeof(vec3i), begin * sizeof(vec3i));
            owlGeomSet1ui(tetGeom, "base_primitive", static_cast<unsigned int>(begin));

            OWLGroup tetGroup = owlTrianglesGeomGroupCreate(context, 1, &tetGeom);
            owlGroupBuildAccel(tetGroup);

            blas.push_back(tetGroup);
        }
    } else {
        box3d bbox;
        for (const auto &root_id : forest.get_root_ids()) {
            for (const auto &v : forest.get_vertices(root_id)) {
                bbox.extend({v[0], v[1], v[2]});
            }
        }

        double center_x = bbox.center().x;
        double center_y = bbox.center().x;
        std::vector<std::unordered_set<TetForest::TetID_t>> coarse_ids(4);
        for (const auto &root_id : forest.get_root_ids()) {
            auto vtx = forest.get_vertices(root_id);
            auto centroid = (vtx[0] + vtx[1] + vtx[2] + vtx[3]) / 4;
            if (centroid.x < center_x) {
                if (centroid.y < center_y) {
                    coarse_ids[0].insert(forest.tree_ids[root_id]);
                } else {
                    coarse_ids[1].insert(forest.tree_ids[root_id]);
                }
            } else {
                if (centroid.y < center_y) {
                    coarse_ids[2].insert(forest.tree_ids[root_id]);
                } else {
                    coarse_ids[3].insert(forest.tree_ids[root_id]);
                }
            }
        }

        std::vector<std::vector<vec3i>> indices(coarse_ids.size());
        TetForest::TetID_t num_leaves = 0;
        // std::vector<TetForest::TetID_t> tree_leaf_counts(forest.num_trees);
        std::vector<std::vector<TetForest::Float>> cell_data(4);
        std::vector<std::vector<TetForest::TetID_t>> tree_ids(4);
        std::vector<TetForest::TetID_t> tree_element_counts(forest.num_trees);
        for (TetForest::TetID_t i = 0; i < forest.num_elements; i++) {
            if (forest.child_counts[i] == 0) { // if it is a leaf, add it
                int bucket = 0;
                for (int j = 1; j < coarse_ids.size(); ++j) {
                    if (coarse_ids[j].contains(forest.tree_ids[i])) {
                        bucket = j;
                        break;
                    }
                }
                const auto idx = forest.get_indices(i);
                indices[bucket].emplace_back(idx[0], idx[1], idx[2]);
                indices[bucket].emplace_back(idx[0], idx[2], idx[3]);
                indices[bucket].emplace_back(idx[0], idx[3], idx[1]);
                indices[bucket].emplace_back(idx[1], idx[3], idx[2]);
                ++tree_element_counts[forest.tree_ids[i]];

                tree_ids[bucket].push_back(forest.tree_ids[i]);
                cell_data[bucket].push_back(forest.cell_data[0][i]);
                ++num_leaves;
            }
        }
        std::vector<vec3i> indices_flat;
        indices_flat.reserve(4 * num_leaves);
        for (auto &ind : indices) {
            indices_flat.insert(indices_flat.end(), ind.begin(), ind.end());
        }

        std::vector<TetForest::Float> cell_data_flat;
        cell_data_flat.reserve(4 * num_leaves);
        for (auto &cd : cell_data) {
            cell_data_flat.insert(cell_data_flat.end(), cd.begin(), cd.end());
        }

        std::vector<TetForest::TetID_t> tree_ids_flat;
        tree_ids_flat.reserve(4 * num_leaves);
        for (auto &t_id : tree_ids) {
            tree_ids_flat.insert(tree_ids_flat.end(), t_id.begin(), t_id.end());
        }

        std::vector<TetForest::TetID_t> tree_root_ids(forest.num_trees, std::numeric_limits<TetForest::TetID_t>::max());
        for (unsigned long long i = 0; i < tree_ids_flat.size(); ++i) {
            tree_root_ids[tree_ids_flat[i]] = std::min(tree_root_ids[tree_ids_flat[i]], i);
        }

        std::cout << "Num leaf elements: " << num_leaves << std::endl;

        vertexBuffer = owlDeviceBufferCreate(context, OWL_FLOAT3, vertices.size(), vertices.data());
        indexBuffer = owlDeviceBufferCreate(context, OWL_INT3, indices_flat.size(), indices_flat.data());

        cellDataBuffer = owlDeviceBufferCreate(context, OWL_FLOAT, cell_data_flat.size(), cell_data_flat.data());
        vertexDataBuffer = owlDeviceBufferCreate(context, OWL_FLOAT, vtx_data.size(), vtx_data.data());

        treeRootIDsBuffer = owlDeviceBufferCreate(context, OWL_ULONG, tree_root_ids.size(), tree_root_ids.data());
        treeElementCountsBuffer =
            owlDeviceBufferCreate(context, OWL_ULONG, tree_element_counts.size(), tree_element_counts.data());
        treeIdsBuffer = owlDeviceBufferCreate(context, OWL_ULONG, tree_ids_flat.size(), tree_ids_flat.data());

        std::vector<unsigned long long> neighbors_flat;
        neighbors_flat.reserve(forest.num_elements * 4);
        for (unsigned long long i = 0; i < forest.num_elements; ++i) {
            neighbors_flat.insert(neighbors_flat.end(), forest.neighbors[i].begin(), forest.neighbors[i].end());
        }

        OWLBuffer neighborsBuffer =
            owlDeviceBufferCreate(context, OWL_ULONG, forest.num_elements * 4, neighbors_flat.data());

        owlParamsSetBuffer(launchParams, "tetData.vertex", vertexBuffer);
        owlParamsSetBuffer(launchParams, "tetData.index", indexBuffer);
        owlParamsSetBuffer(launchParams, "tetData.cell_data", cellDataBuffer);
        owlParamsSetBuffer(launchParams, "tetData.vertex_data", vertexDataBuffer);
        owlParamsSetBuffer(launchParams, "tetData.tree_root_ids", treeRootIDsBuffer);
        owlParamsSetBuffer(launchParams, "tetData.tree_element_counts", treeElementCountsBuffer);
        owlParamsSetBuffer(launchParams, "tetData.tree_ids", treeIdsBuffer);
        owlParamsSetBuffer(launchParams, "tetData.neighbors", neighborsBuffer);

        size_t begin = 0;
        for (int i = 0; i < indices.size(); ++i) {
            size_t count = indices[i].size();

            OWLGeom tetGeom = owlGeomCreate(context, tetGeomType);
            owlTrianglesSetVertices(tetGeom, vertexBuffer, vertices.size(), sizeof(vec3f), 0);
            owlTrianglesSetIndices(tetGeom, indexBuffer, count, sizeof(vec3i), begin * sizeof(vec3i));
            owlGeomSet1ui(tetGeom, "base_primitive", static_cast<unsigned int>(begin));

            OWLGroup tetGroup = owlTrianglesGeomGroupCreate(context, 1, &tetGeom);
            owlGroupBuildAccel(tetGroup);
            begin += count;

            blas.push_back(tetGroup);
        }
    }
    world = owlInstanceGroupCreate(context, blas.size(), blas.data());
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
}

void Renderer::setCamera(const BasicCamera &camera_) {
    camera = camera_;
    owlParamsSet3f(launchParams, "camera.org", camera.org.x, camera.org.y, camera.org.z);
    owlParamsSet3f(launchParams, "camera.llc", camera.llc.x, camera.llc.y, camera.llc.z);
    owlParamsSet3f(launchParams, "camera.horiz", camera.horiz.x, camera.horiz.y, camera.horiz.z);
    owlParamsSet3f(launchParams, "camera.vert", camera.vert.x, camera.vert.y, camera.vert.z);
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

void Renderer::setCellData(const std::vector<float> &cell_data) const {
    owlBufferUpload(cellDataBuffer, cell_data.data());
}
void Renderer::setVertexData(const std::vector<float> &vertex_data) const {
    owlBufferUpload(vertexDataBuffer, vertex_data.data());
}