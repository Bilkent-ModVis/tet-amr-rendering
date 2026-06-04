#pragma once

#include <cmath>
#include <ostream>

class Vec3 {
  public:
    using Float = double;
    Float x, y, z;
    Vec3() : x(0), y(0), z(0) {};
    Vec3(const Float xx, const Float yy, const Float zz)
        : x(xx), y(yy), z(zz) {};

    [[nodiscard]] bool is_equal(const Vec3 &v,
                                const Float epsilon = 1e-6) const {
        return std::abs(x - v.x) < epsilon && std::abs(y - v.y) < epsilon &&
               std::abs(z - v.z) < epsilon;
    }

    [[nodiscard]] Float distance_to(const Vec3 &v) const {
        return std::sqrt(std::pow(x - v.x, 2) + std::pow(y - v.y, 2) +
                         std::pow(z - v.z, 2));
    }

    Vec3 operator+(const Vec3 &v) const { return {x + v.x, y + v.y, z + v.z}; }

    Vec3 operator-(const Vec3 &v) const { return {x - v.x, y - v.y, z - v.z}; }

    Vec3 operator/(const double c) const { return {x / c, y / c, z / c}; }

    Vec3 operator*(const double c) const { return {x * c, y * c, z * c}; }

    static Float dot(const Vec3 &u, const Vec3 &v) {
        return u.x * v.x + u.y * v.y + u.z * v.z;
    }

    static Vec3 cross(const Vec3 &u, const Vec3 &v) {
        return {u.y * v.z - u.z * v.y, //
                u.z * v.x - u.x * v.z, //
                u.x * v.y - u.y * v.x};
    }

    Float operator[](const int dim) const {
        if (dim == 0)
            return x;
        if (dim == 1)
            return y;
        return z;
    }

    friend std::ostream &operator<<(std::ostream &os, const Vec3 &v) {
        return os << "x: " << v.x << ", y: " << v.y << ", z: " << v.z;
    }

    bool operator<(const Vec3 &other) const {
        if (x != other.x)
            return x < other.x;
        if (y != other.y)
            return y < other.y;
        return z < other.z;
    }
};
