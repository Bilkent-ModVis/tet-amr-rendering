#ifdef __JETBRAINS_IDE__
#define __CUDA_ARCH__
#endif

#include "device_code.h"
#include "render_mode_enums.h"
#include <optix_device.h>

#include <owl/common/math/random.h>
using Random = owl::LCG<6>;

#ifndef MACROCELL
#define MACROCELL 0
#endif

#ifndef traverse
// #define traverse traverse_point_plane
#define traverse traverse_barycentric
#endif

extern "C" __constant__ LaunchParams optixLaunchParams;

inline __device__ bool half_space(const vec3f &point, const vec3f &v0, const vec3f &v1, const vec3f &v2) {
    return dot(point - v0, cross(v1 - v0, v2 - v0)) > 0;
}

// positive if bcd points to a
inline __device__ float det(const vec3f &a, const vec3f &b, const vec3f &c, const vec3f &d) {
    const vec3f ab = a - b;
    const vec3f cb = c - b;
    const vec3f db = d - b;
    return dot(cross(cb, db), ab);
}

inline __device__ float cross2d(float ax, float ay, float bx, float by) { return __fmaf_rn(ax, by, -ay * bx); }

inline __device__ void project_uv(const vec3f &P, float &u, float &v) {
    const auto cam = optixLaunchParams.camera;
    const vec3f d = P - cam.org;
    const float denom = dot(d, cam.n);
    const float t = cam.dot_llc_n * __frcp_rn(denom);
    vec3f temp;
    temp.x = fmaf(t, d.x, -cam.llc.x);
    temp.y = fmaf(t, d.y, -cam.llc.y);
    temp.z = fmaf(t, d.z, -cam.llc.z);
    u = dot(temp, cam.horiz_inv_horiz2_fbsize);
    v = dot(temp, cam.vert_inv_vert2_fbsize);
}

inline __device__ float proj_tet_area(const vec3f &A, const vec3f &B, const vec3f &C, const vec3f &D) {
    float Ah, Av;
    project_uv(A, Ah, Av);
    float Bh, Bv;
    project_uv(B, Bh, Bv);
    float Ch, Cv;
    project_uv(C, Ch, Cv);
    float Dh, Dv;
    project_uv(D, Dh, Dv);

    const float BAh = Bh - Ah;
    const float BAv = Bv - Av;
    const float CAh = Ch - Ah;
    const float CAv = Cv - Av;
    const float DAh = Dh - Ah;
    const float DAv = Dv - Av;
    const float CBh = Ch - Bh;
    const float CBv = Cv - Bv;
    const float DBh = Dh - Bh;
    const float DBv = Dv - Bv;

    float area = fabsf(cross2d(BAh, BAv, CAh, CAv));
    area += fabsf(cross2d(BAh, BAv, DAh, DAv));
    area += fabsf(cross2d(CAh, CAv, DAh, DAv));
    area += fabsf(cross2d(CBh, CBv, DBh, DBv));

    // tet area is 1/2 of the total areas of the triangles
    // each triangles area is 1/2 of cross2d result
    // we divide by 4
    return 0.25f * area;
}

inline __device__ float sample_cell_data(const vec3f &point, const vec3f &t, const vec3f &l, const vec3f &r,
                                         const vec3f &b, const unsigned long long tet_id,
                                         const unsigned long long coarse_tet_root_id) {
    const auto &cell_data = optixLaunchParams.tetData.cell_data;

    float sample = cell_data[tet_id];
    float sample_count = 1;

    for (int i = 0; i < 4; ++i) {
        auto neighbor_id = optixLaunchParams.tetData.neighbors[4 * tet_id + i];
        if (neighbor_id == (0ULL - 1)) {
            continue;
        }
        neighbor_id &= 0x007FFFFFFFFFFFFFULL;
        sample += cell_data[neighbor_id];
        ++sample_count;
    }

    return sample / sample_count;
}

inline __device__ float sample_vertex_data(const vec3f &point, const vec3f &t, const vec3f &l, const vec3f &r,
                                           const vec3f &b, const unsigned long long tet_id) {
    const float vol = det(t, l, r, b);
    const float u = det(point, l, r, b) / vol;
    const float v = det(point, t, b, r) / vol;
    const float w = det(point, t, l, b) / vol;
    const float x = 1.f - u - v - w;

    const auto &vertex_data = optixLaunchParams.tetData.vertex_data;
    return u * vertex_data[4 * tet_id] + v * vertex_data[4 * tet_id + 1] + w * vertex_data[4 * tet_id + 2] +
           x * vertex_data[4 * tet_id + 3];
}

