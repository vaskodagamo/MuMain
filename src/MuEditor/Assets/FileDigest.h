#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <filesystem>
#include <string>

// Digests of files: the SHA-256 regeneration requests record (current_sha256),
// and the id git gives a file's content.
namespace Editor::Files
{
// Lower-case hex SHA-256 of the file's bytes, or an empty string when the file
// cannot be read.
std::string Sha256Hex(const std::filesystem::path& file);

// The file's git blob id (what `git hash-object <file>` prints: the SHA-1 of
// "blob <size>" and a NUL byte followed by the bytes), or an empty string when the
// file cannot be read.
std::string GitBlobIdHex(const std::filesystem::path& file);

// Lower-case hex of `count` bytes.
std::string ToHex(const unsigned char* bytes, std::size_t count);
} // namespace Editor::Files

#endif // _EDITOR
