#pragma once

#ifdef _EDITOR

#include <json.hpp>

#include <string>
#include <vector>

// What `concepts.py plan ... --json` says before any money is spent: per item
// and in total the cost split (text prompt / reference image / output images),
// the caps, whether `run` would refuse and why, and whether an API key was found
// (only found and where: never the key). Also the presets of image_prices.json.
namespace Editor::Concepts
{
// US dollars: {"parts": {"text", "reference", "output"}, "total"}.
struct Cost
{
    double text = 0.0;
    double reference = 0.0;
    double output = 0.0;
    double total = 0.0;
};
Cost ParseCost(const nlohmann::json& cost);

struct PlanReason
{
    std::string code; // max_images, max_cost, missing_reference, no_api_key
    std::string message;
    std::vector<std::string> keys; // missing_reference: the items without a reference render
};

struct PlanItem
{
    std::string key; // the concept key (a set's body armour, a shared group's first key)
    std::vector<std::string> keys;
    std::string name;
    std::string family;
    int tier = 0;
    std::string note;
    bool referenceExists = false;
    std::string referencePath;
    std::string parentImage; // a refine: the variant it starts from
    int images = 0;
    Cost estimate;
};

struct ApiKeyStatus
{
    bool found = false;
    std::string source; // "env" or "keychain" (empty when not found)
};

struct ConceptPlan
{
    bool ok = false;
    std::string error; // ok == false: what the tool said
    int exitCode = 0;  // ok == false: the tool's exit code
    std::string kind;  // generate or refine
    std::string model;
    std::string quality;
    std::string size;
    int variants = 0;
    std::vector<PlanItem> items;
    int images = 0;
    int requests = 0;
    Cost total;
    std::vector<std::string> flags; // e.g. "token-estimated (no per-image price)"
    int maxImages = 0;
    double maxCost = 0.0;
    ApiKeyStatus apiKey;
    bool wouldRefuse = false;
    std::vector<PlanReason> reasons;

    // The keys of the missing_reference reason (empty when every reference exists).
    std::vector<std::string> MissingReferences() const;
    // Only a missing API key (or nothing) stands between this plan and a run.
    bool RefusedOnlyForKey() const;
};

// Parses the `--json` result object (also an `"ok": false` error object). False
// when `text` is not a protocol record at all.
bool ParseConceptPlan(const std::string& text, ConceptPlan& plan);

struct ConceptPreset
{
    std::string name; // explore, final
    std::string model;
    std::string quality;
    int variants = 0;
    int referenceSize = 0;
};

// The presets of tools/item_editor/image_prices.json and its default one.
struct ConceptPresets
{
    std::vector<ConceptPreset> presets;
    std::string defaultPreset;
};
ConceptPresets ParseConceptPresets(const std::string& pricesJson);

// "$0.21", "$0.0707" below ten cents.
std::string FormatDollars(double dollars);
} // namespace Editor::Concepts

#endif // _EDITOR