inline __device__ float calculate_exit_t(const float ray_t, const vec3f &point, const vec3f &dir, const vec3f &t,
                                         const vec3f &l, const vec3f &r, const vec3f &b) {
    float plane_t = 10000000.f;
    {
        vec3f N = cross(t - l, r - l);
        float NdotD = dot(dir, N);
        if (NdotD != 0.f) {
            float hit_t = dot(l - point, N) / NdotD;
            if (hit_t > .0f) {
                plane_t = min(plane_t, hit_t);
            }
        }
    }
    {
        vec3f N = cross(t - r, b - r);
        float NdotD = dot(dir, N);
        if (NdotD != 0.f) {
            float hit_t = dot(r - point, N) / NdotD;
            if (hit_t > .0f) {
                plane_t = min(plane_t, hit_t);
            }
        }
    }

    {
        vec3f N = cross(t - b, l - b);
        float NdotD = dot(dir, N);
        if (NdotD != 0.f) {
            float hit_t = dot(b - point, N) / NdotD;
            if (hit_t > .0f) {
                plane_t = min(plane_t, hit_t);
            }
        }
    }

    {
        vec3f N = cross(l - b, r - b);
        float NdotD = dot(dir, N);
        if (NdotD != 0.f) {
            float hit_t = dot(b - point, N) / NdotD;
            if (hit_t > .0f) {
                plane_t = min(plane_t, hit_t);
            }
        }
    }

    if (plane_t == 10000000.f) {
        plane_t = nextafterf(ray_t, INFINITY);
    } else {
        plane_t += ray_t;
    }

    return plane_t;
}

inline __device__ void traverse_point_plane(const vec3f &point, const unsigned long long coarse_tet_id,
                                            unsigned long long &sample_tet_id, vec3f &t, vec3f &l, vec3f &r, vec3f &b) {
    const auto &vertices = optixLaunchParams.tetData.vertex;
    const auto &indices = optixLaunchParams.tetData.index;
    const auto &child_nums = optixLaunchParams.tetData.child_nums;
    const auto &tree_root_ids = optixLaunchParams.tetData.tree_root_ids;

    unsigned long long tet_id = tree_root_ids[coarse_tet_id];

    const int i0 = indices[4 * coarse_tet_id][0];
    const int i1 = indices[4 * coarse_tet_id][1];
    const int i2 = indices[4 * coarse_tet_id][2];
    const int i3 = indices[4 * coarse_tet_id + 1][2];
    t = vertices[i0];
    l = vertices[i1];
    r = vertices[i2];
    b = vertices[i3];

    int r_level = 0;
    vec3f v0 = t, v1 = l, v2 = r, v3 = b;
    bool r0, r1;

    bool aboveScreenSpaceThreshold = true;
    const bool useRefinementCriteria = optixLaunchParams.refinementCriteria > 0.0001f;

    if (useRefinementCriteria) {
        aboveScreenSpaceThreshold = proj_tet_area(v0, v1, v2, v3) > optixLaunchParams.refinementCriteria;
    }

    while (child_nums[tet_id] > 0 && aboveScreenSpaceThreshold) {
        // points towards l & r
        const vec3f tl = (t + l) / 2;
        const vec3f lb = (l + b) / 2;
        const vec3f tr = (t + r) / 2;
        const vec3f tb = (t + b) / 2;

        r0 = half_space(point, tl, lb, tr);
        r1 = half_space(point, tr, tb, lb);
        int tet;
        if (!r0 && r1) {
            // points towards the top
            if (half_space(point, tl, tr, tb)) {
                tet = 0;
                v0 = t;
                v1 = tl;
                v2 = tr;
                v3 = tb;
            } else {
                tet = 1;
                v0 = lb;
                v1 = tb;
                v2 = tr;
                v3 = tl;
            }
        } else if (r0 && r1) {
            // Left quadrant
            const vec3f lr = (l + r) / 2;
            if (half_space(point, lr, tl, lb)) {
                tet = 2;
                v0 = l;
                v1 = tl;
                v2 = lb;
                v3 = lr;
            } else {
                tet = 3;
                v0 = tr;
                v1 = lr;
                v2 = lb;
                v3 = tl;
            }
        } else if (!r0 && !r1) {
            // Back quadrant
            const vec3f rb = (r + b) / 2;
            if (half_space(point, lb, tb, rb)) {
                tet = 6;
                v0 = b;
                v1 = rb;
                v2 = lb;
                v3 = tb;
            } else {
                tet = 7;
                v0 = tr;
                v1 = tb;
                v2 = lb;
                v3 = rb;
            }
        } else {
            // Right quadrant
            const vec3f lr = (l + r) / 2;
            const vec3f rb = (r + b) / 2;
            if (half_space(point, tr, lr, rb)) {
                tet = 4;
                v0 = r;
                v1 = rb;
                v2 = tr;
                v3 = lr;
            } else {
                tet = 5;
                v0 = lb;
                v1 = lr;
                v2 = tr;
                v3 = rb;
            }
        }

        t = v0;
        l = v1;
        r = v2;
        b = v3;

        if (useRefinementCriteria) {
            aboveScreenSpaceThreshold = proj_tet_area(v0, v1, v2, v3) > optixLaunchParams.refinementCriteria;
        }

        tet_id += 1;
        for (int i = 0; i < tet; i++) {
            tet_id += child_nums[tet_id] + 1;
        }

        ++r_level;
    }

    sample_tet_id = tet_id;
}

