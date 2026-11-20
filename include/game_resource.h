#pragma once

#include <filesystem>
#include <string>

// Paths rooted at :builtin: read packaged (RLE-compressed) assets; ordinary paths read files.
// Returns false if the resource cannot be opened.
bool readGameResource(const std::filesystem::path& path, std::string& out);
bool gameResourceExists(const std::filesystem::path& path);
