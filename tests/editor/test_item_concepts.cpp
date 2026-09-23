#include "stdafx.h"

#include <doctest.h>

#include "Assets/ConceptCommands.h"
#include "Assets/ConceptJobState.h"
#include "Assets/ConceptListing.h"
#include "Assets/ConceptPlan.h"
#include "Editing/ItemSelection.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Editor::Concepts;
using Editor::Editing::ClickModifiers;
using Editor::Editing::ItemSelection;

namespace
{
const std::filesystem::path REPO = "/repo";

bool Has(const std::vector<std::string>& args, const std::string& value)
{
    return std::find(args.begin(), args.end(), value) != args.end();
}

// The value after `flag` (every one when the flag repeats).
std::vector<std::string> ValuesOf(const std::vector<std::string>& args, const std::string& flag)
{
    std::vector<std::string> values;
    for (std::size_t i = 0; i + 1 < args.size(); ++i)
    {
        if (args[i] == flag)
            values.push_back(args[i + 1]);
    }
    return values;
}

// The events of a real run against the local mock server (2026-09-23), one retry on a 429.
constexpr const char* RUN_EVENTS[] = {
    R"({"batch": "20260923-153313-0-2-6-0", "concurrency": 1, "dir": "/out/20260923-153313-0-2-6-0", "estimate": {"images": 4, "parts": {"output": 0.05268, "reference": 0.049152, "text": 0.0132}, "requests": 2, "total": 0.115}, "event": "started", "images": 4, "kind": "generate", "protocol": 1, "requests": 2, "time": "2026-09-23T15:33:13+02:00"})",
    R"({"estimated": {"total": 0.0576}, "event": "request_started", "key": "0-2", "protocol": 1, "request": "r1", "time": "t", "variants": ["v1", "v2"]})",
    R"({"attempt": 1, "delay_s": 3.0, "event": "retry", "key": "0-2", "protocol": 1, "reason": "HTTP 429: Rate limit reached - mock", "request": "r1", "time": "t"})",
    R"({"actual": {"total": 0.036}, "event": "request_done", "key": "0-2", "protocol": 1, "request": "r1", "request_id": "req_mock_2", "time": "t", "variants": [{"image": "/out/b/0-2/v1.png", "variant": "v1"}, {"image": "/out/b/0-2/v2.png", "variant": "v2"}]})",
    R"({"estimated": {"total": 0.0575}, "event": "request_started", "key": "6-0", "protocol": 1, "request": "r1", "time": "t", "variants": ["v1", "v2"]})",
    R"({"event": "request_done", "key": "6-0", "protocol": 1, "request": "r1", "time": "t", "variants": [{"variant": "v1"}, {"variant": "v2"}]})",
    R"({"batch": "20260923-153313-0-2-6-0", "cost": {"actual": {"parts": {"output": 0.05268, "reference": 0.016, "text": 0.004}, "total": 0.07268}, "estimated": {"parts": {"output": 0.05268, "reference": 0.049152, "text": 0.0132}, "total": 0.115}, "requests": 2, "requests_with_usage": 2}, "dir": "/out/b", "done": 2, "event": "finished", "failed": 0, "not_started": 0, "nothing_to_do": false, "protocol": 1, "sheet": "/out/b/sheet.html", "time": "t"})",
};

constexpr const char* PLAN = R"({"api_key": {"env": "OPENAI_API_KEY", "found": true, "keychain_service": "openai-api-key", "source": "keychain"},
  "caps": {"max_cost": 5.0, "max_images": 30}, "command": "plan", "flags": ["token-estimated, no per-image price"],
  "items": [
    {"estimate": {"parts": {"output": 0.0395, "reference": 0.0246, "text": 0.0067}, "total": 0.0708}, "family": "swords",
     "key": "0-2", "keys": ["0-2"], "kind": "item", "name": "Rapier", "note": null,
     "reference": {"exists": true, "path": "/r/0-2.png"}, "requests": [{"id": "r1", "n": 3}], "tier": 1},
    {"estimate": {"total": 0.07}, "key": "8-1", "keys": ["7-1", "8-1", "9-1"], "name": "Dragon Armor", "note": "darker",
     "reference": {"exists": false, "path": "/r/8-1.png"}, "requests": [{"id": "r1", "n": 2}, {"id": "r2", "n": 1}], "tier": 6}],
  "kind": "generate", "ok": true, "protocol": 1,
  "reasons": [{"code": "missing_reference", "keys": ["8-1"], "message": "no reference render for 8-1"}],
  "settings": {"model": "gpt-image-2.5-flare", "quality": "medium", "size": "1024x1024", "variants": 3},
  "totals": {"flags": ["token-estimated, no per-image price"], "images": 6, "parts": {"output": 0.079, "reference": 0.049, "text": 0.013}, "requests": 3, "total": 0.1408},
  "would_refuse": true})";