inline __device__ void traverse_barycentric(const vec3f &point, const unsigned long long coarse_tet_id,
                                            unsigned long long &sample_tet_id, vec3f &t, vec3f &l, vec3f &r, vec3f &b) {
    const auto &vertices = optixLaunchParams.tetData.vertex;
    const auto &indices = optixLaunchParams.tetData.index;
    const auto &child_nums = optixLaunchParams.tetData.child_nums;
    const auto &tree_root_ids = optixLaunchParams.tetData.tree_root_ids;

    unsigned long long tet_id = tree_root_ids[coarse_tet_id];

    const int i0 = indices[4 * coarse_tet_id][0];
    const int i1 = indices[4 * coarse_tet_id][1];
    const int i2 = indices[4 * coarse_tet_id][2];
    const int i3 = indices[4 * coarse_tet_id + 1][2];
    t = vertices[i0];
    l = vertices[i1];
    r = vertices[i2];
    b = vertices[i3];

    int r_level = 0;
    vec3f v0 = t, v1 = l, v2 = r, v3 = b;

    bool aboveScreenSpaceThreshold = true;
    const bool useRefinementCriteria = optixLaunchParams.refinementCriteria > 0.0001f;

    if (useRefinementCriteria) {
        aboveScreenSpaceThreshold = proj_tet_area(v0, v1, v2, v3) > optixLaunchParams.refinementCriteria;
    }

    float h_vol = det(t, l, r, b) / 2;

    while (child_nums[tet_id] > 0 && aboveScreenSpaceThreshold) {
        float u = det(point, l, r, b);
        float v = det(point, t, b, r);
        float w = det(point, t, l, b);
        float x = 2.f * h_vol - u - v - w;

        // equivalent to checking if u+v < 1/2, if we had divided them by volume
        bool r0 = u + v < h_vol;
        bool r1 = v + w < h_vol;
        int tet = 0;

        if (!r0 && r1) {
            // points towards the top
            if (u > h_vol) {
                tet = 0;
                v0 = t;
                v1 = (t + l) / 2;
                v2 = (t + r) / 2;
                v3 = (t + b) / 2;
            } else {
                tet = 1;
                v0 = (l + b) / 2;
                v1 = (t + b) / 2;
                v2 = (t + r) / 2;
                v3 = (t + l) / 2;
            }
        } else if (!r0 && !r1) {
            // Left quadrant
            if (v > h_vol) {
                tet = 2;
                v0 = l;
                v1 = (t + l) / 2;
                v2 = (l + b) / 2;
                v3 = (l + r) / 2;
            } else {
                tet = 3;
                v0 = (t + r) / 2;
                v1 = (l + r) / 2;
                v2 = (l + b) / 2;
                v3 = (t + l) / 2;
            }
        } else if (r0 && r1) {
            // Back quadrant
            if (x > h_vol) {
                tet = 6;
                v0 = b;
                v1 = (r + b) / 2;
                v2 = (l + b) / 2;
                v3 = (t + b) / 2;
            } else {
                tet = 7;
                v0 = (t + r) / 2;
                v1 = (t + b) / 2;
                v2 = (l + b) / 2;
                v3 = (r + b) / 2;
            }
        } else {
            // Right quadrant
            if (w > h_vol) {
                tet = 4;
                v0 = r;
                v1 = (r + b) / 2;
                v2 = (t + r) / 2;
                v3 = (l + r) / 2;
            } else {
                tet = 5;
                v0 = (l + b) / 2;
                v1 = (l + r) / 2;
                v2 = (t + r) / 2;
                v3 = (r + b) / 2;
            }
        }

        t = v0;
        l = v1;
        r = v2;
        b = v3;

        h_vol /= 8.f;

        if (useRefinementCriteria) {
            aboveScreenSpaceThreshold = proj_tet_area(v0, v1, v2, v3) > optixLaunchParams.refinementCriteria;
        }

        tet_id += 1;
        for (int i = 0; i < tet; i++) {
            tet_id += child_nums[tet_id] + 1;
        }

        ++r_level;
    }

    sample_tet_id = tet_id;
}

