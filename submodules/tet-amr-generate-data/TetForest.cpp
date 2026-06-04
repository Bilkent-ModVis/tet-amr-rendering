#include "TetForest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <ranges>
#include <set>
#include <stack>
#include <unordered_map>

using TetID_t = TetForest::TetID_t;

std::vector<TetID_t> TetForest::get_indices(const TetID_t tet_id) const {
    using diff_type = std::vector<TetID_t>::difference_type;
    return {indices.begin() + static_cast<diff_type>(4 * tet_id),
            indices.begin() + static_cast<diff_type>(4 * (tet_id + 1))};
}

std::vector<Vec3> TetForest::get_vertices(const TetID_t tet_id) const {
    const auto idx = get_indices(tet_id);
    return {vertices[idx[0]], vertices[idx[1]], vertices[idx[2]],
            vertices[idx[3]]};
}

std::vector<std::vector<TetID_t>> TetForest::get_child_ids() const {
    std::vector<std::vector<TetID_t>> child_ids(num_elements);
    for (TetID_t i = 0; i < num_elements; ++i) {
        if (i != parent_ids[i]) {
            child_ids[parent_ids[i]].push_back(i);
        }
    }
    return child_ids;
}

std::vector<TetID_t> TetForest::get_child_ids(const TetID_t tet_id) const {
    std::vector<TetID_t> child_ids;
    if (child_counts[tet_id] == 0) {
        return child_ids;
    }

    TetID_t curr_id = tet_id + 1;
    for (TetID_t i = 0; i < 8; ++i) {
        child_ids.push_back(curr_id);
        curr_id += child_counts[curr_id] + 1;
    }

    return child_ids;
}

std::vector<TetID_t> TetForest::get_root_ids() const {
    std::vector<TetID_t> root_ids(num_trees);
    TetID_t curr_id = 0;
    for (TetID_t tree_id = 0; tree_id < num_trees; ++tree_id) {
        root_ids[tree_id] = curr_id;
        curr_id += child_counts[curr_id] + 1;
    }
    return root_ids;
}

void TetForest::add_element_to_forest_with_vertices(
    const std::array<Vec3, 4> &vtx, const std::vector<Float> &c_data,
    const unsigned int refinement_level, const TetID_t tree_id) {
    const TetID_t id = num_elements;

    const TetID_t index = vertices.size();
    vertices.insert(vertices.end(), vtx.begin(), vtx.end());
    const auto idx = {index, index + 1, index + 2, index + 3};
    indices.insert(indices.end(), idx);

    for (decltype(num_fields) i = 0; i < num_fields; ++i) {
        cell_data[i].push_back(c_data[i]);
    }

    refinement_levels.push_back(refinement_level);
    tree_ids.push_back(tree_id);
    parent_ids.push_back(id);

    ++num_elements;
}

void TetForest::add_element_to_forest_with_indices(
    const std::array<TetID_t, 4> &idx, const std::vector<Float> &c_data,
    const unsigned int refinement_level, const TetID_t tree_id) {
    const TetID_t id = num_elements;

    indices.insert(indices.end(), idx.begin(), idx.end());

    for (decltype(num_fields) i = 0; i < num_fields; ++i) {
        cell_data[i].push_back(c_data[i]);
    }

    refinement_levels.push_back(refinement_level);
    tree_ids.push_back(tree_id);
    parent_ids.push_back(id);

    ++num_elements;
}

void TetForest::deduplicate_vertices() {
    std::map<Vec3, TetID_t> vtx_to_idx;
    std::vector<Vec3> vtx;
    std::vector<TetID_t> idx;
    TetID_t index = 0;

    for (Vec3 v : vertices) {
        if (!vtx_to_idx.contains(v)) {
            vtx.push_back(v);
            vtx_to_idx[v] = index;
            ++index;
        }
        idx.push_back(vtx_to_idx[v]);
    }

    vertices = vtx;
    indices = idx;
}