constexpr const char* LISTING = R"({"command": "list", "key": "6-0", "ok": true, "protocol": 1, "requested_key": "6-0",
  "item": {"family": "shields", "keys": ["6-0"], "kind": "item", "name": "Small Shield", "tier": 1},
  "reference": {"exists": true, "path": "/out/refs/6-0.png", "source": "refs"},
  "pick": {"batch": "20260923-160000-refine", "image": "/repo/assets-work/Items/concepts/6-0/concept.jpg", "picked": "2026-09-23", "variant": "v1"},
  "variants": [
    {"batch": "20260923-130336-study-top-10", "key": "6-0", "variant": "v1", "image": "/b1/6-0/v1.png", "kind": "generate", "discarded": false, "picked": false, "lineage": [], "parent": null, "cost": {"image_actual": 0.0235}},
    {"batch": "20260923-130336-study-top-10", "key": "6-0", "variant": "v2", "image": "/b1/6-0/v2.png", "kind": "generate", "discarded": true, "picked": false, "lineage": [], "parent": null},
    {"batch": "20260923-160000-refine", "key": "6-0", "variant": "v1", "image": "/b2/6-0/v1.png", "kind": "refine", "note": "more rivets", "discarded": false, "picked": true,
     "parent": {"batch": "20260923-130336-study-top-10", "key": "6-0", "variant": "v1"},
     "lineage": [{"batch": "20260923-130336-study-top-10", "key": "6-0", "variant": "v1"}]}],
  "warnings": []})";
} // namespace

TEST_CASE("Concept commands are argument lists with per-item notes and the repo root [editor][concepts]")
{
    GenerateSettings settings;
    settings.preset = "explore";
    settings.note = "less gold\n  darker grip ";
    settings.itemNotes["6-0"] = "more rivets";
    const std::vector<std::string> plan = PlanArgs({"0-2", "6-0"}, settings, REPO);
    CHECK(plan.front() == "plan");
    CHECK(ValuesOf(plan, "--keys") == std::vector<std::string>{"0-2,6-0"});
    CHECK(ValuesOf(plan, "--preset") == std::vector<std::string>{"explore"});
    CHECK_FALSE(Has(plan, "--variants")); // the preset's own count
    CHECK(ValuesOf(plan, "--note") ==
          std::vector<std::string>{"0-2=less gold; darker grip", "6-0=less gold; darker grip; more rivets"});
    CHECK(Has(plan, "--json"));
    CHECK(Has(plan, "--no-prompts"));
    CHECK(ValuesOf(plan, "--repo-root") == std::vector<std::string>{"/repo"});
    CHECK_FALSE(Has(plan, "--yes"));

    settings.variants = 2;
    settings.sheet = true;
    const std::vector<std::string> run = RunArgs({"0-2"}, settings, REPO, std::string("http://127.0.0.1:18431/v1"));
    CHECK(run.front() == "run");
    CHECK(Has(run, "--yes"));
    CHECK(Has(run, "--json-progress"));
    CHECK(Has(run, "--sheet"));
    CHECK(ValuesOf(run, "--variants") == std::vector<std::string>{"2"});
    CHECK(ValuesOf(run, "--api-base") == std::vector<std::string>{"http://127.0.0.1:18431/v1"});
    CHECK_FALSE(Has(RunArgs({"0-2"}, settings, REPO, std::nullopt), "--api-base"));
}

