#include "game_resource.h"
#include "embedded_assets.h"

#include <cstdio>
#include <string_view>

namespace {
const std::filesystem::path builtinRoot(":builtin:");

bool isBuiltin(const std::filesystem::path& path) {
    return !path.empty() && *path.begin() == builtinRoot;
}

std::string_view lookupBuiltin(const std::filesystem::path& path) {
    return embeddedAsset(path.lexically_relative(builtinRoot).lexically_normal().generic_string());
}

void decodeRle(std::string_view src, std::string& out) {
    out.clear();
    out.reserve(src.size() * 2);
    for (std::size_t i = 0; i < src.size(); ) {
        const unsigned char b = static_cast<unsigned char>(src[i]);
        if (b == 0xFFu && i + 2 < src.size()) {
            const std::size_t count = static_cast<unsigned char>(src[i + 1]);
            out.append(count, src[i + 2]);
            i += 3;
        } else {
            out.push_back(src[i]);
            ++i;
        }
    }
}
} // namespace

bool readGameResource(const std::filesystem::path& path, std::string& out) {
    if (isBuiltin(path)) {
        const auto bytes = lookupBuiltin(path);
        if (bytes.data() == nullptr) return false;
        decodeRle(bytes, out);
        return true;
    }
#ifdef _WIN32
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return false;
#else
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
#endif
    out.clear();
    char buf[8192];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

bool gameResourceExists(const std::filesystem::path& path) {
    if (isBuiltin(path)) return lookupBuiltin(path).data() != nullptr;
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}
