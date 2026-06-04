cmake_minimum_required(VERSION 3.28)

include(FetchContent)

project(tet-amr-volume-render LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CUDA_ARCHITECTURES native)


set(owl_dir ${PROJECT_SOURCE_DIR}/submodules/owl)
add_subdirectory(${owl_dir} EXCLUDE_FROM_ALL)

set(tet_forest_dir ${PROJECT_SOURCE_DIR}/submodules/tet-amr-generate-data)
add_subdirectory(${tet_forest_dir} EXCLUDE_FROM_ALL)


FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.1.tar.gz
        URL_HASH MD5=fdfc58a68ed152b9200aa42bc1aa9363
)
FetchContent_MakeAvailable(imgui)
set(imgui_dir ${FETCHCONTENT_BASE_DIR}/imgui-src)

set(imgui_src
        ${imgui_dir}/imgui.cpp
        ${imgui_dir}/imgui_demo.cpp
        ${imgui_dir}/imgui_draw.cpp
        ${imgui_dir}/imgui_tables.cpp
        ${imgui_dir}/imgui_widgets.cpp
        ${imgui_dir}/backends/imgui_impl_glfw.cpp
        ${imgui_dir}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${imgui_src})
target_include_directories(imgui PUBLIC ${imgui_dir} ${imgui_dir}/backends)


FetchContent_Declare(
        cli11_proj
        QUIET
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.4.2
)

FetchContent_MakeAvailable(cli11_proj)


set(imgui_tfe_dir ${PROJECT_SOURCE_DIR}/imgui_tfe)
set(imgui_tfe_src
        ${imgui_tfe_dir}/ColorMaps.cpp
        ${imgui_tfe_dir}/RGBAEditor.cpp
        ${imgui_tfe_dir}/TFEditor.cpp
)
add_library(imgui_tfe STATIC ${imgui_tfe_src} ${imgui_src})
target_include_directories(imgui_tfe PUBLIC ${imgui_tfe_dir} ${imgui_dir})
target_link_libraries(imgui_tfe
        PRIVATE
        owl
        stb_image
)

find_package(VTK REQUIRED COMPONENTS
        CommonCore
        CommonDataModel
        IOXML
)

add_subdirectory(util)

embed_ptx(
        OUTPUT_TARGET
        tet-amr-unstructured-volume-render-ptx
        PTX_LINK_LIBRARIES
        owl::owl
        SOURCES
        unstructured/device_code.cu
)

add_executable(tet_amr_unstructured_volume_render
        unstructured/main.cpp
        unstructured/renderer.cpp
        owl_viewer_imgui.cpp
        unstructured/viewer.cpp
        unstructured/parse_options.cpp
        unstructured/renderer_common.cpp
        unstructured/benchmark.cpp
)

target_include_directories(tet_amr_unstructured_volume_render
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tet_amr_unstructured_volume_render
        PRIVATE
        tet-amr-unstructured-volume-render-ptx
        owl_viewer
        TetForest
        imgui
        imgui_tfe
        util
        CLI11::CLI11
        VTK::CommonCore
        VTK::CommonDataModel
        VTK::IOXML
)

function(build_config macrocell traversal suffix)
    embed_ptx(
            OUTPUT_TARGET
            tet-amr-volume-render-${suffix}-ptx
            PTX_LINK_LIBRARIES
            owl::owl
            SOURCES
            device_code.cu
    )

    target_compile_definitions(tet-amr-volume-render-${suffix}-ptx_ptx
            PRIVATE
            traverse=${traversal}
            MACROCELL=${macrocell}
    )

    add_executable(tet_amr_volume_render_${suffix}
            main.cpp
            renderer.cpp
            owl_viewer_imgui.cpp
            viewer.cpp
            parse_options.cpp
            density.cu
            renderer_common.cpp
            benchmark.cpp
    )

    target_compile_definitions(tet_amr_volume_render_${suffix}
            PRIVATE
            traverse=${traversal}
            MACROCELL=${macrocell}
    )


    target_link_libraries(tet_amr_volume_render_${suffix}
            PRIVATE
            tet-amr-volume-render-${suffix}-ptx
            owl_viewer
            TetForest
            imgui
            imgui_tfe
            util
            CLI11::CLI11
            VTK::CommonCore
            VTK::CommonDataModel
            VTK::IOXML
    )
endfunction()

# barycentric
build_config(0 traverse_barycentric b)
# point plane
build_config(0 traverse_point_plane p)
# barycentric with macrocell enabled
build_config(1 traverse_barycentric b_m)
