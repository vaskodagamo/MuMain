#include "FileDigest.h"

#ifdef _EDITOR

#include <openssl/evp.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace Editor::Files
{
namespace
{
constexpr std::size_t READ_CHUNK_BYTES = 64 * 1024;
constexpr const char* HEX_DIGITS = "0123456789abcdef";
constexpr int HIGH_NIBBLE_SHIFT = 4;
constexpr unsigned LOW_NIBBLE_MASK = 0x0F;
// Git names a file's content by the SHA-1 of this, its size in decimal, a NUL
// byte and the bytes.
constexpr const char* GIT_BLOB_HEADER = "blob ";

struct DigestContextFree
{
    void operator()(EVP_MD_CTX* context) const
    {
        EVP_MD_CTX_free(context);
    }
};
using DigestContext = std::unique_ptr<EVP_MD_CTX, DigestContextFree>;

// Lower-case hex digest of `prefix` followed by the file's bytes; empty when the
// file cannot be read.
std::string DigestHex(const std::filesystem::path& file, const EVP_MD* algorithm, const std::string& prefix)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return {};

    DigestContext context(EVP_MD_CTX_new());
    if (!context || EVP_DigestInit_ex(context.get(), algorithm, nullptr) != 1)
        return {};
    if (!prefix.empty() && EVP_DigestUpdate(context.get(), prefix.data(), prefix.size()) != 1)
        return {};

    std::vector<char> chunk(READ_CHUNK_BYTES);
    while (stream)
    {
        stream.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const std::streamsize got = stream.gcount();
        if (got > 0 && EVP_DigestUpdate(context.get(), chunk.data(), static_cast<std::size_t>(got)) != 1)
            return {};
    }
    if (stream.bad())
        return {};

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestBytes = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digestBytes) != 1)
        return {};
    return ToHex(digest.data(), digestBytes);
}
} // namespace

std::string ToHex(const unsigned char* bytes, std::size_t count)
{
    std::string text;
    text.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i)
    {
        text.push_back(HEX_DIGITS[bytes[i] >> HIGH_NIBBLE_SHIFT]);
        text.push_back(HEX_DIGITS[bytes[i] & LOW_NIBBLE_MASK]);
    }
    return text;
}

std::string Sha256Hex(const std::filesystem::path& file)
{
    return DigestHex(file, EVP_sha256(), {});
}

std::string GitBlobIdHex(const std::filesystem::path& file)
{
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec)
        return {};
    std::string header = GIT_BLOB_HEADER + std::to_string(size);
    header.push_back('\0');
    return DigestHex(file, EVP_sha1(), header);
}
} // namespace Editor::Files

#endif // _EDITOR