// returns the indices of the vertices at the outer corners
std::array<TetID_t, 4>
get_corner_indices(const TetForest &forest,
                   const std::vector<TetID_t> &element_ids) {
    std::array<TetID_t, 4> corner_vertices{};
    std::unordered_map<TetID_t, int> idx_counts;

    for (int i = 0; i < 8; ++i) {
        for (const TetID_t idx : forest.get_indices(element_ids[i])) {
            idx_counts[idx]++;
        }
    }

    int vtx_count = 0;
    for (const auto [idx, count] : idx_counts) {
        if (count == 1)
            corner_vertices[vtx_count++] = idx;
    }

    return corner_vertices;
}

// NOLINTNEXTLINE(misc-no-recursion)
void merge_elements(TetForest &forest, const TetID_t tree_id,
                    const unsigned int level,
                    std::vector<std::vector<TetID_t>> &acc) {
    using TetID_t = TetID_t;
    const TetID_t new_element_id = forest.num_elements;

    const std::vector<TetID_t> &element_ids = acc[level];

    const std::array<TetID_t, 4> corner_indices =
        get_corner_indices(forest, element_ids);

    std::vector<TetForest::Float> cell_data(forest.num_fields);
    for (decltype(forest.num_fields) i = 0; i < forest.num_fields; ++i) {
        for (const TetID_t id : element_ids) {
            cell_data[i] += forest.cell_data[i][id];
        }
        cell_data[i] /= 8;
    }

    for (const TetID_t id : element_ids) {
        forest.parent_ids[id] = new_element_id;
    }

    forest.add_element_to_forest_with_indices(corner_indices, cell_data,
                                              level - 1, tree_id);

    acc[level] = {};
    if (level > 0) {
        acc[level - 1].push_back(new_element_id);
        if (acc[level - 1].size() == 8) {
            merge_elements(forest, tree_id, level - 1, acc);
        }
    }
}

void TetForest::create_parent_elements() {
    parent_ids.resize(num_elements, 0);
    for (TetID_t i = 0; i < num_elements; ++i) {
        parent_ids[i] = i;
    }
    std::vector<std::vector<TetID_t>> tree_element_ids(num_trees);
    std::vector<decltype(refinement_levels)::value_type> max_refinement_levels(
        num_trees);

    for (TetID_t i = 0; i < num_elements; ++i) {
        tree_element_ids[tree_ids[i]].push_back(i);
        max_refinement_levels[tree_ids[i]] =
            std::max(max_refinement_levels[tree_ids[i]], refinement_levels[i]);
    }

    for (TetID_t i = 0; i < num_trees; ++i) {
        std::vector<std::vector<unsigned long long>> acc(
            max_refinement_levels[i] + 1);
        for (TetID_t element_id : tree_element_ids[i]) {
            const unsigned int r_level = refinement_levels[element_id];
            acc[r_level].push_back(element_id);
            if (acc[r_level].size() == 8) {
                merge_elements(*this, i, r_level, acc);
            }
        }
    }
}

template <typename T>
void reorder_vector_multiple(const std::vector<TetID_t> &old_ids,
                             std::vector<T> &vec, const int n) {
    std::vector<T> old_vec(vec.begin(), vec.end());
    for (TetID_t i = 0; i < old_ids.size(); ++i) {
        for (int j = 0; j < n; ++j) {
            vec[n * i + j] = old_vec[n * old_ids[i] + j];
        }
    }
}

template <typename T>
void reorder_vector(const std::vector<TetID_t> &old_ids, std::vector<T> &vec) {
    std::vector<T> old_vec(vec.begin(), vec.end());
    for (TetID_t i = 0; i < old_ids.size(); ++i) {
        vec[i] = old_vec[old_ids[i]];
    }
}