__device__ inline vec4f getColor(const float &scalar) {
    const vec2f range = optixLaunchParams.tf.range;
    const float color_t = (scalar - range.x) / (range.y - range.x);
    auto xf = tex2D<float4>(optixLaunchParams.tf.tfTex, color_t, 0.5f);
    return xf;
}

__device__ inline bool woodcock(const Ray &ray, Random &rand, vec4f &color, float &t, const float tmax,
                                const unsigned int coarse_tet_id) {
    float max_sample = optixLaunchParams.tf.opacity * (optixLaunchParams.dataRenderMode == 0
                                                           ? optixLaunchParams.tetData.cell_density[coarse_tet_id].y
                                                           : optixLaunchParams.tetData.vertex_density[coarse_tet_id].y);

    if (max_sample <= 0.f)
        return false;

    float exit_t = nextafterf(t, -INFINITY);

    unsigned long long sample_tet_id = 0;
    vec3f sample_t, sample_l, sample_r, sample_b;
    while (true) {
        // sample free-path distance
        t -= logf(1.f - rand()) / (max_sample);
        if (t >= tmax) {
            return false;
        }

        vec3f sample_point = ray.origin + t * ray.direction;
        if (t > exit_t) {
            // recalculate the tet
            traverse(sample_point, coarse_tet_id, sample_tet_id, sample_t, sample_l, sample_r, sample_b);
            exit_t = calculate_exit_t(t, sample_point, ray.direction, sample_t, sample_l, sample_r, sample_b);
        }

        const float scalar =
            optixLaunchParams.dataRenderMode == 0
                ? sample_cell_data(sample_point, sample_t, sample_l, sample_r, sample_b, sample_tet_id, coarse_tet_id)
                : sample_vertex_data(sample_point, sample_t, sample_l, sample_r, sample_b, sample_tet_id);

        if (scalar < optixLaunchParams.tf.range.x || scalar > optixLaunchParams.tf.range.y) {
            continue;
        }
        const vec4f tfColor = getColor(scalar);

        const float sample = tfColor.w * optixLaunchParams.tf.opacity;
        if (sample >= rand() * max_sample) {
            color = tfColor;
            return true;
        }
    }
}

inline __device__ vec3f woodcock_coarse(Ray &ray, PerRayData &prd, Random &rand) {
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

        float next_face_t = .0f;
        if (!prd.missed) {
            if (coarse_id == prd.tet_id) {
                // case 1
                ray.tmin = nextafterf(front_t, INFINITY);
                next_face_t = nextafterf(back_t, INFINITY);
            } else {
                // case 2
                next_face_t = nextafterf(front_t, INFINITY);
            }
        } else {
            // case 3
            next_face_t = nextafterf(back_t, INFINITY);
        }

        const bool accept = woodcock(ray, rand, color, ray.tmin, back_t, coarse_id);
        if (accept) {
            return vec3f{color};
        }
        ray.tmin = next_face_t;
    }
}