TEST_CASE("Refine, resume, refs and library commands [editor][concepts]")
{
    const VariantRef variant{"20260923-130336-study-top-10", "6-0", "v2"};
    CHECK(VariantPath(variant) == "20260923-130336-study-top-10/6-0/v2");
    const std::vector<std::string> refine = RefineRunArgs(variant, "thinner rim\nno gems", REPO, std::nullopt);
    CHECK(ValuesOf(refine, "--from") == std::vector<std::string>{"20260923-130336-study-top-10/6-0/v2"});
    CHECK(ValuesOf(refine, "--note") == std::vector<std::string>{"6-0=thinner rim; no gems"});
    CHECK(Has(refine, "--yes"));
    CHECK(RefinePlanArgs(variant, "x", REPO).front() == "plan");

    const std::vector<std::string> resume =
        ResumeArgs({"run", "--resume", "b1", "--yes"}, REPO, std::string("http://localhost:9/v1"));
    CHECK(std::vector<std::string>(resume.begin(), resume.begin() + 5) ==
          std::vector<std::string>{"run", "--resume", "b1", "--yes", "--json-progress"});
    CHECK(ValuesOf(resume, "--api-base").size() == 1);
    CHECK(std::count(resume.begin(), resume.end(), "--json-progress") == 1);
    CHECK(ResumeArgs({"run", "--json-progress"}, REPO, std::nullopt).size() == 4);
    CHECK(ResumeBatchArgs("b2", REPO, std::nullopt)[2] == "b2");

    CHECK(RefsArgs({"8-1", "0-2"}, REPO) ==
          std::vector<std::string>{"refs", "--keys", "8-1,0-2", "--json-progress", "--repo-root", "/repo"});
    CHECK(ListKeyArgs("6-0", REPO) == std::vector<std::string>{"list", "--key", "6-0", "--json", "--repo-root", "/repo"});
    CHECK(ListAllArgs(REPO).front() == "list");
    CHECK(PickArgs(variant, REPO) == std::vector<std::string>{"pick", "20260923-130336-study-top-10", "6-0", "v2",
                                                              "--json", "--repo-root", "/repo"});
    CHECK(UnpickArgs("6-0", REPO)[1] == "6-0");
    CHECK(DiscardArgs(variant, true, REPO).front() == "discard");
    CHECK(DiscardArgs(variant, false, REPO).front() == "undiscard");
}

TEST_CASE("Only a loopback http API base is accepted for tests [editor][concepts]")
{
    CHECK(LoopbackApiBase("http://127.0.0.1:18431/v1") == std::optional<std::string>("http://127.0.0.1:18431/v1"));
    CHECK(LoopbackApiBase("http://localhost/v1").has_value());
    CHECK(LoopbackApiBase("http://127.0.0.1").has_value());
    CHECK_FALSE(LoopbackApiBase("https://api.openai.com/v1").has_value());
    CHECK_FALSE(LoopbackApiBase("http://example.com/v1").has_value());
    CHECK_FALSE(LoopbackApiBase("http://127.0.0.1.evil.com/v1").has_value());
    CHECK_FALSE(LoopbackApiBase("http://localhost@evil.com/v1").has_value());
    CHECK_FALSE(LoopbackApiBase("http://127.0.0.1:80abc/v1").has_value());
    CHECK_FALSE(LoopbackApiBase("").has_value());
    CHECK(OneLineNote("  a \n\n b  ") == "a; b");
}

