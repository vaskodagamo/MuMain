#include "ClientReview.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "JsonFields.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Editor::Assets
{
namespace
{
// Hide Editor::Text (the namespace) behind the JSON field readers.
using Json::Text;

constexpr const char* CLIENT_REVIEW_FILE_NAME = "client-review.json";
constexpr int JSON_INDENT = 2;

// The file as a JSON object: an empty object when it does not exist yet.
bool ReadDocument(const fs::path& file, json& out, std::string& error)
{
    out = json::object();
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return true;
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    out = json::parse(text, nullptr, false);
    if (out.is_discarded() || !out.is_object())
    {
        error = Editor::Text::PathToUtf8(file) + " is not a JSON object; fix or delete it first";
        return false;
    }
    return true;
}

bool WriteDocument(const fs::path& file, const json& document, std::string& error)
{
    // Invalid UTF-8 in a typed note becomes U+FFFD instead of failing the save.
    const std::string text = document.dump(JSON_INDENT, ' ', false, json::error_handler_t::replace) + '\n';
    return Json::ReplaceFileText(file, text, error);
}
} // namespace

fs::path ClientReviewFile(const fs::path& repoRoot, int world)
{
    return WorldAssetsDir(repoRoot, world) / CLIENT_REVIEW_FILE_NAME;
}

bool ParseClientReviews(const std::string& text, ClientReviews& out, std::string& error)
{
    const json document = json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object())
    {
        error = "client-review.json is not a JSON object";
        return false;
    }
    out.clear();
    for (const auto& [model, entry] : document.items())
    {
        if (entry.is_object())
            out[model] = ClientReview{Text(entry, "verdict"), Text(entry, "note"), Text(entry, "date")};
    }
    return true;
}

ClientReviews ReadClientReviews(const fs::path& file, std::string& error)
{
    ClientReviews reviews;
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return reviews;
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    ParseClientReviews(text, reviews, error);
    return reviews;
}

bool RecordClientReview(const fs::path& file, const std::string& model, const ClientReview& review, std::string& error)
{
    json document;
    if (!ReadDocument(file, document, error))
        return false;
    document[model] = json{{"verdict", review.verdict}, {"note", review.note}, {"date", review.date}};
    return WriteDocument(file, document, error);
}
} // namespace Editor::Assets

#endif // _EDITOR
