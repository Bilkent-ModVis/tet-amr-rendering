#ifdef __JETBRAINS_IDE__
#define __CUDA_ARCH__
#endif

#include "device_code.h"
#include "render_mode_enums.h"
#include <optix_device.h>

#include <owl/common/math/random.h>
using Random = owl::LCG<6>;

extern "C" __constant__ LaunchParams optixLaunchParams;

inline __device__ float det(const vec3f &a, const vec3f &b, const vec3f &c, const vec3f &d) {
    const vec3f ba = b - a;
    const vec3f ca = c - a;
    const vec3f da = d - a;
    return dot(cross(ba, ca), da);
}

inline __device__ float sample_point(const vec3f &point, const unsigned long long coarse_tet_id) {
    const auto &vertices = optixLaunchParams.tetData.vertex;
    const auto &indices = optixLaunchParams.tetData.index;
    float scalar;

    if (optixLaunchParams.dataRenderMode == 0) {
        const auto &cell_data = optixLaunchParams.tetData.cell_data;
        scalar = cell_data[coarse_tet_id];
    } else {
        const auto &vertex_data = optixLaunchParams.tetData.vertex_data;
        const int i0 = indices[4 * coarse_tet_id][0];
        const int i1 = indices[4 * coarse_tet_id][1];
        const int i2 = indices[4 * coarse_tet_id][2];
        const int i3 = indices[4 * coarse_tet_id + 1][2];

        const vec3f v0 = vertices[i0];
        const vec3f v1 = vertices[i1];
        const vec3f v2 = vertices[i2];
        const vec3f v3 = vertices[i3];

        const float vol = det(v0, v1, v2, v3);
        const float u = det(point, v1, v2, v3) / vol;
        const float v = det(v0, point, v2, v3) / vol;
        const float w = det(v0, v1, point, v3) / vol;
        const float x = 1.f - u - v - w;
        scalar = (u * vertex_data[i0] + v * vertex_data[i1] + w * vertex_data[i2] + x * vertex_data[i3]);
    }

    return scalar;
}

inline __device__ void get_vertices(const unsigned long long coarse_tet_id, vec3f &v0, vec3f &v1, vec3f &v2, vec3f &v3,
                                    float &s0, float &s1, float &s2, float &s3) {
    const auto &vertices = optixLaunchParams.tetData.vertex;
    const auto &indices = optixLaunchParams.tetData.index;

    const auto &vertex_data = optixLaunchParams.tetData.vertex_data;
    const int i0 = indices[4 * coarse_tet_id][0];
    const int i1 = indices[4 * coarse_tet_id][1];
    const int i2 = indices[4 * coarse_tet_id][2];
    const int i3 = indices[4 * coarse_tet_id + 1][2];

    v0 = vertices[i0];
    v1 = vertices[i1];
    v2 = vertices[i2];
    v3 = vertices[i3];

    s0 = vertex_data[i0];
    s1 = vertex_data[i1];
    s2 = vertex_data[i2];
    s3 = vertex_data[i3];
}

OPTIX_CLOSEST_HIT_PROGRAM(Tet)() {
    const unsigned int primID = optixGetPrimitiveIndex() + owl::getProgramData<TetData>().base_primitive;
    auto &prd = owl::getPRD<PerRayData>();
    prd.missed = false;

    const float hit_t = optixGetRayTmax();
    const vec3f dir = optixGetWorldRayDirection();
    const vec3f org = optixGetWorldRayOrigin();
    const vec3f hit_P = org + hit_t * dir;
    prd.hit_point = hit_P;
    prd.hit_t = hit_t;

    prd.tet_id = primID / 4;
}

__device__ inline vec4f getColor(const float &scalar) {
    const vec2f range = optixLaunchParams.tf.range;
    const float color_t = (scalar - range.x) / (range.y - range.x);
    auto xf = tex2D<float4>(optixLaunchParams.tf.tfTex, color_t, 0.5f);
    return xf;
}

__device__ inline bool woodcock_no_macrocell(const Ray &ray, Random &rand, vec4f &color, float &t, const float tmax,
                                             const unsigned int coarse_tet_id) {
    float max_sample = optixLaunchParams.tf.opacity;

    if (max_sample <= 0.f)
        return false;

    while (true) {
        vec3f point = ray.origin + t * ray.direction;
        float scalar = sample_point(point, coarse_tet_id);

        if (!(scalar < optixLaunchParams.tf.range.x || scalar > optixLaunchParams.tf.range.y)) {

            const vec4f tfColor = getColor(scalar);

            const float sample = tfColor.w * optixLaunchParams.tf.opacity;
            if (sample >= rand() * max_sample) {
                color = tfColor;
                return true;
            }
        }

        // sample free-path distance
        t -= logf(1.f - rand()) / (max_sample);
        if (t >= tmax) {
            return false;
        }
    }
}

inline __device__ vec3f woodcock_coarse_no_macrocell(Ray &ray, PerRayData &prd, Random &rand) {
    vec4f color{optixLaunchParams.bgColor, 1.0f};

    while (true) {
        // each iteration of this loop processes 1 coarse element
        traceRay(optixLaunchParams.world, ray, prd,
                 OPTIX_RAY_FLAG_DISABLE_ANYHIT | OPTIX_RAY_FLAG_CULL_FRONT_FACING_TRIANGLES);
        if (prd.missed) {
            return optixLaunchParams.bgColor;
        }

        const float back_t = prd.hit_t;
        const unsigned long long coarse_id = prd.tet_id;

        traceRay(optixLaunchParams.world, ray, prd,
                 OPTIX_RAY_FLAG_DISABLE_ANYHIT | OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES);
        const float front_t = prd.hit_t;

        if (!prd.missed) {
            if (coarse_id == prd.tet_id) {
                // case 1
                ray.tmin = nextafterf(front_t, INFINITY);

                ray.tmin -= logf(1.f - rand()) / (optixLaunchParams.tf.opacity);
                if (ray.tmin >= back_t) {
                    continue;
                }
            }
        }

        if (woodcock_no_macrocell(ray, rand, color, ray.tmin, back_t, coarse_id)) {
            return vec3f{color};
        }
    }
}

