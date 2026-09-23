#include <doctest.h>

#include "Assets/EditorText.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace Editor::Text;

namespace
{
// UTF-8 of a few characters Python's str.strip() removes, and one it keeps.
constexpr const char* NO_BREAK_SPACE = "\xC2\xA0";        // U+00A0
constexpr const char* EM_SPACE = "\xE2\x80\x83";          // U+2003
constexpr const char* IDEOGRAPHIC_SPACE = "\xE3\x80\x80"; // U+3000
constexpr const char* ZERO_WIDTH_SPACE = "\xE2\x80\x8B";  // U+200B, not white space to Python
constexpr const char* E_ACUTE = "\xC3\xA9";               // U+00E9
} // namespace

TEST_CASE("Trim removes the white space Python's strip() removes [editor][text]")
{
    CHECK(Trim(" \t Taller flames \r\n") == "Taller flames");
    CHECK(Trim(std::string(NO_BREAK_SPACE)).empty());
    CHECK(Trim(std::string(IDEOGRAPHIC_SPACE) + "Moss" + EM_SPACE + NO_BREAK_SPACE) == "Moss");
    CHECK(Trim("\v\f\x1C text \x1F") == "text");
    CHECK(Trim(std::string("caf") + E_ACUTE + " ") == std::string("caf") + E_ACUTE);
    CHECK(Trim(ZERO_WIDTH_SPACE) == ZERO_WIDTH_SPACE);
    CHECK(Trim("").empty());
    // A cut-off sequence at the end is kept as it is.
    CHECK(Trim("abc\xE2\x80") == "abc\xE2\x80");
}

TEST_CASE("NonEmptyLines keeps one trimmed item per line with text [editor][text]")
{
    const std::string text = std::string("Taller flames\n") + NO_BREAK_SPACE + "\n  keep the base  \r\n\n\t";
    CHECK(NonEmptyLines(text) == std::vector<std::string>{"Taller flames", "keep the base"});
    CHECK(NonEmptyLines("").empty());
    CHECK(NonEmptyLines("single") == std::vector<std::string>{"single"});
}

TEST_CASE("Join, case folding and UTF-8 paths [editor][text]")
{
    CHECK(Join({"a", "b", "c"}, ", ") == "a, b, c");
    CHECK(Join({}, "; ").empty());
    CHECK(LowerAscii('Q') == 'q');
    CHECK(LowerAscii('\xC3') == '\xC3');
    CHECK(EqualIgnoringCase("Tree_A.TGA", "tree_a.tga"));
    CHECK_FALSE(EqualIgnoringCase("tree.jpg", "tree.jpeg"));

    const std::string name = std::string("Lorencia ") + E_ACUTE + ".obj";
    CHECK(PathToUtf8(Utf8Path(name)) == name);
    CHECK(GenericPathToUtf8(fs::path("src") / "bin" / "Data") == "src/bin/Data");
}

TEST_CASE("ContainsIgnoringCase finds a part of a name in any case [editor][text]")
{
    CHECK(ContainsIgnoringCase("Tree01", "tree"));
    CHECK(ContainsIgnoringCase("HouseWall03", "WALL0"));
    CHECK_FALSE(ContainsIgnoringCase("Fence01", "tree"));
    CHECK(ContainsIgnoringCase("Fence01", ""));
    CHECK_FALSE(ContainsIgnoringCase("", "a"));
}