TEST_CASE("A plan gives per-item and total costs, caps, the key status and refusal reasons [editor][concepts]")
{
    ConceptPlan plan;
    REQUIRE(ParseConceptPlan(PLAN, plan));
    CHECK(plan.ok);
    CHECK(plan.model == "gpt-image-2.5-flare");
    REQUIRE(plan.items.size() == 2);
    CHECK(plan.items[0].images == 3);
    CHECK(plan.items[0].estimate.reference == doctest::Approx(0.0246));
    CHECK(plan.items[0].referenceExists);
    CHECK(plan.items[1].images == 3); // two requests
    CHECK(plan.items[1].note == "darker");
    CHECK(plan.items[1].keys.size() == 3);
    CHECK(plan.total.total == doctest::Approx(0.1408));
    CHECK(plan.total.output == doctest::Approx(0.079));
    CHECK(plan.images == 6);
    CHECK(plan.requests == 3);
    CHECK(plan.maxImages == 30);
    CHECK(plan.maxCost == doctest::Approx(5.0));
    CHECK(plan.apiKey.found);
    CHECK(plan.apiKey.source == "keychain");
    CHECK(plan.wouldRefuse);
    CHECK(plan.MissingReferences() == std::vector<std::string>{"8-1"});
    CHECK_FALSE(plan.RefusedOnlyForKey());

    ConceptPlan failed;
    REQUIRE(ParseConceptPlan(R"({"protocol": 1, "command": "plan", "ok": false, "error": "unknown item 99-1", "exit": 2})",
                             failed));
    CHECK_FALSE(failed.ok);
    CHECK(failed.error == "unknown item 99-1");
    CHECK(failed.exitCode == 2);
    CHECK_FALSE(ParseConceptPlan("usage: concepts.py ...", failed));
    CHECK_FALSE(ParseConceptPlan(R"({"protocol": 2, "ok": true})", failed));
}

TEST_CASE("Presets come from image_prices.json and dollars are formatted [editor][concepts]")
{
    const ConceptPresets presets = ParseConceptPresets(R"({"default_preset": "explore", "presets": {
        "explore": {"model": "gpt-image-2.5-flare", "quality": "medium", "variants": 3, "ref_size": 512},
        "final": {"model": "gpt-image-2.5-sunburst", "quality": "high", "variants": 1, "ref_size": 1024}}})");
    CHECK(presets.defaultPreset == "explore");
    REQUIRE(presets.presets.size() == 2);
    const auto final = std::find_if(presets.presets.begin(), presets.presets.end(),
                                    [](const ConceptPreset& preset) { return preset.name == "final"; });
    REQUIRE(final != presets.presets.end());
    CHECK(final->variants == 1);
    CHECK(final->referenceSize == 1024);
    CHECK(ParseConceptPresets("not json").presets.empty());
    CHECK(FormatDollars(0.2122) == "$0.21");
    CHECK(FormatDollars(0.0707) == "$0.0707");
    CHECK(FormatDollars(4.5) == "$4.50");
}

