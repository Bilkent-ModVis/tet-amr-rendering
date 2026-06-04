#include <filesystem>

class FileCache {
  public:
    std::filesystem::path cache_path;

    FileCache() = delete;
    FileCache(const std::filesystem::path &cache_dir_name) {
        cache_path = std::filesystem::current_path() / cache_dir_name;
    }
    bool cache_exists() { return std::filesystem::exists(cache_path); }

    bool create_cache() { return std::filesystem::create_directory(cache_path); }

    bool has_file(const std::string &filename) { return std::filesystem::exists(cache_path / filename); }
};
