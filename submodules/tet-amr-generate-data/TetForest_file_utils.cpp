#include "TetForest.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>

#include <zstd.h>

using TetID_t = TetForest::TetID_t;

std::vector<Vec3::Float> get_flat_vertices(const std::vector<Vec3> &vertices) {
    std::vector<Vec3::Float> vertices_flat;
    vertices_flat.reserve(3 * vertices.size());
    for (const Vec3 &vtx : vertices) {
        vertices_flat.insert(vertices_flat.end(), {vtx.x, vtx.y, vtx.z});
    }
    return vertices_flat;
}

std::vector<TetID_t>
get_flat_neighbors(const std::vector<std::vector<TetID_t>> &neighbors) {
    std::vector<TetID_t> neighbors_flat;
    neighbors_flat.reserve(4 * neighbors.size());
    for (const auto &n : neighbors) {
        neighbors_flat.insert(neighbors_flat.end(), n.begin(), n.end());
    }
    return neighbors_flat;
}

template <typename T>
void write_to_ostream(std::ostream &os, const T &data) {
    os.write(reinterpret_cast<const char *>(&data), sizeof(T));
}

template <typename T>
void read_from_istream(std::istream &is, T &data) {
    is.read(reinterpret_cast<char *>(&data), sizeof(T));
}

template <typename T>
void write_vector_to_ostream(std::ostream &os, const std::vector<T> &vec) {
    os.write(reinterpret_cast<const char *>(vec.data()),
             vec.size() * sizeof(T));
}

template <typename T>
void read_vector_from_istream(std::istream &is, std::vector<T> &vec) {
    is.read(reinterpret_cast<char *>(vec.data()), vec.size() * sizeof(T));
}

void write_forest_to_ostream(const TetForest &forest, std::ostream &os) {
    os.write(TetForest::magic.data(), TetForest::magic.size());
    const bool has_vertex_data = !forest.vertex_data.empty();
    write_to_ostream(os, has_vertex_data);
    const bool has_child_counts = !forest.child_counts.empty();
    write_to_ostream(os, has_child_counts);

    write_to_ostream(os, forest.num_trees);
    write_to_ostream(os, forest.num_elements);
    write_to_ostream(os, forest.num_fields);

    for (const std::string &name : forest.field_names) {
        // +1 size for the null terminator
        os.write(name.data(), static_cast<std::streamsize>(name.size() + 1));
    }

    for (const auto [min, max] : forest.field_ranges) {
        write_to_ostream(os, min);
        write_to_ostream(os, max);
    }

    write_vector_to_ostream(os, forest.refinement_levels);
    write_vector_to_ostream(os, forest.tree_ids);
    write_vector_to_ostream(os, forest.parent_ids);
    if (has_child_counts) {
        write_vector_to_ostream(os, forest.child_counts);
    }

    write_to_ostream(os, forest.vertices.size());
    write_vector_to_ostream(os, get_flat_vertices(forest.vertices));
    write_vector_to_ostream(os, forest.indices);

    for (const auto &field : forest.cell_data) {
        write_vector_to_ostream(os, field);
    }
    for (const auto &field : forest.vertex_data) {
        write_vector_to_ostream(os, field);
    }

    write_vector_to_ostream(os, get_flat_neighbors(forest.neighbors));
}

void write_forest_to_ostream_barebones(const TetForest &forest,
                                       std::ostream &os) {
    os.write(TetForest::magic.data(), TetForest::magic.size());
    const bool has_vertex_data = !forest.vertex_data.empty();
    write_to_ostream(os, has_vertex_data);
    const bool has_child_counts = !forest.child_counts.empty();
    write_to_ostream(os, has_child_counts);

    write_to_ostream(os, forest.num_trees);
    write_to_ostream(os, forest.num_elements);
    write_to_ostream(os, forest.num_fields);

    for (const std::string &name : forest.field_names) {
        // +1 size for the null terminator
        os.write(name.data(), static_cast<std::streamsize>(name.size() + 1));
    }

    if (has_child_counts) {
        write_vector_to_ostream(os, forest.child_counts);
    }
    std::vector<Vec3> root_vertices;
    root_vertices.reserve(4 * forest.num_trees);
    for (const auto id : forest.get_root_ids()) {
        const auto vtx = forest.get_vertices(id);
        root_vertices.insert(root_vertices.end(), vtx.begin(), vtx.end());
    }

    write_vector_to_ostream(os, get_flat_vertices(root_vertices));

    for (const auto &field : forest.cell_data) {
        write_vector_to_ostream(os, field);
    }
}