void TetForest::calculate_children_counts() {
    child_counts.resize(num_elements, 0);
    std::vector<std::pair<decltype(refinement_levels)::value_type, TetID_t>>
        r_level_id;
    r_level_id.reserve(num_elements - num_trees); // we will not push tree roots

    for (TetID_t i = 0; i < num_elements; ++i) {
        const auto r_level = refinement_levels[i];
        if (r_level > 0) {
            r_level_id.emplace_back(r_level, i);
        }
    }
    std::ranges::sort(r_level_id, std::greater());

    for (const auto id : r_level_id | std::views::values) {
        const TetID_t parent_id = parent_ids[id];
        child_counts[parent_id] += 1 + child_counts[id];
    }
}

// positive if v1, v2, v3 is facing v0
double signed_volume(const std::vector<Vec3> &vertices) {
    const Vec3 v0 = vertices[0];
    const Vec3 v1 = vertices[1];
    const Vec3 v2 = vertices[2];
    const Vec3 v3 = vertices[3];

    return Vec3::dot((v0 - v2), Vec3::cross((v3 - v2), (v1 - v2)));
}

TetForest::TetID_t get_opposite_index(const TetForest &forest,
                                      const TetForest::TetID_t top_vtx_index,
                                      const std::vector<TetID_t> &child_ids) {
    std::vector<std::vector<TetID_t>> child_indices(8);
    for (int i = 0; const TetID_t child_id : child_ids) {
        child_indices[i++] = forest.get_indices(child_id);
    }

    int top_child_local_index = -1;
    for (int i = 0; const auto &child_idx : child_indices) {
        if (std::ranges::find(child_idx, top_vtx_index) != child_idx.end()) {
            top_child_local_index = i;
            break;
        }
        ++i;
    }

    std::array<TetID_t, 3> shared{};
    for (int i = 0; const TetID_t idx : child_indices[top_child_local_index]) {
        if (idx != top_vtx_index) {
            shared[i++] = idx;
        }
    }
    std::ranges::sort(shared);

    TetID_t top_inner_vertex_index = -1;
    for (int i = 0; i < 8; ++i) {
        if (i == top_child_local_index) {
            continue;
        }

        auto &child_idx = child_indices[i];
        std::ranges::sort(child_idx);
        std::vector<TetID_t> diff;
        std::ranges::set_difference(child_idx, shared,
                                    std::back_inserter(diff));
        if (diff.size() == 1) {
            top_inner_vertex_index = diff[0];
            break;
        }
    }

    return top_inner_vertex_index;
}

TetID_t get_child_with_vtx(const TetForest &forest, const TetID_t vtx_id,
                           const std::vector<TetID_t> &c_ids) {
    for (const TetID_t child_id : c_ids) {
        const std::vector<TetID_t> indices = forest.get_indices(child_id);
        if (std::ranges::find(indices, vtx_id) != indices.end()) {
            return child_id;
        }
    }
    return -1;
}

TetID_t get_neighbor(const TetForest &forest, const TetID_t neighbor_id,
                     const std::vector<TetID_t> &c_ids) {
    std::vector<TetID_t> neighbor_idx = forest.get_indices(neighbor_id);
    std::ranges::sort(neighbor_idx);
    for (const TetID_t child_id : c_ids) {
        if (child_id == neighbor_id) {
            continue;
        }

        std::vector<TetID_t> indices = forest.get_indices(child_id);
        std::ranges::sort(indices);

        std::vector<TetID_t> isect;
        std::ranges::set_intersection(neighbor_idx, indices,
                                      std::back_inserter(isect));
        if (isect.size() == 3) {
            return child_id;
        }
    }
    return -1;
}

std::vector<TetID_t> get_shared_vertex_indices(const TetForest &forest,
                                               const TetID_t id_0,
                                               const TetID_t id_1) {
    const std::vector<TetID_t> ind_0 = forest.get_indices(id_0);
    const std::vector<TetID_t> ind_1 = forest.get_indices(id_1);
    std::vector<TetID_t> shared;
    for (const TetID_t i_0 : ind_0) {
        for (const TetID_t i_1 : ind_1) {
            if (i_0 == i_1) {
                shared.push_back(i_0);
                break;
            }
        }
    }
    return shared;
}

