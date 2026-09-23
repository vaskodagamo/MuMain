#include "ConceptListing.h"

#ifdef _EDITOR

#include "EditorText.h"
#include "JsonFields.h"

#include <algorithm>

using nlohmann::json;

namespace Editor::Concepts
{
namespace
{
using Assets::Json::Array;
using Assets::Json::Bool;
using Assets::Json::Int;
using Assets::Json::Member;
using Assets::Json::Number;
using Assets::Json::Text;
using Assets::Json::TextList;

constexpr int PROTOCOL_VERSION = 1;
constexpr std::string_view LINEAGE_ARROW = " <- ";

// A JSON document of protocol 1, or a discarded value.
json ParseRecord(const std::string& text)
{
    json document = json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object() || Int(document, "protocol", 0) != PROTOCOL_VERSION)
        return json(json::value_t::discarded);
    return document;
}

std::optional<VariantRef> ParseRef(const json& link)
{
    if (!link.is_object())
        return std::nullopt;
    VariantRef ref{Text(link, "batch"), Text(link, "key"), Text(link, "variant")};
    if (ref.batch.empty() || ref.variant.empty())
        return std::nullopt;
    return ref;
}

ConceptVariant ParseVariant(const json& variant)
{
    ConceptVariant parsed;
    parsed.ref = {Text(variant, "batch"), Text(variant, "key"), Text(variant, "variant")};
    parsed.kind = Text(variant, "kind");
    parsed.image = Text(variant, "image");
    parsed.created = Text(variant, "created");
    parsed.note = Text(variant, "note");
    parsed.model = Text(variant, "model");
    parsed.quality = Text(variant, "quality");
    parsed.size = Text(variant, "size");
    const json& cost = Member(variant, "cost");
    parsed.imageCostActual = Number(cost, "image_actual", 0.0);
    parsed.imageCostEstimated = Number(cost, "image_estimated", 0.0);
    const auto parentIt = variant.find("parent");
    if (parentIt != variant.end())
        parsed.parent = ParseRef(*parentIt);
    for (const json& link : Array(variant, "lineage"))
    {
        if (const std::optional<VariantRef> ref = ParseRef(link))
            parsed.lineage.push_back(*ref);
    }
    parsed.discarded = Bool(variant, "discarded", false);
    parsed.picked = Bool(variant, "picked", false);
    return parsed;
}

std::optional<ConceptPick> ParsePick(const json& document)
{
    const auto it = document.find("pick");
    if (it == document.end() || !it->is_object())
        return std::nullopt;
    return ConceptPick{Text(*it, "image"), Text(*it, "batch"), Text(*it, "variant"), Text(*it, "picked")};
}

void ParseItem(const json& item, ConceptListing& listing)
{
    listing.name = Text(item, "name");
    listing.family = Text(item, "family");
    listing.tier = Int(item, "tier", 0);
    listing.keys = TextList(item, "keys");
}
} // namespace

bool ParseConceptListing(const std::string& text, ConceptListing& listing)
{
    listing = {};
    const json document = ParseRecord(text);
    if (document.is_discarded())
        return false;
    listing.ok = Bool(document, "ok", false);
    listing.error = Text(document, "error");
    listing.key = Text(document, "key");
    listing.requestedKey = Text(document, "requested_key");
    ParseItem(Member(document, "item"), listing);
    const json& reference = Member(document, "reference");
    listing.referencePath = Text(reference, "path");
    listing.referenceExists = Bool(reference, "exists", false);
    for (const json& variant : Array(document, "variants"))
        listing.variants.push_back(ParseVariant(variant));
    listing.pick = ParsePick(document);
    listing.warnings = TextList(document, "warnings");
    return true;
}

std::vector<ConceptBatchGroup> GroupByBatch(const ConceptListing& listing, bool includeDiscarded)
{
    std::vector<ConceptBatchGroup> groups;
    for (std::size_t i = 0; i < listing.variants.size(); ++i)
    {
        const ConceptVariant& variant = listing.variants[i];
        if (variant.discarded && !includeDiscarded)
            continue;
        auto group = std::find_if(groups.begin(), groups.end(),
                                  [&](const ConceptBatchGroup& g) { return g.batch == variant.ref.batch; });
        if (group == groups.end())
        {
            groups.push_back({variant.ref.batch, variant.kind, variant.created, {}});
            group = groups.end() - 1;
        }
        group->variants.push_back(i);
    }
    std::stable_sort(groups.begin(), groups.end(),
                     [](const ConceptBatchGroup& a, const ConceptBatchGroup& b) { return a.batch > b.batch; });
    return groups;
}

std::string LineageText(const ConceptVariant& variant)
{
    std::vector<std::string> steps;
    for (const VariantRef& ref : variant.lineage)
        steps.push_back(VariantPath(ref));
    if (steps.empty() && variant.parent)
        steps.push_back(VariantPath(*variant.parent));
    return Editor::Text::Join(steps, LINEAGE_ARROW);
}

bool ParseConceptSummaries(const std::string& text, ConceptSummaries& summaries)
{
    summaries = {};
    const json document = ParseRecord(text);
    if (document.is_discarded())
        return false;
    summaries.ok = Bool(document, "ok", false);
    summaries.error = Text(document, "error");
    for (const json& item : Array(document, "items"))
    {
        const ConceptSummary summary{Text(item, "key"), Int(item, "variants", 0), Int(item, "discarded", 0),
                                     Bool(item, "picked", false)};
        std::vector<std::string> keys = TextList(item, "keys");
        if (keys.empty())
            keys.push_back(summary.key);
        for (const std::string& key : keys)
            summaries.byItemKey[key] = summary;
    }
    for (const json& batch : Array(document, "batches"))
    {
        if (Bool(batch, "running", false))
            summaries.runningBatches.push_back(Text(batch, "batch"));
    }
    return true;
}

bool CommandSucceeded(const std::string& text, std::string& error)
{
    const json document = ParseRecord(text);
    if (document.is_discarded())
    {
        error = "concepts.py gave no readable answer";
        return false;
    }
    if (Bool(document, "ok", false))
        return true;
    error = Text(document, "error");
    return false;
}

std::string SummaryLabel(const ConceptSummary& summary)
{
    if (summary.picked)
        return "picked";
    const int shown = summary.variants - summary.discarded;
    if (shown <= 0)
        return summary.variants > 0 ? "all discarded" : "";
    return std::to_string(shown) + (shown == 1 ? " concept" : " concepts");
}
} // namespace Editor::Concepts

#endif // _EDITOR
