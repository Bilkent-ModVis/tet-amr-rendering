#pragma once

#include "device_code.h"

namespace Density_CUDA {
void calculate_density_cuda(interval<float> tf_range, OWLBuffer cell_density, OWLBuffer vertex_density,
                            OWLBuffer cell_scalar_range, OWLBuffer vertex_scalar_range, OWLTexture cmap_texture,
                            unsigned long long num_trees);
} // namespace Density_CUDA