std::vector<TetID_t> get_ordered_child_ids(const TetForest &forest,
                                           const TetID_t id,
                                           const std::vector<TetID_t> &c_ids) {
    const std::vector<TetID_t> indices = forest.get_indices(id);

    std::vector<TetID_t> child_ids;
    child_ids.reserve(8);
    for (const TetID_t i : indices) {
        TetID_t child_id = get_child_with_vtx(forest, i, c_ids);
        TetID_t child_neighbor_id = get_neighbor(forest, child_id, c_ids);
        child_ids.push_back(child_id);
        child_ids.push_back(child_neighbor_id);
    }

    return child_ids;
}

void TetForest::sample_vertex_data() {
    std::vector<TetID_t> vertex_degrees(vertices.size());
    std::vector<Vec3::Float> vertex_distances(vertices.size());
    std::vector vtx_data(num_fields, std::vector<Float>(vertices.size()));

    for (TetID_t i = 0; i < num_elements; ++i) {
        auto vtx = get_vertices(i);
        auto centroid = (vtx[0] + vtx[1] + vtx[2] + vtx[3]) / 4;
        if (child_counts[i] == 0) {
            for (const TetID_t vtx_id : get_indices(i)) {
                ++vertex_degrees[vtx_id];
                const auto dist = 1 / vertices[vtx_id].distance_to(centroid);
                vertex_distances[vtx_id] += dist;
                for (int k = 0; k < num_fields; ++k) {
                    vtx_data[k][vtx_id] +=
                        cell_data[k][i] * static_cast<float>(dist);
                }
            }
        }
    }

    for (auto &field : vtx_data) {
        for (TetID_t j = 0; j < vertices.size(); ++j) {
            // field[j] /= static_cast<Float>(vertex_degrees[j]);
            field[j] /= static_cast<Float>(vertex_distances[j]);
        }
    }

    this->vertex_data = vtx_data;
}

// only looking at cell data to compute the ranges
void TetForest::calculate_field_ranges() {
    field_ranges = std::vector<std::pair<Float, Float>>();
    for (const auto &field : cell_data) {
        field_ranges.emplace_back(*std::ranges::min_element(field),
                                  *std::ranges::max_element(field));
    }
}

// get the ids of the children of an element, with the following order:
// TOP, INNER_TOP, LEFT, INNER_LEFT, RIGHT, INNER_RIGHT, BACK, INNER_BACK
// child element order is determined wrt the parent vertex order
std::vector<TetID_t>
get_ordered_children_ids(const TetForest &forest, const TetID_t id,
                         const std::vector<TetID_t> &c_ids) {
    const auto parent_ind = forest.get_indices(id);
    std::vector<TetID_t> ordered_child_ids(8);
    for (int i = 0; i < 4; ++i) {
        ordered_child_ids[2 * i] =
            get_child_with_vtx(forest, parent_ind[i], c_ids);
        ordered_child_ids[2 * i + 1] =
            get_neighbor(forest, ordered_child_ids[2 * i], c_ids);
    }
    return ordered_child_ids;
}

// get the vertex order for a non-root element with children
std::vector<std::vector<TetID_t>>
get_children_vertex_order(const TetForest &forest, const TetID_t id,
                          const std::vector<TetID_t> &c_ids) {
    const auto parent_ind = forest.get_indices(id);

    const std::vector<TetID_t> ordered_child_ids =
        get_ordered_children_ids(forest, id, c_ids);

    // midpoint vertices
    const TetID_t lb = get_shared_vertex_indices(forest, ordered_child_ids[2],
                                                 ordered_child_ids[6])[0];
    const TetID_t tr = get_shared_vertex_indices(forest, ordered_child_ids[0],
                                                 ordered_child_ids[4])[0];
    const TetID_t tb = get_shared_vertex_indices(forest, ordered_child_ids[0],
                                                 ordered_child_ids[6])[0];
    const TetID_t tl = get_shared_vertex_indices(forest, ordered_child_ids[0],
                                                 ordered_child_ids[2])[0];
    const TetID_t lr = get_shared_vertex_indices(forest, ordered_child_ids[2],
                                                 ordered_child_ids[4])[0];
    const TetID_t rb = get_shared_vertex_indices(forest, ordered_child_ids[4],
                                                 ordered_child_ids[6])[0];

    return {
        // top
        {parent_ind[0], tl, tr, tb},
        {lb,            tb, tr, tl},
        // left
        {parent_ind[1], tl, lb, lr},
        {tr,            lr, lb, tl},
        // right
        {parent_ind[2], rb, tr, lr},
        {lb,            lr, tr, rb},
        // back
        {parent_ind[3], rb, lb, tb},
        {tr,            tb, lb, rb}
    };
}

