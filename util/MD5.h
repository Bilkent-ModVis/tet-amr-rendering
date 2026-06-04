#include <cstdint>
#include <string>

class MD5 {
  public:
    static std::string hash_bytes(const char *bytes, const uint64_t n_bytes);
    static std::string hash_file(const std::string &filename);
};