bool read_forest_from_istream(TetForest &forest, std::istream &is) {
    std::string magic;
    magic.resize(TetForest::magic.size());
    is.read(magic.data(), TetForest::magic.size());
    if (magic != TetForest::magic) {
        std::cerr << "TetForest version mismatch" << std::endl;
        return false;
    }
    bool has_vertex_data;
    read_from_istream(is, has_vertex_data);
    bool has_child_counts;
    read_from_istream(is, has_child_counts);

    read_from_istream(is, forest.num_trees);
    read_from_istream(is, forest.num_elements);
    read_from_istream(is, forest.num_fields);

    for (decltype(forest.num_fields) i = 0; i < forest.num_fields; ++i) {
        std::string name;
        std::getline(is, name, '\0');
        forest.field_names.push_back(name);
    }

    for (decltype(forest.num_fields) i = 0; i < forest.num_fields; ++i) {
        TetForest::Float min, max;
        read_from_istream(is, min);
        read_from_istream(is, max);
        forest.field_ranges.emplace_back(min, max);
    }

    forest.refinement_levels.resize(forest.num_elements);
    read_vector_from_istream(is, forest.refinement_levels);
    forest.tree_ids.resize(forest.num_elements);
    read_vector_from_istream(is, forest.tree_ids);
    forest.parent_ids.resize(forest.num_elements);
    read_vector_from_istream(is, forest.parent_ids);
    if (has_child_counts) {
        forest.child_counts.resize(forest.num_elements);
        read_vector_from_istream(is, forest.child_counts);
    }

    TetID_t vertices_size;
    read_from_istream(is, vertices_size);
    std::vector<Vec3::Float> vertices_flat(3 * vertices_size);
    read_vector_from_istream(is, vertices_flat);
    forest.vertices.resize(vertices_size);
    for (TetID_t i = 0; i < vertices_size; ++i) {
        forest.vertices[i] = {vertices_flat[3 * i + 0],
                              vertices_flat[3 * i + 1],
                              vertices_flat[3 * i + 2]};
    }

    forest.indices.resize(4 * forest.num_elements);
    read_vector_from_istream(is, forest.indices);

    forest.cell_data.resize(forest.num_fields);
    for (auto &field : forest.cell_data) {
        field.resize(forest.num_elements);
        read_vector_from_istream(is, field);
    }

    if (has_vertex_data) {
        forest.vertex_data.resize(forest.num_fields);
        for (auto &field : forest.vertex_data) {
            field.resize(vertices_size);
            read_vector_from_istream(is, field);
        }
    }

    std::vector<TetID_t> neighbors_flat;
    neighbors_flat.resize(forest.num_elements * 4);
    read_vector_from_istream(is, neighbors_flat);
    forest.neighbors.resize(forest.num_elements);
    for (int i = 0; i < forest.num_elements; i++) {
        forest.neighbors[i] = {neighbors_flat[4 * i], neighbors_flat[4 * i + 1],
                               neighbors_flat[4 * i + 2],
                               neighbors_flat[4 * i + 3]};
    }

    return is.peek() == EOF;
}

void TetForest::save(const std::string &filename) const {
    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename
                  << std::endl;
        std::exit(1);
    }

    write_forest_to_ostream(*this, ofs);

    ofs.close();
}

void TetForest::save_barebones(const std::string &filename) const {
    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename
                  << std::endl;
        std::exit(1);
    }

    write_forest_to_ostream_barebones(*this, ofs);

    ofs.close();
}

void TetForest::save_compressed(const std::string &filename) const {
    std::stringstream stream;
    write_forest_to_ostream(*this, stream);

    // seek to the end of the file, get the position,
    // then seek back to the beginning of the file
    stream.seekg(0, std::ios_base::end);
    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    stream.read(buffer.data(), size);
    // empty the stringstream
    // clear is probably not needed if we aren't going to reuse the stream
    stream.str(std::string());
    stream.clear();

    size_t compress_bound = ZSTD_compressBound(buffer.size());
    std::vector<char> compressed(compress_bound);

    size_t compressed_size = ZSTD_compress(compressed.data(), compressed.size(),
                                           buffer.data(), buffer.size(), 0);

    // clear the buffer after compressing it
    buffer.resize(0);

    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename
                  << std::endl;
        std::exit(1);
    }

    ofs.write(compressed.data(), static_cast<std::streamsize>(compressed_size));
    ofs.close();
}

void TetForest::save_compressed_barebones(const std::string &filename) const {
    std::stringstream stream;
    write_forest_to_ostream_barebones(*this, stream);

    // seek to the end of the file, get the position,
    // then seek back to the beginning of the file
    stream.seekg(0, std::ios_base::end);
    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    stream.read(buffer.data(), size);
    // empty the stringstream
    // clear is probably not needed if we aren't going to reuse the stream
    stream.str(std::string());
    stream.clear();

    size_t compress_bound = ZSTD_compressBound(buffer.size());
    std::vector<char> compressed(compress_bound);

    size_t compressed_size = ZSTD_compress(compressed.data(), compressed.size(),
                                           buffer.data(), buffer.size(), 0);

    // clear the buffer after compressing it
    buffer.resize(0);

    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename
                  << std::endl;
        std::exit(1);
    }

    ofs.write(compressed.data(), static_cast<std::streamsize>(compressed_size));
    ofs.close();
}

TetForest TetForest::load(const std::string &filename) {
    TetForest forest;
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open file: " << std::endl;
        std::exit(1);
    }

    if (!read_forest_from_istream(forest, ifs)) {
        std::cerr << "Failed to read forest from file" << std::endl;
        std::exit(1);
    }
    ifs.close();

    return forest;
}

TetForest TetForest::load_compressed(const std::string &filename) {
    TetForest forest;
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        std::exit(1);
    }

    // seek to the end of the file, get the position,
    // then seek back to the beginning of the file
    ifs.seekg(0, std::ios_base::end);
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!ifs.read(buffer.data(), size)) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        std::exit(1);
    }
    ifs.close();

    size_t frame_content_size =
        ZSTD_getFrameContentSize(buffer.data(), buffer.size());
    std::vector<char> decompressed(frame_content_size);
    size_t decompressed_size = ZSTD_decompress(
        decompressed.data(), decompressed.size(), buffer.data(), buffer.size());
    std::stringstream stream;
    stream.write(decompressed.data(),
                 static_cast<std::streamsize>(decompressed_size));

    if (!read_forest_from_istream(forest, stream)) {
        std::cerr << "Failed to read forest from file" << std::endl;
        std::exit(1);
    }

    return forest;
}
