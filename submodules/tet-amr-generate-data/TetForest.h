#pragma once

#include "Vec3.h"

#include <string>
#include <vector>

class TetForest {
  public:
    using Float = float;
    using TetID_t = unsigned long long;

    static constexpr std::string magic{"TETAMR-V1.10"};

    TetID_t num_trees = 0;
    TetID_t num_elements = 0;

    unsigned int num_fields = 0;
    std::vector<std::pair<Float, Float>> field_ranges;
    std::vector<std::string> field_names;

    std::vector<Vec3> vertices;
    std::vector<TetID_t> indices;

    std::vector<std::vector<Float>> cell_data;
    std::vector<std::vector<Float>> vertex_data;

    std::vector<unsigned int> refinement_levels;
    std::vector<TetID_t> tree_ids;
    std::vector<TetID_t> parent_ids;
    std::vector<TetID_t> child_counts;
    // index i has the face neighbor list for the ith root element
    std::vector<std::vector<TetID_t>> neighbors;

    [[nodiscard]] std::vector<TetID_t> get_indices(TetID_t tet_id) const;
    [[nodiscard]] std::vector<Vec3> get_vertices(TetID_t tet_id) const;
    [[nodiscard]] std::vector<std::vector<TetID_t>> get_child_ids() const;
    [[nodiscard]] std::vector<TetID_t> get_child_ids(TetID_t tet_id) const;
    [[nodiscard]] std::vector<TetID_t> get_root_ids() const;

    TetForest() = default;

    explicit TetForest(const unsigned int n_fields) : num_fields(n_fields) {
        cell_data = std::vector<std::vector<Float>>(n_fields);
    }

    void deduplicate_vertices();
    void add_element_to_forest_with_vertices(const std::array<Vec3, 4> &vtx,
                                             const std::vector<Float> &c_data,
                                             unsigned int refinement_level,
                                             TetID_t tree_id);
    void add_element_to_forest_with_indices(const std::array<TetID_t, 4> &idx,
                                            const std::vector<Float> &c_data,
                                            unsigned int refinement_level,
                                            TetID_t tree_id);
    void create_parent_elements();
    void reorder_elements();
    void create_neighborhood_info();
    void calculate_children_counts();
    void sample_vertex_data();
    void calculate_field_ranges();
    void remove_non_root_vertices();
    TetForest &process();

    void save(const std::string &filename) const;
    void save_compressed(const std::string &filename) const;
    void save_barebones(const std::string &filename) const;
    void save_compressed_barebones(const std::string &filename) const;
    static TetForest load(const std::string &filename);
    static TetForest load_compressed(const std::string &filename);
    static TetForest
    from_vtu(const std::string &filename,
             const std::vector<std::string> &priority_fields = {},
             const std::vector<std::string> &fields_to_include = {});
};
