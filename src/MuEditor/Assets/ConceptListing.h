#pragma once

#ifdef _EDITOR

#include "ConceptCommands.h" // VariantRef

#include <map>
#include <optional>
#include <string>
#include <vector>

// The concept images `concepts.py list` reports: one item's current reference,
// every variant of every batch (with its refine lineage, discarded and picked
// marks) and the pick (`list --key K --json`), or one line per item with
// concepts (`list --json`) for Browse's concept column.
namespace Editor::Concepts
{
struct ConceptVariant
{
    VariantRef ref;
    std::string kind; // generate or refine
    std::string image; // absolute path of the PNG
    std::string created;
    std::string note;
    std::string model;
    std::string quality;
    std::string size;
    double imageCostActual = 0.0; // 0 when unknown
    double imageCostEstimated = 0.0;
    std::optional<VariantRef> parent; // a refine: the variant it came from
    std::vector<VariantRef> lineage;  // nearest first
    bool discarded = false;
    bool picked = false;
};

struct ConceptPick
{
    std::string image; // assets-work/Items/concepts/<key>/concept.jpg (absolute)
    std::string batch;
    std::string variant;
    std::string picked; // date
};

struct ConceptListing
{
    bool ok = false;
    std::string error;
    std::string key;          // the concept key
    std::string requestedKey; // the item asked for
    std::string name;
    std::string family;
    int tier = 0;
    std::vector<std::string> keys; // every item sharing the concept
    std::string referencePath;
    bool referenceExists = false;
    std::vector<ConceptVariant> variants; // oldest batch first
    std::optional<ConceptPick> pick;
    std::vector<std::string> warnings;
};

// Parses `list --key K --json`. False when `text` is not a protocol record.
bool ParseConceptListing(const std::string& text, ConceptListing& listing);

// The variants of one batch, as the Concepts panel groups them.
struct ConceptBatchGroup
{
    std::string batch;
    std::string kind;
    std::string created;
    std::vector<std::size_t> variants; // indices into ConceptListing::variants, in variant order
};

// Batches newest first (by name: they start with the time they were made);
// discarded variants only with `includeDiscarded`, and batches left empty dropped.
std::vector<ConceptBatchGroup> GroupByBatch(const ConceptListing& listing, bool includeDiscarded);

// "v1 <- 20260923-130336-study-top-10/6-0/v2": a refine's ancestry, nearest first.
std::string LineageText(const ConceptVariant& variant);

// One item's line of `list --json`.
struct ConceptSummary
{
    std::string key; // the concept key
    int variants = 0;
    int discarded = 0;
    bool picked = false;
};

struct ConceptSummaries
{
    bool ok = false;
    std::string error;
    std::map<std::string, ConceptSummary> byItemKey; // every item key of each concept
    std::vector<std::string> runningBatches;          // batches a run is still writing
};

// Parses `list --json`.
bool ParseConceptSummaries(const std::string& text, ConceptSummaries& summaries);

// The answer of pick / unpick / discard / undiscard `--json`: true when it says
// "ok"; else `error` is the tool's message.
bool CommandSucceeded(const std::string& text, std::string& error);

// Browse's short text: "picked", "3 concepts", "" (none).
std::string SummaryLabel(const ConceptSummary& summary);
} // namespace Editor::Concepts

#endif // _EDITOR
