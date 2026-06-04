#include "density.cuh"

#include <cfloat>
#include <vector>

__global__ void reset_density_buffers(vec2f *cell_density, vec2f *vertex_density) {
    cell_density[blockIdx.x].x = FLT_MAX;
    cell_density[blockIdx.x].y = 0;
    vertex_density[blockIdx.x].x = FLT_MAX;
    vertex_density[blockIdx.x].y = 0;
}

inline __global__ void recalculateDensityRangesCuda(const size_t gridSize, const owl::interval<float> *scalarRanges,
                                                    const cudaTextureObject_t texture, const int numTexels,
                                                    const owl::interval<float> xfDomain, owl::interval<float> *mRants) {
    const auto cellID = (static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x);
    if (cellID >= gridSize)
        return;

    if (scalarRanges[cellID].empty()) {
        mRants[cellID].lower = 0.f;
        mRants[cellID].upper = 0.f;
        return;
    }

    const float mn = owl::clamp(scalarRanges[cellID].lower, xfDomain.lower, xfDomain.upper);
    const float mx = owl::clamp(scalarRanges[cellID].upper, xfDomain.lower, xfDomain.upper);

    // transform data min max to transfer function space
    const float remappedMin = (mn - xfDomain.lower) / (xfDomain.upper - xfDomain.lower);
    const float remappedMax = (mx - xfDomain.lower) / (xfDomain.upper - xfDomain.lower);
    const float addr1 = remappedMin * static_cast<float>(numTexels);
    const float addr2 = remappedMax * static_cast<float>(numTexels);

    const int addrMin = min(max(static_cast<int>(fminf(floorf(addr1), floorf(addr2))), 0), numTexels - 1);
    const int addrMax = min(max(static_cast<int>(fmaxf(ceilf(addr1), ceilf(addr2))), 0), numTexels - 1);

    float maxDensity = 0.f;
    float minDensity = INFINITY;
    for (int i = addrMin; i <= addrMax; ++i) {
        const float density = tex2D<float4>(texture, static_cast<float>(i) / static_cast<float>(numTexels), 0.5f).w;
        maxDensity = fmaxf(maxDensity, density);
        minDensity = fminf(minDensity, density);
    }

    mRants[cellID].lower = minDensity;
    mRants[cellID].upper = maxDensity;
}

inline __host__ int64_t iDivUp(int64_t a, int64_t b) { return (a + b - 1) / b; }

void recalculateDensityRanges(const size_t gridSize, const owl::interval<float> *scalarRanges,
                              const cudaTextureObject_t texture, const int numTexels,
                              const owl::interval<float> xfDomain, owl::interval<float> *mRants) {
    constexpr auto numThreads = 1;
    recalculateDensityRangesCuda<<<iDivUp(static_cast<int64_t>(gridSize), numThreads), numThreads>>>(
        gridSize, scalarRanges, texture, numTexels, xfDomain, mRants);
}

void Density_CUDA::calculate_density_cuda(interval<float> tf_range, OWLBuffer cell_density, OWLBuffer vertex_density,
                                          OWLBuffer cell_scalar_range, OWLBuffer vertex_scalar_range,
                                          OWLTexture cmap_texture, unsigned long long num_trees) {
    const auto cell_scalar_range_buffer = (interval<float> *)(owlBufferGetPointer(cell_scalar_range, 0));
    const auto vertex_scalar_range_buffer = (interval<float> *)(owlBufferGetPointer(vertex_scalar_range, 0));
    const auto cell_density_buffer = (interval<float> *)(owlBufferGetPointer(cell_density, 0));
    const auto vertex_density_buffer = (interval<float> *)(owlBufferGetPointer(vertex_density, 0));
    recalculateDensityRanges(num_trees, cell_scalar_range_buffer, owlTextureGetObject(cmap_texture, 0), 8192, tf_range,
                             cell_density_buffer);
    recalculateDensityRanges(num_trees, vertex_scalar_range_buffer, owlTextureGetObject(cmap_texture, 0), 8192,
                             tf_range, vertex_density_buffer);

    std::vector<interval<float>> h(num_trees);
    cudaError_t err =
        cudaMemcpy(h.data(), vertex_density_buffer, h.size() * sizeof(interval<float>), cudaMemcpyDeviceToHost);
    int count = 0;
    for (int i = 0; i < num_trees; ++i) {
        if (h[i].upper <= 0.001f) {
            ++count;
        }
    }
    printf("zero density macrocells: %d\n", count);
}