inline __device__ vec3f march_ray(Ray &ray, PerRayData &prd) {
    vec4f color(0);
    float alpha(0);

    while (alpha < 0.99f) {
        traceRay(optixLaunchParams.world, ray, prd,
                 OPTIX_RAY_FLAG_DISABLE_ANYHIT | OPTIX_RAY_FLAG_CULL_FRONT_FACING_TRIANGLES);
        if (prd.missed) {
            break;
        }

        const float back_t = prd.hit_t;
        unsigned long long coarse_id = prd.tet_id;

        traceRay(optixLaunchParams.world, ray, prd,
                 OPTIX_RAY_FLAG_DISABLE_ANYHIT | OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES);
        const float front_t = prd.hit_t;

        if (!prd.missed) {
            if (coarse_id == prd.tet_id) {
                // case 1
                ray.tmin = nextafterf(front_t, INFINITY);
            }
        }

        vec3f v0, v1, v2, v3;
        float s0, s1, s2, s3;
        get_vertices(coarse_id, v0, v1, v2, v3, s0, s1, s2, s3);
        const float vol = det(v0, v1, v2, v3);
        while (ray.tmin < back_t) {
            float scalar = .0f;
            if (optixLaunchParams.dataRenderMode == 0) {
                const auto &cell_data = optixLaunchParams.tetData.cell_data;
                scalar = cell_data[coarse_id];
            } else {
                const vec3f point = ray.origin + ray.direction * ray.tmin;

                const float u = det(point, v1, v2, v3) / vol;
                const float v = det(v0, point, v2, v3) / vol;
                const float w = det(v0, v1, point, v3) / vol;
                const float x = 1.f - u - v - w;
                scalar = u * s0 + v * s1 + w * s2 + x * s3;
            }
            ray.tmin += optixLaunchParams.stepSize;

            if (scalar < optixLaunchParams.tf.range.x || scalar > optixLaunchParams.tf.range.y) {
                continue;
            }

            vec4f tfColor = getColor(scalar);
            tfColor.w *= optixLaunchParams.tf.opacity;
            tfColor.w = 1.f - powf(1.f - clamp(tfColor.w, 0.f, 1.f), optixLaunchParams.stepSize);
            color = color + (1.0f - alpha) * tfColor.w * tfColor;
            alpha = alpha + tfColor.w * (1.0f - alpha);
        }
    }

    return (1 - alpha) * optixLaunchParams.bgColor + vec3f(color);
}

inline __device__ vec4f convertRGBAtoVec4f(const uint32_t rgba) {
    constexpr float inv = 1.f / 255.f;
    return {static_cast<float>(rgba & 0xFF) * inv,         //
            static_cast<float>((rgba >> 8) & 0xFF) * inv,  //
            static_cast<float>((rgba >> 16) & 0xFF) * inv, //
            static_cast<float>((rgba >> 24) & 0xFF) * inv};
}

OPTIX_MISS_PROGRAM(miss)() {
    auto &prd = owl::getPRD<PerRayData>();
    prd.missed = true;
}

OPTIX_RAYGEN_PROGRAM(rayGen)() {
    const vec2i pixelID = getLaunchIndex();
    PerRayData prd;
    prd.missed = true;
    vec3f color = 0.f;

    Random rand(pixelID.x + optixLaunchParams.fbSize.x * optixLaunchParams.accumulateCount,
                pixelID.y + optixLaunchParams.fbSize.y * optixLaunchParams.accumulateCount);
    Ray ray;
    vec2f screen = vec2f(pixelID) / vec2f(optixLaunchParams.fbSize);
    screen.x += rand() / optixLaunchParams.fbSize.x;
    screen.y += rand() / optixLaunchParams.fbSize.y;
    const vec3f origin = optixLaunchParams.camera.org;
    const vec3f direction = optixLaunchParams.camera.llc + screen.u * optixLaunchParams.camera.horiz +
                            screen.v * optixLaunchParams.camera.vert;
    ray.origin = origin;
    ray.direction = normalize(direction);

    if (optixLaunchParams.renderMode == RayMarcher) {
        color = march_ray(ray, prd);
    } else if (optixLaunchParams.renderMode == Woodcock) {
        color = woodcock_coarse_no_macrocell(ray, prd, rand);
    }

    const int pixelIdx = pixelID.x + optixLaunchParams.fbSize.x * pixelID.y;
    if (optixLaunchParams.accumulateCount == 0) {
        optixLaunchParams.accumulationBuffer[pixelIdx] = 0.f;
    }

    optixLaunchParams.accumulationBuffer[pixelIdx] += vec4f{color, 1.0f};
    optixLaunchParams.fbPtr[pixelIdx] = make_rgba(optixLaunchParams.accumulationBuffer[pixelIdx] /
                                                  static_cast<float>(optixLaunchParams.accumulateCount + 1));
}