// get the vertex order for a root element
std::vector<TetID_t> get_root_vertex_order(const TetForest &forest,
                                           const TetID_t id,
                                           const std::vector<TetID_t> &c_ids) {
    std::vector<TetID_t> root_idx = forest.get_indices(id);
    if (forest.child_counts[id] == 0) {
        if (signed_volume(forest.get_vertices(id)) < 0) {
            std::swap(root_idx[2], root_idx[3]);
        }
    } else {
        const TetID_t a = get_child_with_vtx(forest, root_idx[1], c_ids);
        const TetID_t b = get_child_with_vtx(forest, root_idx[2], c_ids);
        const TetID_t c = get_child_with_vtx(forest, root_idx[3], c_ids);
        const TetID_t ab = get_shared_vertex_indices(forest, a, b)[0];
        const TetID_t ac = get_shared_vertex_indices(forest, a, c)[0];

        const TetID_t top_inner =
            get_opposite_index(forest, root_idx[0], c_ids);
        if (ab == top_inner) {
            root_idx = {root_idx[0], root_idx[1], root_idx[3], root_idx[2]};
        } else if (ac == top_inner) {
            root_idx = {root_idx[0], root_idx[1], root_idx[2], root_idx[3]};
        } else { // bc
            root_idx = {root_idx[0], root_idx[2], root_idx[1], root_idx[3]};
        }

        const std::vector<Vec3> root_vtx{
            forest.vertices[root_idx[0]], forest.vertices[root_idx[1]],
            forest.vertices[root_idx[2]], forest.vertices[root_idx[3]]};

        if (signed_volume(root_vtx) < 0) {
            std::swap(root_idx[1], root_idx[3]);
        }
    }
    return root_idx;
}

void TetForest::reorder_elements() {
    const std::vector<std::vector<TetID_t>> child_ids = get_child_ids();
    std::vector<TetID_t> tree_roots(num_trees);
    for (TetID_t i = 0; i < num_elements; ++i) {
        const TetID_t tree_id = tree_ids[i];
        tree_roots[tree_id] = std::max(tree_roots[tree_id], i);
    }

    std::vector<TetID_t> old_ids(num_elements);
    std::stack<std::pair<TetID_t, std::vector<TetID_t>>> dfs;

    TetID_t curr_id = 0;
    for (TetID_t i = 0; i < num_trees; ++i) {
        const TetID_t root_id = tree_roots[i];

        dfs.emplace(root_id,
                    get_root_vertex_order(*this, root_id, child_ids[root_id]));
        while (!dfs.empty()) {
            const TetID_t id = dfs.top().first;
            const std::vector<TetID_t> idx = dfs.top().second;

            old_ids[curr_id++] = id;
            dfs.pop();
            const std::vector<TetID_t> &c_ids = child_ids[id];
            std::ranges::copy(
                idx,
                indices.begin() +
                    static_cast<std::vector<TetID_t>::difference_type>(4 * id));

            if (child_counts[id] == 0) {
                continue;
            }

            const std::vector<TetID_t> tet_child_ids =
                get_ordered_children_ids(*this, id, c_ids);
            const auto tet_child_indices =
                get_children_vertex_order(*this, id, c_ids);

            for (int j = static_cast<int>(tet_child_ids.size()) - 1; j >= 0;
                 --j) {
                dfs.emplace(tet_child_ids[j], tet_child_indices[j]);
            }
        }
    }

    reorder_vector_multiple(old_ids, indices, 4);

    for (auto &field : cell_data) {
        reorder_vector(old_ids, field);
    }

    reorder_vector(old_ids, refinement_levels);
    reorder_vector(old_ids, tree_ids);
    reorder_vector(old_ids, child_counts);

    reorder_vector(old_ids, parent_ids);
    std::vector<TetID_t> new_ids(old_ids.size());
    for (TetID_t i = 0; i < old_ids.size(); ++i) {
        new_ids[old_ids[i]] = i;
    }
    for (unsigned long long &parent_id : parent_ids) {
        parent_id = new_ids[parent_id];
    }
}

