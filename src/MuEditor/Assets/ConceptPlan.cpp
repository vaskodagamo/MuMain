#include "ConceptPlan.h"

#ifdef _EDITOR

#include "JsonFields.h"

#include <algorithm>
#include <cstdio>

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
constexpr const char* NO_API_KEY = "no_api_key";
constexpr double CENTS_PRECISION_BELOW = 0.10; // under ten cents, show four decimals
constexpr std::size_t MONEY_CHARS = 32;

int CountImages(const json& item)
{
    int images = 0;
    for (const json& request : Array(item, "requests"))
        images += Int(request, "n", 0);
    return images;
}

PlanItem ParseItem(const json& item)
{
    PlanItem parsed;
    parsed.key = Text(item, "key");
    parsed.keys = TextList(item, "keys");
    parsed.name = Text(item, "name");
    parsed.family = Text(item, "family");
    parsed.tier = Int(item, "tier", 0);
    parsed.note = Text(item, "note");
    const json& reference = Member(item, "reference");
    parsed.referenceExists = Bool(reference, "exists", false);
    parsed.referencePath = Text(reference, "path");
    parsed.parentImage = Text(item, "parent_image");
    parsed.images = CountImages(item);
    parsed.estimate = ParseCost(Member(item, "estimate"));
    return parsed;
}

PlanReason ParseReason(const json& reason)
{
    return {Text(reason, "code"), Text(reason, "message"), TextList(reason, "keys")};
}

void ParseSettings(const json& settings, ConceptPlan& plan)
{
    plan.model = Text(settings, "model");
    plan.quality = Text(settings, "quality");
    plan.size = Text(settings, "size");
    plan.variants = Int(settings, "variants", 0);
}

void ParseTotals(const json& document, ConceptPlan& plan)
{
    const json& totals = Member(document, "totals");
    plan.images = Int(totals, "images", 0);
    plan.requests = Int(totals, "requests", 0);
    plan.total = ParseCost(totals);
    plan.flags = TextList(totals, "flags");
    const json& caps = Member(document, "caps");
    plan.maxImages = Int(caps, "max_images", 0);
    plan.maxCost = Number(caps, "max_cost", 0.0);
}
} // namespace

Cost ParseCost(const json& cost)
{
    const json& parts = Member(cost, "parts");
    return {Number(parts, "text", 0.0), Number(parts, "reference", 0.0), Number(parts, "output", 0.0),
            Number(cost, "total", 0.0)};
}

std::vector<std::string> ConceptPlan::MissingReferences() const
{
    for (const PlanReason& reason : reasons)
    {
        if (reason.code == "missing_reference")
            return reason.keys;
    }
    return {};
}

bool ConceptPlan::RefusedOnlyForKey() const
{
    const auto isKeyReason = [](const PlanReason& reason) { return reason.code == NO_API_KEY; };
    return std::all_of(reasons.begin(), reasons.end(), isKeyReason);
}

bool ParseConceptPlan(const std::string& text, ConceptPlan& plan)
{
    plan = {};
    const json document = json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object() || Int(document, "protocol", 0) != PROTOCOL_VERSION)
        return false;
    plan.ok = Bool(document, "ok", false);
    if (!plan.ok)
    {
        plan.error = Text(document, "error");
        plan.exitCode = Int(document, "exit", 1);
        return true;
    }
    plan.kind = Text(document, "kind");
    ParseSettings(Member(document, "settings"), plan);
    for (const json& item : Array(document, "items"))
        plan.items.push_back(ParseItem(item));
    ParseTotals(document, plan);
    const json& key = Member(document, "api_key");
    plan.apiKey.found = Bool(key, "found", false);
    plan.apiKey.source = plan.apiKey.found ? Text(key, "source") : std::string();
    plan.wouldRefuse = Bool(document, "would_refuse", true);
    for (const json& reason : Array(document, "reasons"))
        plan.reasons.push_back(ParseReason(reason));
    return true;
}

ConceptPresets ParseConceptPresets(const std::string& pricesJson)
{
    ConceptPresets result;
    const json document = json::parse(pricesJson, nullptr, false);
    if (document.is_discarded() || !document.is_object())
        return result;
    result.defaultPreset = Text(document, "default_preset");
    for (const auto& [name, preset] : Member(document, "presets").items())
    {
        if (!preset.is_object())
            continue;
        result.presets.push_back({name, Text(preset, "model"), Text(preset, "quality"), Int(preset, "variants", 0),
                                  Int(preset, "ref_size", 0)});
    }
    return result;
}

std::string FormatDollars(double dollars)
{
    char text[MONEY_CHARS];
    std::snprintf(text, sizeof(text), dollars < CENTS_PRECISION_BELOW ? "$%.4f" : "$%.2f", dollars);
    return text;
}
} // namespace Editor::Concepts

#endif // _EDITOR