TEST_CASE("Run events become per-item states, the actual cost and the end [editor][concepts]")
{
    ConceptJobState state;
    state.Item("0-2").name = "Rapier";
    state.Item("6-0").name = "Small Shield";
    CHECK(state.Progress() == 0.0f);
    CHECK(ApplyLine(state, RUN_EVENTS[0], 0.0));
    CHECK(state.phase == JobPhase::Running);
    CHECK(state.batch == "20260923-153313-0-2-6-0");
    CHECK(state.estimated.total == doctest::Approx(0.115));
    ApplyLine(state, RUN_EVENTS[1], 1.0);
    CHECK(state.items[0].state == JobItemState::Generating);
    ApplyLine(state, RUN_EVENTS[2], 10.0);
    CHECK(state.items[0].state == JobItemState::Retrying);
    CHECK(state.items[0].message == "HTTP 429: Rate limit reached - mock");
    CHECK(RetrySecondsLeft(state.items[0], 11.0) == doctest::Approx(2.0));
    CHECK(RetrySecondsLeft(state.items[0], 20.0) == 0.0);
    ApplyLine(state, RUN_EVENTS[3], 13.0);
    CHECK(state.items[0].state == JobItemState::Done);
    CHECK(state.items[0].images == 2);
    CHECK(state.items[1].state == JobItemState::Waiting);
    CHECK(state.Progress() == doctest::Approx(0.5f));
    ApplyLine(state, RUN_EVENTS[4], 13.0);
    ApplyLine(state, RUN_EVENTS[5], 14.0);
    ApplyLine(state, RUN_EVENTS[6], 14.0);
    CHECK(state.phase == JobPhase::Finished);
    CHECK(IsFinal(state.phase));
    REQUIRE(state.actual.has_value());
    CHECK(state.actual->total == doctest::Approx(0.07268));
    CHECK(state.actual->reference == doctest::Approx(0.016));
    CHECK(state.sheet == "/out/b/sheet.html");
    CHECK(state.items.size() == 2); // the events named the seeded items
    CHECK_FALSE(state.CanResume());
    ApplyExit(state, 0);
    CHECK(state.phase == JobPhase::Finished); // the final event decides
    CHECK(state.exitCode == 0);
    CHECK_FALSE(ApplyLine(state, "Plan: 3 item(s)", 0.0));
    CHECK_FALSE(ApplyLine(state, R"({"protocol": 1, "command": "plan"})", 0.0));
}

TEST_CASE("A cancelled run keeps its resume arguments; failures can be retried [editor][concepts]")
{
    ConceptJobState cancelled;
    cancelled.Item("0-2");
    cancelled.Item("1-0");
    ApplyLine(cancelled, RUN_EVENTS[0], 0.0);
    ApplyLine(cancelled, R"({"protocol": 1, "event": "request_started", "key": "0-2"})", 0.0);
    ApplyLine(cancelled, R"({"protocol": 1, "event": "request_done", "key": "0-2", "variants": [{}]})", 0.0);
    ApplyLine(cancelled,
              R"({"protocol": 1, "event": "cancelled", "reason": "SIGTERM", "batch": "b1", "done": 1, "failed": 0, "not_started": 1,
                  "resume": ["run", "--resume", "b1", "--yes"], "cost": {"actual": {"total": 0.02}, "estimated": {"total": 0.13}, "requests_with_usage": 1}})",
              0.0);
    CHECK(cancelled.phase == JobPhase::Cancelled);
    CHECK(cancelled.items[0].state == JobItemState::Done);
    CHECK(cancelled.items[1].state == JobItemState::Cancelled);
    CHECK(cancelled.resume == std::vector<std::string>{"run", "--resume", "b1", "--yes"});
    CHECK(cancelled.CanResume());
    CHECK(cancelled.message == "Cancelled (SIGTERM)");
    ApplyExit(cancelled, 130);
    CHECK(cancelled.phase == JobPhase::Cancelled);

    ConceptJobState failed;
    ApplyLine(failed, RUN_EVENTS[0], 0.0);
    ApplyLine(failed, R"({"protocol": 1, "event": "request_started", "key": "6-0"})", 0.0);
    ApplyLine(failed, R"({"protocol": 1, "event": "request_failed", "key": "6-0", "error": "HTTP 500: server error", "status": 500})", 0.0);
    CHECK(failed.items[0].state == JobItemState::Failed);
    CHECK(failed.items[0].message == "HTTP 500: server error");
    ApplyLine(failed, R"({"protocol": 1, "event": "finished", "batch": "b2", "done": 0, "failed": 1, "not_started": 0, "cost": {}})", 0.0);
    CHECK(failed.phase == JobPhase::FinishedWithFailures);
    CHECK(failed.resume == std::vector<std::string>{"run", "--resume", "b2", "--yes"});
    CHECK(failed.CanResume());
    CHECK_FALSE(failed.actual.has_value());
}