void TetForest::remove_non_root_vertices() {
    std::unordered_map<TetID_t, TetID_t> old_to_new_indices;
    std::vector<TetID_t> new_indices;
    std::vector<Vec3> new_vertices;
    for (const TetID_t id : get_root_ids()) {
        for (const TetID_t i : get_indices(id)) {
            if (!old_to_new_indices.contains(i)) {
                old_to_new_indices[i] = new_vertices.size();
                new_vertices.push_back(vertices[i]);
            }
            new_indices.push_back(old_to_new_indices[i]);
        }
    }
    indices = new_indices;
    vertices = new_vertices;
}

TetForest &TetForest::process() {
    std::cout << "Calculating data field ranges" << std::endl;
    calculate_field_ranges();

    std::cout << "Removing duplicate vertices" << std::endl;
    deduplicate_vertices();

    std::cout << "Creating parent elements" << std::endl;
    create_parent_elements();

    std::cout << "Calculating children counts" << std::endl;
    calculate_children_counts();

    std::cout << "Sampling vertex data" << std::endl;
    sample_vertex_data();

    std::cout << "Reordering elements" << std::endl;
    reorder_elements();

    std::cout << "Creating root neighborhood info" << std::endl;
    create_neighborhood_info();

    return *this;
}

inline float det(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d) {
    const Vec3 ba = b - a;
    const Vec3 ca = c - a;
    const Vec3 da = d - a;
    return Vec3::dot(Vec3::cross(ba, ca), da);
}

// positive if bcd is pointing to a
inline float det2(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d) {
    const Vec3 cb = c - b;
    const Vec3 db = d - b;
    const Vec3 ab = a - b;
    return Vec3::dot(Vec3::cross(cb, db), ab);
}

