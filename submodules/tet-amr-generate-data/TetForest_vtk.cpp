#include "TetForest.h"
#include <vtkCellData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridReader.h>

TetForest
TetForest::from_vtu(const std::string &filename,
                    const std::vector<std::string> &priority_fields,
                    const std::vector<std::string> &fields_to_include) {

    TetForest forest(0);

    const vtkNew<vtkXMLUnstructuredGridReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();

    vtkUnstructuredGrid *grid = reader->GetOutput();

    vtkCellData *grid_cell_data = grid->GetCellData();

    std::vector<vtkDataArray *> data_arrays;
    for (int i = 0; i < grid_cell_data->GetNumberOfArrays(); ++i) {
        vtkDataArray *const array = grid_cell_data->GetArray(i);
        // TODO: skipping when not a scalar array
        if (array->GetNumberOfComponents() != 1)
            continue;
        std::string array_name{array->GetName()};
        bool add = true;
        if (!fields_to_include.empty()) {
            add = std::ranges::find(fields_to_include, array_name) !=
                  fields_to_include.end();
        }

        if (add) {
            data_arrays.push_back(array);
            forest.cell_data.emplace_back();
            forest.field_names.push_back(array_name);
            ++forest.num_fields;
        }
    }

    int swapped_fields = 0;
    for (const std::string &prio_field : priority_fields) {
        const auto field = std::ranges::find(forest.field_names, prio_field);
        if (field != forest.field_names.end()) {
            const long field_num = field - forest.field_names.begin();
            std::swap(forest.field_names[swapped_fields],
                      forest.field_names[field_num]);
            std::swap(data_arrays[swapped_fields], data_arrays[field_num]);
            ++swapped_fields;
        }
    }

    vtkDataArray *const tree_id_array = grid_cell_data->GetArray("treeid");
    forest.num_trees = static_cast<TetID_t>(tree_id_array->GetRange()[1]) + 1;
    vtkDataArray *const r_level_array = grid_cell_data->GetArray("level");

    const vtkIdType num_elements = grid->GetNumberOfCells();

    for (vtkIdType cell_id = 0; cell_id < num_elements; ++cell_id) {
        const auto tree_id =
            static_cast<TetID_t>(tree_id_array->GetTuple1(cell_id));
        const auto r_level =
            static_cast<decltype(refinement_levels)::size_type>(
                r_level_array->GetTuple1(cell_id));
        vtkPoints *const points = grid->GetPoints();
        vtkIdList *const point_ids = grid->GetCell(cell_id)->GetPointIds();

        std::array<Vec3, 4> vertices;
        std::vector<Float> cell_data(data_arrays.size());

        for (vtkIdType i = 0; i < 4; i++) {
            const vtkIdType point_id = point_ids->GetId(i);
            double vtx[3];
            points->GetPoint(point_id, vtx);
            vertices[i] = {vtx[0], vtx[1], vtx[2]};
        }

        for (std::size_t i = 0; i < data_arrays.size(); ++i) {
            cell_data[i] =
                static_cast<Float>(data_arrays[i]->GetTuple1(cell_id));
        }

        forest.add_element_to_forest_with_vertices(vertices, cell_data, r_level,
                                                   tree_id);
    }

    return forest;
}