TEST_CASE("Refusals, errors and exits without a final event [editor][concepts]")
{
    ConceptJobState refused;
    ApplyLine(refused, R"({"protocol": 1, "event": "refused", "reasons": [{"code": "max_cost", "message": "above $5"}], "estimate": {"total": 9.5}})", 0.0);
    CHECK(refused.phase == JobPhase::Refused);
    REQUIRE(refused.reasons.size() == 1);
    CHECK(refused.reasons[0].code == "max_cost");
    CHECK(refused.estimated.total == doctest::Approx(9.5));

    ConceptJobState noKey;
    ApplyLine(noKey, R"({"protocol": 1, "event": "error", "message": "no API key", "exit": 5})", 0.0);
    CHECK(noKey.phase == JobPhase::NoApiKey);
    CHECK(noKey.message == "no API key");

    ConceptJobState busy;
    ApplyExit(busy, 6);
    CHECK(busy.phase == JobPhase::Busy);
    CHECK_FALSE(busy.message.empty());

    ConceptJobState usage;
    ApplyExit(usage, 2);
    CHECK(usage.phase == JobPhase::Failed);
    CHECK(usage.message.find("exit 2") != std::string::npos);

    ConceptJobState killed;
    ApplyLine(killed, RUN_EVENTS[0], 0.0);
    ApplyExit(killed, -9);
    CHECK(killed.phase == JobPhase::Failed);

    ConceptJobState cutShort;
    ApplyLine(cutShort, RUN_EVENTS[0], 0.0);
    ApplyExit(cutShort, 0);
    CHECK(cutShort.phase == JobPhase::Failed);
}

TEST_CASE("Reference renders report cached and rendered items [editor][concepts]")
{
    ConceptJobState refs;
    ApplyLine(refs, R"({"protocol": 1, "event": "started", "dir": "/refs", "requests": 1, "items": 2})", 0.0);
    ApplyLine(refs, R"({"protocol": 1, "event": "request_done", "key": "0-2", "reference": "/refs/0-2.png", "cached": true})", 0.0);
    ApplyLine(refs, R"({"protocol": 1, "event": "request_started", "key": "8-1"})", 0.0);
    CHECK(refs.items[0].state == JobItemState::Done);
    CHECK(refs.items[1].state == JobItemState::Generating);
    ApplyLine(refs, R"({"protocol": 1, "event": "request_done", "key": "8-1", "reference": "/refs/8-1.png", "cached": false})", 0.0);
    ApplyLine(refs, R"({"protocol": 1, "event": "finished", "dir": "/refs", "rendered": 1, "cached": 1, "failed": 0})", 0.0);
    CHECK(refs.phase == JobPhase::Finished);
    CHECK(refs.rendered == 1);
    CHECK(refs.cached == 1);
    CHECK(refs.Progress() == 1.0f);
}

TEST_CASE("An item's concept listing: variants, lineage, pick and batches newest first [editor][concepts]")
{
    ConceptListing listing;
    REQUIRE(ParseConceptListing(LISTING, listing));
    CHECK(listing.ok);
    CHECK(listing.key == "6-0");
    CHECK(listing.name == "Small Shield");
    CHECK(listing.referenceExists);
    REQUIRE(listing.pick.has_value());
    CHECK(listing.pick->variant == "v1");
    REQUIRE(listing.variants.size() == 3);
    CHECK(listing.variants[0].imageCostActual == doctest::Approx(0.0235));
    CHECK_FALSE(listing.variants[0].parent.has_value());
    const ConceptVariant& refined = listing.variants[2];
    REQUIRE(refined.parent.has_value());
    CHECK(refined.parent->variant == "v1");
    CHECK(refined.picked);
    CHECK(refined.note == "more rivets");
    CHECK(LineageText(refined) == "20260923-130336-study-top-10/6-0/v1");

    const std::vector<ConceptBatchGroup> shown = GroupByBatch(listing, false);
    REQUIRE(shown.size() == 2);
    CHECK(shown[0].batch == "20260923-160000-refine"); // newest first
    CHECK(shown[0].kind == "refine");
    CHECK(shown[1].variants == std::vector<std::size_t>{0}); // v2 is discarded
    CHECK(GroupByBatch(listing, true)[1].variants == std::vector<std::size_t>{0, 1});

    ConceptListing broken;
    CHECK_FALSE(ParseConceptListing("{}", broken));
    std::string error;
    CHECK(CommandSucceeded(R"({"protocol": 1, "command": "pick", "ok": true})", error));
    CHECK_FALSE(CommandSucceeded(R"({"protocol": 1, "command": "pick", "ok": false, "error": "no variant v9"})", error));
    CHECK(error == "no variant v9");
}

