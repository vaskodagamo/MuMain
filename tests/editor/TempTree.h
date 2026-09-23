#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

// Scratch folders and small file helpers for the editor's file-system tests.
namespace EditorTest
{
// A throwaway folder <temp>/<name>, removed again when the test case ends.
class TempTree
{
public:
    explicit TempTree(const std::string& name) : m_root(std::filesystem::temp_directory_path() / name)
    {
        std::filesystem::remove_all(m_root);
        std::filesystem::create_directories(m_root);
    }
    ~TempTree()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }
    TempTree(const TempTree&) = delete;
    TempTree& operator=(const TempTree&) = delete;

    const std::filesystem::path& Root() const
    {
        return m_root;
    }

private:
    std::filesystem::path m_root;
};

inline void WriteText(const std::filesystem::path& file, const std::string& text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

inline std::string ReadText(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}
} // namespace EditorTest