__device__ inline bool woodcock_no_macrocell(const Ray &ray, Random &rand, vec4f &color, float &t, const float tmax,
                                             const unsigned int coarse_tet_id) {
    float max_sample = optixLaunchParams.tf.opacity;

    if (max_sample <= 0.f)
        return false;

    float exit_t = nextafterf(t, -INFINITY);

    unsigned long long sample_tet_id = 0;
    vec3f sample_t, sample_l, sample_r, sample_b;
    while (true) {
        // sample free-path distance
        vec3f sample_point = ray.origin + t * ray.direction;
        if (t > exit_t) {
            // recalculate the tet
            traverse(sample_point, coarse_tet_id, sample_tet_id, sample_t, sample_l, sample_r, sample_b);
            exit_t = calculate_exit_t(t, sample_point, ray.direction, sample_t, sample_l, sample_r, sample_b);
        }

        const float scalar =
            optixLaunchParams.dataRenderMode == 0
                ? sample_cell_data(sample_point, sample_t, sample_l, sample_r, sample_b, sample_tet_id, coarse_tet_id)
                : sample_vertex_data(sample_point, sample_t, sample_l, sample_r, sample_b, sample_tet_id);

        if (!(scalar < optixLaunchParams.tf.range.x || scalar > optixLaunchParams.tf.range.y)) {
            const vec4f tfColor = getColor(scalar);

            const float sample = tfColor.w * optixLaunchParams.tf.opacity;
            if (sample >= rand() * max_sample) {
                color = tfColor;
                return true;
            }
        }

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

#if MACROCELL
        float next_face_t = .0f;

        if (!prd.missed) {
            if (coarse_id == prd.tet_id) {
                // case 1
                ray.tmin = nextafterf(front_t, INFINITY);
                next_face_t = nextafterf(back_t, INFINITY);
            } else {
                // case 2
                next_face_t = nextafterf(front_t, INFINITY);
            }
        } else {
            // case 3
            next_face_t = nextafterf(back_t, INFINITY);
        }
#else
        if (!prd.missed) {
            if (coarse_id == prd.tet_id) {
                // case 1
                ray.tmin = nextafterf(front_t, INFINITY);
            }
        }
#endif

#if MACROCELL
        const bool skip_element = optixLaunchParams.tetData.vertex_density[coarse_id].y <= .001f;
#endif

#if MACROCELL
        while (ray.tmin < back_t && !skip_element) {
#else
        while (ray.tmin < back_t) {
#endif
            float exit_t = .0f;
            unsigned long long sample_tet_id;
            vec3f t, l, r, b;
            vec3f point = ray.origin + ray.direction * ray.tmin;
            traverse(point, coarse_id, sample_tet_id, t, l, r, b);
            exit_t = calculate_exit_t(ray.tmin, point, ray.direction, t, l, r, b);
            exit_t = min(back_t, exit_t);

            do {
                const float scalar = optixLaunchParams.dataRenderMode == 0
                                         ? sample_cell_data(point, t, l, r, b, sample_tet_id, coarse_id)
                                         : sample_vertex_data(point, t, l, r, b, sample_tet_id);
                ray.tmin += optixLaunchParams.stepSize;
                point = ray.origin + ray.direction * ray.tmin;
                if (scalar < optixLaunchParams.tf.range.x || scalar > optixLaunchParams.tf.range.y) {
                    continue;
                }

                vec4f tfColor = getColor(scalar);
                tfColor.w *= optixLaunchParams.tf.opacity;
                tfColor.w = 1.f - powf(1.f - clamp(tfColor.w, 0.f, 1.f), optixLaunchParams.stepSize);
                color = color + (1.0f - alpha) * tfColor.w * tfColor;
                alpha = alpha + tfColor.w * (1.0f - alpha);
            } while (ray.tmin < exit_t);
        }
#if MACROCELL
        // empty space skipping
        if (skip_element) {
            const float t_next =
                floorf(fmaxf(next_face_t + optixLaunchParams.stepSize - ray.tmin, 0.f) / optixLaunchParams.stepSize) *
                    optixLaunchParams.stepSize +
                ray.tmin;
            ray.tmin = t_next;
        }
#endif
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

OPTIX_CLOSEST_HIT_PROGRAM(Tet)() {
    const unsigned int primID = optixGetPrimitiveIndex();
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
#if MACROCELL
        color = woodcock_coarse(ray, prd, rand);
#else
        color = woodcock_coarse_no_macrocell(ray, prd, rand);
#endif
    }

    const int pixelIdx = pixelID.x + optixLaunchParams.fbSize.x * pixelID.y;

    if (optixLaunchParams.accumulateCount == 0) {
        optixLaunchParams.accumulationBuffer[pixelIdx] = 0.f;
    }

    optixLaunchParams.accumulationBuffer[pixelIdx] += vec4f{color, 1.0f};
    optixLaunchParams.fbPtr[pixelIdx] = make_rgba(optixLaunchParams.accumulationBuffer[pixelIdx] /
                                                  static_cast<float>(optixLaunchParams.accumulateCount + 1));
}