TEST_CASE("Concept summaries for Browse, by every item key of a concept [editor][concepts]")
{
    ConceptSummaries summaries;
    REQUIRE(ParseConceptSummaries(R"({"protocol": 1, "command": "list", "ok": true,
        "items": [{"key": "8-1", "keys": ["7-1", "8-1"], "variants": 4, "discarded": 1, "picked": false},
                  {"key": "6-0", "keys": ["6-0"], "variants": 3, "discarded": 0, "picked": true}],
        "batches": [{"batch": "b1", "running": true}, {"batch": "b0", "running": false}]})",
                                  summaries));
    REQUIRE(summaries.byItemKey.count("7-1") == 1);
    CHECK(summaries.byItemKey["7-1"].key == "8-1");
    CHECK(SummaryLabel(summaries.byItemKey["7-1"]) == "3 concepts");
    CHECK(SummaryLabel(summaries.byItemKey["6-0"]) == "picked");
    CHECK(SummaryLabel({"1-0", 1, 1, false}) == "all discarded");
    CHECK(SummaryLabel({"1-0", 1, 0, false}) == "1 concept");
    CHECK(summaries.runningBatches == std::vector<std::string>{"b1"});
}

TEST_CASE("Browse selection: click, Cmd-click, Shift-click, select all and clear [editor][concepts]")
{
    const std::vector<int> shown = {10, 11, 12, 13, 14};
    ItemSelection selection;
    CHECK(selection.Primary() == ItemSelection::NONE);
    selection.Click(11, {}, shown);
    CHECK(selection.Types() == std::vector<int>{11});
    CHECK(selection.Primary() == 11);

    selection.Click(13, ClickModifiers{true, false}, shown); // Cmd-click adds
    CHECK(selection.Types() == std::vector<int>{11, 13});
    CHECK(selection.Primary() == 13);
    selection.Click(13, ClickModifiers{true, false}, shown); // and takes out again
    CHECK(selection.Types() == std::vector<int>{11});
    CHECK(selection.Primary() == 11);

    selection.Click(11, {}, shown);
    selection.Click(14, ClickModifiers{false, true}, shown); // Shift: the range from the anchor
    CHECK(selection.Types() == std::vector<int>{11, 12, 13, 14});
    CHECK(selection.Primary() == 14);
    selection.Click(10, ClickModifiers{false, true}, shown); // the anchor stays 11
    CHECK(selection.Types() == std::vector<int>{10, 11});

    selection.Toggle(12);
    CHECK(selection.Contains(12));
    CHECK(selection.Count() == 3);

    selection.Clear();
    CHECK(selection.Count() == 0);
    CHECK(selection.Primary() == 12); // the details panel keeps showing it
    selection.SelectAll(shown);
    CHECK(selection.Count() == shown.size());
    CHECK(selection.Primary() == 12);
    selection.SelectOnly(10);
    CHECK(selection.Types() == std::vector<int>{10});

    ItemSelection hidden;
    hidden.Click(99, {}, shown); // an item no longer shown: Shift from it selects just the clicked one
    hidden.Click(12, ClickModifiers{false, true}, shown);
    CHECK(hidden.Types() == std::vector<int>{12});
}
