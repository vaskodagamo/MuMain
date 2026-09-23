#include "GitCheckout.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "FileDigest.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace Editor::Git
{
namespace
{
constexpr const char* GIT_ENTRY = ".git";
constexpr const char* HEAD_FILE = "HEAD";
constexpr const char* PACKED_REFS_FILE = "packed-refs";
constexpr const char* COMMON_DIR_FILE = "commondir"; // a worktree's pointer to the shared git folder
// Workers fetch base_commit from origin (the fork), never from another remote.
constexpr const char* ORIGIN_REMOTES_FOLDER = "refs/remotes/origin";
constexpr std::string_view GITDIR_PREFIX = "gitdir:";
constexpr std::string_view SYMBOLIC_REF_PREFIX = "ref:";
constexpr std::string_view BRANCH_PREFIX = "refs/heads/";
constexpr std::string_view ORIGIN_REMOTE_PREFIX = "refs/remotes/origin/";
constexpr char PACKED_COMMENT = '#';
constexpr char PACKED_PEELED = '^';
constexpr std::size_t COMMIT_HEX_CHARS = 40;
// HEAD -> branch is one step; a longer chain of symbolic refs is a broken checkout.
constexpr int MAX_SYMBOLIC_DEPTH = 5;

// The index (staging area), versions 2 and 3: a 12-byte header ("DIRC", version,
// entry count), then per entry 40 bytes of file stat data, the 20-byte SHA-1 blob
// id, 16 bits of flags, 2 more flag bytes in version 3 when the extended flag is
// set, the path, and 1 to 8 NUL bytes that pad the entry to a multiple of 8.
constexpr const char* INDEX_FILE = "index";
constexpr std::string_view INDEX_SIGNATURE = "DIRC";
constexpr std::uint32_t FIRST_INDEX_VERSION = 2;
constexpr std::uint32_t LAST_INDEX_VERSION = 3;
constexpr std::size_t INDEX_VERSION_OFFSET = 4;
constexpr std::size_t INDEX_COUNT_OFFSET = 8;
constexpr std::size_t INDEX_HEADER_BYTES = 12;
constexpr std::size_t ENTRY_BLOB_ID_OFFSET = 40;
constexpr std::size_t BLOB_ID_BYTES = 20;
constexpr std::size_t ENTRY_FLAGS_OFFSET = 60;
constexpr std::size_t ENTRY_PATH_OFFSET = 62;
constexpr std::size_t EXTENDED_FLAGS_BYTES = 2;
constexpr std::size_t ENTRY_ALIGNMENT = 8;
constexpr std::uint32_t FLAG_EXTENDED = 0x4000;
constexpr std::uint32_t FLAG_STAGE_MASK = 0x3000;       // non-zero: one side of a merge conflict
constexpr std::uint32_t FLAG_PATH_LENGTH_MASK = 0x0FFF; // this value itself: a longer, NUL-ended path
constexpr int BITS_PER_BYTE = 8;

// The per-worktree git folder (HEAD) and the shared one (refs, packed-refs).
struct GitDirs
{
    fs::path gitDir;
    fs::path commonDir;
};

std::string_view Trim(std::string_view text)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && isSpace(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && isSpace(text.back()))
        text.remove_suffix(1);
    return text;
}

std::optional<std::string> ReadFirstLine(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    std::string line;
    if (!stream || !std::getline(stream, line))
        return std::nullopt;
    return std::string(Trim(line));
}

bool IsCommitHex(std::string_view text)
{
    if (text.size() != COMMIT_HEX_CHARS)
        return false;
    return std::all_of(text.begin(), text.end(),
                       [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

// `value` from a git file, relative to `base` unless it is absolute.
fs::path ResolveFrom(const fs::path& base, std::string_view value)
{
    const fs::path path = Editor::Text::Utf8Path(value);
    return path.is_absolute() ? path : (base / path).lexically_normal();
}

std::vector<unsigned char> ReadAllBytes(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::uint32_t ReadBigEndian(const std::vector<unsigned char>& bytes, std::size_t offset, std::size_t count)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < count; ++i)
        value = (value << BITS_PER_BYTE) | bytes[offset + i];
    return value;
}

// Where the path of an index entry ends; nullopt when it runs past the file.
std::optional<std::size_t> EntryPathEnd(const std::vector<unsigned char>& index, std::size_t pathStart,
                                        std::uint32_t flags)
{
    if (pathStart > index.size())
        return std::nullopt;
    const std::size_t length = flags & FLAG_PATH_LENGTH_MASK;
    if (length < FLAG_PATH_LENGTH_MASK)
    {
        if (pathStart + length > index.size())
            return std::nullopt;
        return pathStart + length;
    }
    const auto nul = std::find(index.begin() + static_cast<std::ptrdiff_t>(pathStart), index.end(), '\0');
    if (nul == index.end())
        return std::nullopt;
    return static_cast<std::size_t>(nul - index.begin());
}

// The blob id (hex) the index records for `path`; nullopt when the path is not in
// it (or only as a merge conflict) or the index is not one this reads.
std::optional<std::string> IndexedBlobId(const std::vector<unsigned char>& index, std::string_view path)
{
    if (index.size() < INDEX_HEADER_BYTES || !std::equal(INDEX_SIGNATURE.begin(), INDEX_SIGNATURE.end(), index.begin()))
        return std::nullopt;
    const std::uint32_t version = ReadBigEndian(index, INDEX_VERSION_OFFSET, sizeof(std::uint32_t));
    if (version < FIRST_INDEX_VERSION || version > LAST_INDEX_VERSION)
        return std::nullopt;

    const std::uint32_t count = ReadBigEndian(index, INDEX_COUNT_OFFSET, sizeof(std::uint32_t));
    std::size_t entry = INDEX_HEADER_BYTES;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (entry + ENTRY_PATH_OFFSET > index.size())
            return std::nullopt;
        const std::uint32_t flags = ReadBigEndian(index, entry + ENTRY_FLAGS_OFFSET, sizeof(std::uint16_t));
        const std::size_t pathStart = entry + ENTRY_PATH_OFFSET + ((flags & FLAG_EXTENDED) ? EXTENDED_FLAGS_BYTES : 0);
        const std::optional<std::size_t> pathEnd = EntryPathEnd(index, pathStart, flags);
        if (!pathEnd)
            return std::nullopt;
        const std::string_view entryPath(reinterpret_cast<const char*>(index.data() + pathStart), *pathEnd - pathStart);
        if (entryPath == path && (flags & FLAG_STAGE_MASK) == 0)
            return Editor::Files::ToHex(index.data() + entry + ENTRY_BLOB_ID_OFFSET, BLOB_ID_BYTES);
        entry += ((*pathEnd - entry) / ENTRY_ALIGNMENT + 1) * ENTRY_ALIGNMENT;
    }
    return std::nullopt;
}

std::optional<GitDirs> FindGitDirs(const fs::path& repoRoot, std::string& error)
{
    const fs::path entry = repoRoot / GIT_ENTRY;
    std::error_code ec;
    if (fs::is_directory(entry, ec))
        return GitDirs{entry, entry};

    const std::optional<std::string> line = ReadFirstLine(entry);
    if (!line || line->rfind(GITDIR_PREFIX, 0) != 0)
    {
        error = "no .git folder or worktree file in " + Editor::Text::PathToUtf8(repoRoot);
        return std::nullopt;
    }
    GitDirs dirs;
    dirs.gitDir = ResolveFrom(repoRoot, Trim(std::string_view(*line).substr(GITDIR_PREFIX.size())));
    const std::optional<std::string> common = ReadFirstLine(dirs.gitDir / COMMON_DIR_FILE);
    dirs.commonDir = common ? ResolveFrom(dirs.gitDir, *common) : dirs.gitDir;
    return dirs;
}

// Calls `visit(name, commit)` for every packed ref; peeled tag lines are skipped.
template <typename Visitor> void ForEachPackedRef(const fs::path& commonDir, Visitor visit)
{
    std::ifstream stream(commonDir / PACKED_REFS_FILE, std::ios::binary);
    std::string line;
    while (std::getline(stream, line))
    {
        const std::string_view text = Trim(line);
        if (text.empty() || text.front() == PACKED_COMMENT || text.front() == PACKED_PEELED)
            continue;
        const std::size_t space = text.find(' ');
        if (space == std::string_view::npos)
            continue;
        visit(Trim(text.substr(space + 1)), text.substr(0, space));
    }
}

std::optional<std::string> PackedRef(const fs::path& commonDir, std::string_view ref)
{
    std::optional<std::string> found;
    ForEachPackedRef(commonDir,
                     [&](std::string_view name, std::string_view commit)
                     {
                         if (name == ref)
                             found = std::string(commit);
                     });
    return found;
}

// The text stored for `ref`: a loose file (the worktree's own first, then the
// shared one) or else the packed-refs line.
std::optional<std::string> RefValue(const GitDirs& dirs, const std::string& ref)
{
    for (const fs::path& dir : {dirs.gitDir, dirs.commonDir})
    {
        if (std::optional<std::string> value = ReadFirstLine(dir / Editor::Text::Utf8Path(ref)))
            return value;
    }
    return PackedRef(dirs.commonDir, ref);
}

// Follows HEAD (or a symbolic ref) to a commit; `branch` gets the last refs/heads/ name.
std::string ResolveToCommit(const GitDirs& dirs, std::string value, std::string& branch, std::string& error)
{
    for (int depth = 0; depth < MAX_SYMBOLIC_DEPTH; ++depth)
    {
        if (IsCommitHex(value))
            return value;
        if (value.rfind(SYMBOLIC_REF_PREFIX, 0) != 0)
        {
            error = "unexpected ref content '" + value + "'";
            return {};
        }
        const std::string ref(Trim(std::string_view(value).substr(SYMBOLIC_REF_PREFIX.size())));
        if (ref.rfind(BRANCH_PREFIX, 0) == 0)
            branch = ref.substr(BRANCH_PREFIX.size());
        std::optional<std::string> next = RefValue(dirs, ref);
        if (!next)
        {
            error = ref + " does not exist yet (no commit on this branch?)";
            return {};
        }
        value = *next;
    }
    error = "symbolic refs nest too deeply";
    return {};
}
} // namespace

HeadInfo ReadHead(const fs::path& repoRoot)
{
    HeadInfo info;
    const std::optional<GitDirs> dirs = FindGitDirs(repoRoot, info.error);
    if (!dirs)
        return info;
    const std::optional<std::string> head = ReadFirstLine(dirs->gitDir / HEAD_FILE);
    if (!head)
    {
        info.error = "cannot read " + Editor::Text::PathToUtf8(dirs->gitDir / HEAD_FILE);
        return info;
    }
    info.commit = ResolveToCommit(*dirs, *head, info.branch, info.error);
    return info;
}

bool IsOriginBranchTip(const fs::path& repoRoot, const std::string& commit)
{
    std::string error;
    const std::optional<GitDirs> dirs = FindGitDirs(repoRoot, error);
    if (!dirs || !IsCommitHex(commit))
        return false;

    // A loose ref file replaces the packed-refs line of the same name.
    std::set<std::string> looseNames;
    bool found = false;
    std::error_code ec;
    const fs::path remotes = dirs->commonDir / ORIGIN_REMOTES_FOLDER;
    for (fs::recursive_directory_iterator it(remotes, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec))
            continue;
        looseNames.insert(Editor::Text::GenericPathToUtf8(it->path().lexically_relative(dirs->commonDir)));
        found = found || ReadFirstLine(it->path()) == commit;
    }
    ForEachPackedRef(dirs->commonDir,
                     [&](std::string_view name, std::string_view packedCommit)
                     {
                         const bool onOrigin = name.rfind(ORIGIN_REMOTE_PREFIX, 0) == 0;
                         if (onOrigin && packedCommit == commit && !looseNames.contains(std::string(name)))
                             found = true;
                     });
    return found;
}

bool IsFileUnchanged(const fs::path& repoRoot, const std::string& repoRelative)
{
    std::string error;
    const std::optional<GitDirs> dirs = FindGitDirs(repoRoot, error);
    if (!dirs)
        return false;
    const std::optional<std::string> indexed = IndexedBlobId(ReadAllBytes(dirs->gitDir / INDEX_FILE), repoRelative);
    return indexed && *indexed == Editor::Files::GitBlobIdHex(repoRoot / Editor::Text::Utf8Path(repoRelative));
}
} // namespace Editor::Git

#endif // _EDITOR
