cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(tet-amr-generate-data)
find_package(VTK COMPONENTS IOXML)

# find zstd
find_path(ZSTD_INCLUDE_DIR zstd.h PATH_SUFFIXES zstd PATHS /usr/include /usr/local/include)
find_library(ZSTD_LIBRARY zstd PATH_SUFFIXES lib PATHS /usr/lib /usr/local/lib)

include_directories(${ZSTD_INCLUDE_DIR})
link_directories(${ZSTD_LIBRARY})

add_library(TetForest TetForest.cpp TetForest_file_utils.cpp TetForest_vtk.cpp)
target_include_directories(TetForest PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(TetForest
        PRIVATE
        ${VTK_LIBRARIES}
        ${ZSTD_LIBRARY}
)
vtk_module_autoinit(
        TARGETS TetForest
        MODULES ${VTK_LIBRARIES}
)