TetID_t element_at_point(const TetForest &forest, const Vec3 &point,
                         const TetID_t tree_root_id,
                         const unsigned int max_r_level) {
    const auto &vertices = forest.vertices;
    const auto &child_nums = forest.child_counts;
    unsigned long long tet_id = tree_root_id;

    const auto idx = forest.get_indices(tree_root_id);
    Vec3 t = vertices[idx[0]];
    Vec3 l = vertices[idx[1]];
    Vec3 r = vertices[idx[2]];
    Vec3 b = vertices[idx[3]];

    int r_level = 0;
    Vec3 v0 = t, v1 = l, v2 = r, v3 = b;

    float h_vol = det(t, r, l, b) / 2;

    while (child_nums[tet_id] > 0 && r_level < max_r_level) {
        float u = det(l, point, r, b);
        float v = det(point, t, r, b);
        float w = det(l, t, point, b);
        float x = 2.f * h_vol - u - v - w;

        bool rx = u + v < h_vol;
        bool ry = v + w < h_vol;
        bool rz = (u < h_vol) & (v < h_vol) & (w < h_vol) & (x < h_vol);
        int tet = (rx * 4) + ((rx == ry) * 2) + rz;

        switch (tet) {
        case 0:
            v0 = t;
            v1 = (t + l) / 2.f;
            v2 = (t + r) / 2.f;
            v3 = (t + b) / 2.f;
            break;
        case 1:
            v0 = (l + b) / 2.f;
            v1 = (t + b) / 2.f;
            v2 = (t + r) / 2.f;
            v3 = (t + l) / 2.f;
            break;
        case 2:
            v0 = l;
            v1 = (l + t) / 2.f;
            v2 = (l + b) / 2.f;
            v3 = (l + r) / 2.f;
            break;
        case 3:
            v0 = (r + t) / 2.f;
            v1 = (l + r) / 2.f;
            v2 = (l + b) / 2.f;
            v3 = (l + t) / 2.f;
            break;
        case 4:
            v0 = r;
            v1 = (r + b) / 2.f;
            v2 = (r + t) / 2.f;
            v3 = (r + l) / 2.f;
            break;
        case 5:
            v0 = (b + l) / 2.f;
            v1 = (r + l) / 2.f;
            v2 = (r + t) / 2.f;
            v3 = (r + b) / 2.f;
            break;
        case 6:
            v0 = b;
            v1 = (b + r) / 2.f;
            v2 = (b + l) / 2.f;
            v3 = (b + t) / 2.f;
            break;
        case 7:
            v0 = (r + t) / 2.f;
            v1 = (b + t) / 2.f;
            v2 = (b + l) / 2.f;
            v3 = (b + r) / 2.f;
            break;
        default:
            break;
        }

        t = v0;
        l = v1;
        r = v2;
        b = v3;

        h_vol /= 8.f;

        unsigned long long child_offset = 1;
        for (int i = 0; i < tet; i++) {
            child_offset += child_nums[tet_id + child_offset] + 1;
        }

        ++r_level;
        tet_id += child_offset;
    }

    return tet_id;
}

