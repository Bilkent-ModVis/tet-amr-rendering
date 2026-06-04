#include "MD5.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

// https://dl.acm.org/doi/pdf/10.17487/RFC1321
// T[i] = 4294967296 * std::abs(std::sin(i + 1));
constexpr uint32_t T[] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

inline void process_block(const char *buffer, const uint32_t buffer_size,
                          uint32_t &A, uint32_t &B, uint32_t &C, uint32_t &D) {
    for (uint32_t block = 0; block < buffer_size / 64; ++block) {
        const uint32_t *const X = (uint32_t *)buffer + 16 * block;

        uint32_t AA = A;
        uint32_t BB = B;
        uint32_t CC = C;
        uint32_t DD = D;

        constexpr uint32_t shift_0[] = {7, 12, 17, 22};
        for (int i = 0; i < 16; ++i) {
            uint32_t tmp = A + ((B & C) | ((~B) & D)) + X[i] + T[i];
            A = D;
            D = C;
            C = B;
            B += std::rotl(tmp, shift_0[i % 4]);
        }

        constexpr uint32_t shift_1[] = {5, 9, 14, 20};
        for (int i = 16; i < 32; ++i) {
            uint32_t tmp =
                A + ((B & D) | (C & (~D))) + X[(5 * i + 1) % 16] + T[i];
            A = D;
            D = C;
            C = B;
            B += std::rotl(tmp, shift_1[i % 4]);
        }
        constexpr uint32_t shift_2[] = {4, 11, 16, 23};
        for (int i = 32; i < 48; ++i) {
            uint32_t tmp = A + (B ^ C ^ D) + X[(3 * i + 5) % 16] + T[i];
            A = D;
            D = C;
            C = B;
            B += std::rotl(tmp, shift_2[i % 4]);
        }
        constexpr uint32_t shift_3[] = {6, 10, 15, 21};
        for (int i = 48; i < 64; ++i) {
            uint32_t tmp = A + (C ^ (B | ~D)) + X[(7 * i) % 16] + T[i];
            A = D;
            D = C;
            C = B;
            B += std::rotl(tmp, shift_3[i % 4]);
        }

        A += AA;
        B += BB;
        C += CC;
        D += DD;
    }
}

std::string MD5::hash_file(const std::string &filename) {
    uint64_t file_size{std::filesystem::file_size(filename)};
    std::ifstream ifstream(filename, std::ios::binary | std::ios::in);

    // the smallest block we can process is 512 bits = 64 bytes
    // block_size needs to be a multiple of that
    constexpr uint32_t read_block_size = 1 << 20; // 1 MB
    alignas(64) std::vector<uint8_t> buffer(read_block_size);

    uint32_t A = 0x67452301;
    uint32_t B = 0xefcdab89;
    uint32_t C = 0x98badcfe;
    uint32_t D = 0x10325476;

    const uint64_t read_block_count = file_size / read_block_size;
    for (uint64_t read_block = 0; read_block < read_block_count; ++read_block) {
        ifstream.read((char *)buffer.data(), read_block_size);
        process_block((char *)buffer.data(), read_block_size, A, B, C, D);
    }

    // process the remaining bytes + padding
    const std::streamsize read_bytes =
        ifstream.readsome((char *)buffer.data(), read_block_size);
    ifstream.close();

    auto pad = (960 - (8 * file_size) % 512) % 512;
    pad = pad == 0 ? 512 : pad;
    const auto pad_bytes = pad / 8;

    buffer.resize(read_bytes + pad_bytes + 8, 0);
    std::fill(buffer.begin() + read_bytes, buffer.end(), 0);
    buffer[read_bytes] = 0x80; // set first bit of padding to 1
    uint64_t *const size =
        reinterpret_cast<uint64_t *>(buffer.data() + read_bytes + pad_bytes);
    *size = file_size * 8;

    process_block((char *)buffer.data(), buffer.size(), A, B, C, D);

    std::string result;
    for (uint32_t x : {A, B, C, D}) {
        for (int i = 0; i < 4; ++i) {
            result += std::format("{:02x}", x & 0xFF);
            x >>= 8;
        }
    }

    return result;
}

std::string MD5::hash_bytes(const char *bytes, const uint64_t n_bytes) {
    auto pad = (960 - (8 * n_bytes) % 512) % 512;
    pad = pad == 0 ? 512 : pad;
    const auto pad_bytes = pad / 8;

    uint32_t A = 0x67452301;
    uint32_t B = 0xefcdab89;
    uint32_t C = 0x98badcfe;
    uint32_t D = 0x10325476;

    constexpr uint32_t block_size = 1 << 8; // 1 KB
    const uint64_t block_count = n_bytes / block_size;
    for (uint64_t block = 0; block < block_count; ++block) {
        process_block(bytes + block * block_size, block_size, A, B, C, D);
    }

    // process the remaining bytes + padding
    uint32_t remanining_bytes = n_bytes % block_size;
    std::vector<uint8_t> pad_buffer(remanining_bytes + pad_bytes + 8, 0);
    std::copy(bytes + block_count * block_size, bytes + n_bytes,
              pad_buffer.begin());

    pad_buffer[remanining_bytes] = 0x80; // set first bit of padding to 1
    uint64_t *const size = reinterpret_cast<uint64_t *>(
        pad_buffer.data() + remanining_bytes + pad_bytes);
    *size = n_bytes * 8;

    process_block((char *)pad_buffer.data(), pad_buffer.size(), A, B, C, D);

    std::string result;
    for (uint32_t x : {A, B, C, D}) {
        for (int i = 0; i < 4; ++i) {
            result += std::format("{:02x}", x & 0xFF);
            x >>= 8;
        }
    }

    return result;
}
