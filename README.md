# Direct Volume Rendering of Tree-based Tetrahedral Adaptive Mesh Refinement Data

Official implementation of the paper: 

> **Direct Volume Rendering of Tree-Based Tetrahedral Adaptive Mesh Refinement Data**  
> Musa Ege Ünalan, Serkan Demirci, Stefan Zellmann, Uğur Güdükbay  
> *Computers & Graphics, Volume 138, 2026*
> **DOI:** [10.1016/j.cag.2026.104638](https://doi.org/10.1016/j.cag.2026.104638)

## Overview

Tree-based tetrahedral adaptive mesh refinement (Tet-AMR) combines the advantages of unstructured tetrahedral meshes and adaptive mesh refinement (AMR). A Tet-AMR dataset consists of a coarse tetrahedral mesh and a forest of refinement trees rooted at the coarse elements.

A common approach for visualizing Tet-AMR data is to flatten the hierarchy into a fully refined unstructured tetrahedral mesh. While straightforward, this significantly increases memory consumption because every leaf tetrahedron must be explicitly stored.

This repository implements a direct volume rendering method that preserves the Tet-AMR hierarchy during rendering. Instead of storing all refined elements, only the coarse mesh and refinement trees are stored. During rendering, finer-level tetrahedra are generated on the fly by traversing the refinement hierarchy.

## Third-Party Libraries

The following third-party libraries were used in the development of this work:

- [Optix 7.7](https://developer.nvidia.com/rtx/ray-tracing/optix)
- [OWL: A Productivity Library for OptiX](https://github.com/NVIDIA/OWL)
- [t8code](https://github.com/DLR-AMR/t8code)
- [VTK](https://vtk.org/)
- [Imgui](https://github.com/ocornut/imgui)
- [Tetgen](https://wias-berlin.de/software/index.jsp?id=TetGen&lang=1)
- [CLI11](https://github.com/cliutils/cli11)

## Building

```
TODO
```

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