void TetForest::create_neighborhood_info() {
    struct Face {
        std::array<TetID_t, 3> indices;
        TetID_t element_id;
        // we store the face neighbors at the index of the opposite vertex
        // so since the top vertex has the index 0,
        // the L-R-B face neighbor gets stored at index 0
        int face_index;

        static std::array<TetID_t, 3> create_face_index_arr(const TetID_t i0,
                                                            const TetID_t i1,
                                                            const TetID_t i2) {
            std::array idx{i0, i1, i2};
            std::ranges::sort(idx);
            return idx;
        }
    };

    // since the coarse mesh is a tet-mesh
    // every root element face either has a direct face neighbor,
    // or is on the boundary of the mesh, if we are on the boundary
    // then the neighbor_id will be TetID_t's max value
    std::vector<std::vector<TetID_t>> root_neighbors_coarse_ids =
        std::vector<std::vector<TetID_t>>(
            num_trees, {std::numeric_limits<TetID_t>::max(),
                        std::numeric_limits<TetID_t>::max(),
                        std::numeric_limits<TetID_t>::max(),
                        std::numeric_limits<TetID_t>::max()});
    std::vector<Face> faces;
    faces.reserve(num_trees * 4);
    const auto root_ids = get_root_ids();
    for (int i = 0; i < num_trees; ++i) {
        const auto idx = get_indices(root_ids[i]);
        faces.emplace_back(Face::create_face_index_arr(idx[1], idx[2], idx[3]),
                           i, 0);
        faces.emplace_back(Face::create_face_index_arr(idx[0], idx[2], idx[3]),
                           i, 1);
        faces.emplace_back(Face::create_face_index_arr(idx[0], idx[1], idx[3]),
                           i, 2);
        faces.emplace_back(Face::create_face_index_arr(idx[0], idx[1], idx[2]),
                           i, 3);
    }

    std::ranges::sort(faces, [](auto const &l, auto const &r) {
        return l.indices < r.indices;
    });

    for (int i = 0; i < faces.size(); ++i) {
        if (faces[i].indices == faces[i + 1].indices) {
            root_neighbors_coarse_ids[faces[i].element_id]
                                     [faces[i].face_index] =
                                         faces[i + 1].element_id;
            root_neighbors_coarse_ids[faces[i + 1].element_id]
                                     [faces[i + 1].face_index] =
                                         faces[i].element_id;
            i += 1;
        }
    }

    neighbors = std::vector<std::vector<TetID_t>>(
        num_elements, {std::numeric_limits<TetID_t>::max(),
                       std::numeric_limits<TetID_t>::max(),
                       std::numeric_limits<TetID_t>::max(),
                       std::numeric_limits<TetID_t>::max()});

    unsigned long long max_level_diff = 0;
    // for each non-root element of each tree find the 4 face neighbors
    for (int i = 0; i < num_trees; ++i) {
        const TetID_t root_id = root_ids[i];
        auto root_vtx = get_vertices(root_id);
        auto root_neighbors = root_neighbors_coarse_ids[i];
        for (int j = 0; j < 4; ++j) {
            if (root_neighbors[j] != std::numeric_limits<TetID_t>::max()) {
                root_neighbors[j] =
                    root_ids[root_neighbors[j]] | 0x8000000000000000ULL;
            }
        }
        // we have inserted the root neighbors
        const float root_vol =
            det2(root_vtx[0], root_vtx[1], root_vtx[2], root_vtx[3]);
        neighbors[root_id] = root_neighbors;

        for (TetID_t j = root_id + 1; j < root_id + 1 + child_counts[root_id];
             ++j) {
            auto vtx = get_vertices(j);
            auto &element_neighbors = neighbors[j];
            Vec3 vtx_sum = vtx[0] + vtx[1] + vtx[2] + vtx[3];
            for (int k = 0; k < 4; ++k) {
                // face centroid
                Vec3 point = (vtx_sum - vtx[k]) / 3;
                point = point + (point - vtx[k]) * 0.0001f;
                const float u =
                    det2(point, root_vtx[1], root_vtx[2], root_vtx[3]);
                const float v =
                    det2(point, root_vtx[0], root_vtx[3], root_vtx[2]);
                const float w =
                    det2(point, root_vtx[0], root_vtx[1], root_vtx[3]);
                const float x = root_vol - u - v - w;
                TetID_t neighbor_coarse_id = root_id;
                if (u < 0.f) {
                    neighbor_coarse_id = root_neighbors[0];
                } else if (v < 0.f) {
                    neighbor_coarse_id = root_neighbors[1];
                } else if (w < 0.f) {
                    neighbor_coarse_id = root_neighbors[2];
                } else if (x < 0.f) {
                    neighbor_coarse_id = root_neighbors[3];
                }

                TetID_t neighbor_id = std::numeric_limits<TetID_t>::max();

                // we aren't on the boundary, so there is a neighbor on the
                // other side
                if (neighbor_coarse_id != std::numeric_limits<TetID_t>::max()) {
                    neighbor_id = element_at_point(
                        *this, point, neighbor_coarse_id, refinement_levels[j]);
                    if (neighbor_coarse_id != root_id) {
                        // std::cout << "Not equal!" << std::endl;
                        neighbor_id |= 0x8000000000000000ULL;
                    }
                    unsigned long long level_diff =
                        refinement_levels[j] - refinement_levels[neighbor_id];
                    max_level_diff = std::max(level_diff, max_level_diff);
                    if (level_diff > 0) {
                        level_diff &= 0x7F;
                        neighbor_id |= level_diff << 56;
                    }
                }

                element_neighbors[k] = neighbor_id;
            }
        }
    }
    std::cout << "max level diff: " << max_level_diff << std::endl;
}

inline std::array<TetID_t, 3>
get_sorted_indices(const std::array<TetID_t, 3> &idx) {
    auto result = idx;
    if (result[0] > result[1]) {
        std::swap(result[0], result[1]);
    }
    if (result[1] > result[2]) {
        std::swap(result[1], result[2]);
    }
    if (result[0] > result[1]) {
        std::swap(result[0], result[1]);
    }
    return result;
}
