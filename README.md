# Direct Volume Rendering of Tree-based Tetrahedral Adaptive Mesh Refinement Data

Related publication: 

> **Direct Volume Rendering of Tree-Based Tetrahedral Adaptive Mesh Refinement Data**  
> Musa Ege Ünalan, Serkan Demirci, Stefan Zellmann, Uğur Güdükbay  
> *Computers & Graphics, Volume 138, 2026*
> **DOI:** [10.1016/j.cag.2026.104638](https://doi.org/10.1016/j.cag.2026.104638)

## Overview

Tree-based tetrahedral adaptive mesh refinement (Tet-AMR) combines the advantages of unstructured tetrahedral meshes and adaptive mesh refinement (AMR). A Tet-AMR dataset consists of a coarse tetrahedral mesh and a forest of refinement trees rooted at the coarse elements.

A common approach for visualizing Tet-AMR data is to flatten the hierarchy into a fully refined unstructured tetrahedral mesh. While straightforward, this significantly increases memory consumption because every leaf tetrahedron must be explicitly stored.

This repository implements a direct volume rendering method that preserves the Tet-AMR hierarchy during rendering. Instead of storing all refined elements, only the coarse mesh and refinement trees are stored. During rendering, finer-level tetrahedra are generated on the fly by traversing the refinement hierarchy.

## Third-Party Libraries and Software

The following third-party libraries and software were used in the development of this work:

- [Optix 7.7](https://developer.nvidia.com/rtx/ray-tracing/optix)
- [OWL: A Productivity Library for OptiX](https://github.com/NVIDIA/OWL)
- [t8code](https://github.com/DLR-AMR/t8code)
- [VTK](https://vtk.org/)
- [Imgui](https://github.com/ocornut/imgui)
- [Tetgen](https://wias-berlin.de/software/index.jsp?id=TetGen&lang=1)
- [CLI11](https://github.com/cliutils/cli11)

## Build and Usage

### Prerequisites

All development and benchmarking for this work were performed on Ubuntu 24.04. Running the renderer requires an NVIDIA GPU with hardware ray-tracing support, along with a compatible NVIDIA driver.

Before building:
* Make sure NVIDIA Linux driver version >=570.26 is installed on the system.
    * On Ubuntu 24.04 the recommended driver can be installed with ```sudo ubuntu-drivers install```, refer to [Ubuntu Docs](https://ubuntu.com/server/docs/how-to/graphics/install-nvidia-drivers/) for more info.
* Install [OptiX 7.7](https://developer.nvidia.com/designworks/optix/downloads/legacy) and set the `OptiX_INSTALL_DIR` environment variable to point to the OptiX installation directory.
* Install the required system packages. On Ubuntu 24.04.4:

```bash
sudo apt install build-essential git git-lfs cmake \ 
    libzstd-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libglfw3-dev
```
### Building

Clone the repository with:
```bash
GIT_LFS_SKIP_SMUDGE=1 git clone https://github.com/Bilkent-ModVis/tet-amr-rendering.git
```

#### Automatic Build

The project includes scripts for building with the CUDA Toolkit and VTK packages from conda-forge using [Micromamba](https://github.com/mamba-org/micromamba-releases):
```bash
./build.sh
```
Builds the project.

```bash
./build_and_run.sh
```
Builds the project and runs the ```mri_tf_opacity_steps_benchmarks.py``` benchmark, which provides the data for Table 3 in the related publication, in ```benchmark/tet_amr_mri_tf_opacity_steps_raymarcher/results.txt```.


#### Manual Build
The manual build uses the system packages for the CUDA toolkit and VTK.

* Install [CUDA Toolkit 12.8](https://docs.nvidia.com/cuda/cuda-installation-guide-linux/)
    * CUDA 13 is not supported because it breaks compatibility with the version of Optix/OWL used in this project.
* Install VTK
    * ```sudo apt install libvtk9-dev```
    * However, the `libvtk9-dev` package provided by Ubuntu 24.04 does not correctly read the volume VTU files used by this project. To avoid this issue, [build and install VTK from source](https://docs.vtk.org/en/latest/build_instructions/index.html) and point CMake to the installation directory with `-DVTK_DIR`. Versions 9.3.1 and 9.6.2 have been tested successfully.

Then run:
```bash
git submodule update --init --recursive

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DVTK_DIR=$VTK_INSTALL_DIR

cmake --build build -j
```

#### Build Artifacts
The build produces the following binaries:

| Binary                               | Description                                                                                          |
|--------------------------------------|------------------------------------------------------------------------------------------------------|
| `tet_amr_volume_render_b`            | Tet-AMR Volume Renderer with barycentric coordinate traversal                                        |
| `tet_amr_volume_render_p`            | Tet-AMR Volume Renderer with point-plane test traversal                                              |
| `tet_amr_volume_render_b_m`          | Tet-AMR Volume Renderer with barycentric coordinate traversal, using macrocells                      |
| `tet_amr_unstructured_volume_render` | Unstructured renderer for Tet-AMR data                                                               |
| `util/tf_reduce_opacity_progressive` | The tool used to create the transfer functions for the `mri_tf_opacity_steps_benchmark.py` benchmark |

#### Usage

The renderers can be run as:

```bash
./tet_amr_volume_render_b [OPTIONS] <tet_amr_volume.vtu>
```

To view all available command-line options:

```bash
./tet_amr_volume_render_b --help
```

#### File Cache

By default, the renderer caches processed volumes in a `.tet_cache` directory created in the current working directory. This behavior can be disabled before
compilation by removing the `USE_FILE_CACHE` macro definition from main.cpp.

### Benchmarks

The benchmarks that reproduce the results in the related publication are available in the `benchmarks` directory.

### Data

If you experience any problems downloading the data, please contact us.

## Citation

Please cite this work if you find it useful:

```
@article{Unalan2026TetAMR,
title = {Direct volume rendering of tree-based tetrahedral adaptive mesh refinement data},
journal = {Computers & Graphics},
volume = {138},
pages = {104638},
year = {2026},
issn = {0097-8493},
doi = {https://doi.org/10.1016/j.cag.2026.104638},
url = {https://www.sciencedirect.com/science/article/pii/S0097849326001093},
author = {Musa Ege Ünalan and Serkan Demirci and Stefan Zellmann and Uğur Güdükbay},
keywords = {Direct volume rendering, Ray tracing, Delta tracking, Acceleration structure, Adaptive mesh refinement},
abstract = {Tree-based tetrahedral AMR data (Tet-AMR) combines the benefits of unstructured meshes and AMR by maintaining both a coarse unstructured tetrahedral mesh and a forest of refinement trees, each rooted at a coarse mesh element. Tet-AMR data can be visualized by combining all leaf tetrahedra in the refinement trees into a single unstructured mesh; however, this approach significantly increases the memory usage. We propose a volume rendering method that leverages the regular subdivision of refinement trees by storing only the coarse elements. We construct a bounding volume hierarchy over the coarse mesh elements to efficiently locate the refinement tree to sample. We then generate the geometry of the finer-level elements on the fly as we traverse the refinement tree to find the leaf element. We further demonstrate that the tree structure can be used to implement a dynamic, view-dependent level-of-detail effect, thereby improving performance by reducing fidelity in less impactful regions. Additionally, we enhance rendering performance by utilizing the coarse mesh as both a space-skipping structure for ray marching and a macrocell structure for Woodcock renderers.}
}
```
