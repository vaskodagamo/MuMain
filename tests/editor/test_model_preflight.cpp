#include "App/stdafx.h"

#include <doctest.h>

#include "TempTree.h"

#include "Assets/AssetCatalog.h"
#include "Assets/AssetVariant.h"
#include "Assets/CaptureImage.h"
#include "Assets/EditorText.h"
#include "Assets/FileDigest.h"
#include "Assets/ModelPreflight.h"
#include "Render/Terrain/ZzzLodTerrain.h" // MapFileDecrypt, for the shipped models

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace Editor::Assets;
using EditorTest::TempTree;
using EditorTest::WriteText;

namespace
{
using Bytes = std::vector<std::uint8_t>;

// The engine's limits (ZzzBMD.h MAX_MESH / MAX_VERTICES / MAX_BONES, CGlobalBitmap's 1024,
// BITMAP_t::FileName) as the hot reload passes them.
ModelLimits EngineLimits()
{
    ModelLimits limits;
    limits.maxMeshes = 50;
    limits.maxVertices = 15000;
    limits.maxBones = 200;
    limits.maxTextureSize = 1024;
    limits.maxPathBytes = 255;
    return limits;
}

void PutInt16(Bytes& out, int value)
{
    const auto v = static_cast<std::int16_t>(value);
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(v));
}

void PutName(Bytes& out, const std::string& name)
{
    Bytes field(BMD_NAME_BYTES, 0);
    std::memcpy(field.data(), name.data(), std::min(name.size(), field.size()));
    out.insert(out.end(), field.begin(), field.end());
}

void PutZeros(Bytes& out, std::size_t count)
{
    out.insert(out.end(), count, 0);
}

void SetInt16(Bytes& bytes, std::size_t offset, int value)
{
    const auto v = static_cast<std::int16_t>(value);
    std::memcpy(&bytes[offset], &v, sizeof(v));
}

// Triangle_t inside a triangle record: corners, then vertex, normal and texture-coordinate indices.
constexpr std::size_t VERTEX_INDEX_OFFSET = 2;
constexpr std::size_t NORMAL_INDEX_OFFSET = 10;
constexpr std::size_t TEXCOORD_INDEX_OFFSET = 18;
constexpr std::size_t INDEX_BYTES = 2;

// A plain (decrypted) one-mesh model the way BMD::Open2 reads it; each field can be
// broken by a test.
struct ModelSpec
{
    int meshes = 1;
    int bones = 1;
    int actions = 1;
    int vertices = 3;
    int textureSlot = -1; // -1: the mesh's own slot
    int corners = 3;
    int cornerVertex = 2; // the last corner's vertex index
    int keys = 1;
    int boneParent = -1;
    std::string texture = "tree.jpg";
};

Bytes BuildModel(const ModelSpec& spec)
{
    Bytes out;
    PutName(out, "Test01.smd");
    PutInt16(out, spec.meshes);
    PutInt16(out, spec.bones);
    PutInt16(out, spec.actions);
    for (int mesh = 0; mesh < spec.meshes; ++mesh)
    {
        PutInt16(out, spec.vertices); // vertices
        PutInt16(out, 3);             // normals
        PutInt16(out, 3);             // texture coordinates
        PutInt16(out, 1);             // triangles
        PutInt16(out, spec.textureSlot < 0 ? mesh : spec.textureSlot);
        PutZeros(out, spec.vertices * BMD_VERTEX_BYTES + 3 * BMD_NORMAL_BYTES + 3 * BMD_TEXCOORD_BYTES);
        Bytes triangle(BMD_TRIANGLE_BYTES, 0);
        triangle[0] = static_cast<std::uint8_t>(spec.corners);
        for (int corner = 0; corner < 3; ++corner)
        {
            SetInt16(triangle, VERTEX_INDEX_OFFSET + corner * INDEX_BYTES, corner == 2 ? spec.cornerVertex : corner);
            SetInt16(triangle, NORMAL_INDEX_OFFSET + corner * INDEX_BYTES, corner);
            SetInt16(triangle, TEXCOORD_INDEX_OFFSET + corner * INDEX_BYTES, corner);
        }
        out.insert(out.end(), triangle.begin(), triangle.end());
        PutName(out, spec.texture);
    }
    for (int action = 0; action < spec.actions; ++action)
    {
        PutInt16(out, spec.keys);
        out.push_back(0); // no locked positions
    }
    for (int bone = 0; bone < spec.bones; ++bone)
    {
        out.push_back(0); // a real bone
        PutName(out, "Bone01");
        PutInt16(out, spec.boneParent);
        PutZeros(out, spec.actions * 2 * std::max(spec.keys, 0) * BMD_KEY_BYTES);
    }
    return out;
}

bool Check(const Bytes& model, ModelSummary& summary, std::string& error)
{
    return CheckModelBytes(model.data(), model.size(), EngineLimits(), summary, error);
}

bool Accepts(const ModelSpec& spec)
{
    ModelSummary summary;
    std::string error;
    return Check(BuildModel(spec), summary, error);
}

void WriteBytes(const fs::path& file, const Bytes& bytes)
{
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
}

Bytes ReadBytes(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

// An .OZT: 4 prefix bytes, an 18-byte TGA header, then the pixels.
Bytes BuildOzt(int width, int height, std::uint8_t bits, std::uint8_t imageType, std::size_t pixelBytes)
{
    Bytes out(4 + 18, 0);
    out[4 + 2] = imageType;
    out[4 + 12] = static_cast<std::uint8_t>(width & 0xFF);
    out[4 + 13] = static_cast<std::uint8_t>(width >> 8);
    out[4 + 14] = static_cast<std::uint8_t>(height & 0xFF);
    out[4 + 15] = static_cast<std::uint8_t>(height >> 8);
    out[4 + 16] = bits;
    PutZeros(out, pixelBytes);
    return out;
}

// An .OZJ: 24 prefix bytes and a real JPEG of a grey frame.
Bytes BuildOzj(std::uint32_t width, std::uint32_t height)
{
    mu::FramePixels frame;
    frame.width = width;
    frame.height = height;
    frame.rgb.assign(static_cast<std::size_t>(width) * height * 3, 128);
    Bytes out(24, 0);
    const Bytes jpeg = Editor::Capture::EncodeJpeg(frame, Editor::Capture::CAPTURE_JPEG_QUALITY);
    out.insert(out.end(), jpeg.begin(), jpeg.end());
    return out;
}

bool TextureAccepted(const fs::path& file, TextureKind kind, TextureFile& texture)
{
    std::string error;
    return CheckTextureContainer(file, kind, EngineLimits().maxTextureSize, texture, error);
}
} // namespace

TEST_CASE("BMD container header: plain and encrypted models, refused versions and sizes")
{
    BmdEnvelope envelope;
    std::string error;
    Bytes plain = {'B', 'M', 'D', BMD_VERSION_PLAIN, 1, 2, 3};
    REQUIRE(ReadBmdEnvelope(plain, envelope, error));
    CHECK(envelope.payloadOffset == 4);
    CHECK(envelope.payloadSize == 3);
    CHECK_FALSE(envelope.IsEncrypted());

    Bytes encrypted = {'B', 'M', 'D', BMD_VERSION_ENCRYPTED, 2, 0, 0, 0, 9, 9, 9};
    REQUIRE(ReadBmdEnvelope(encrypted, envelope, error));
    CHECK(envelope.IsEncrypted());
    CHECK(envelope.payloadOffset == 8);
    CHECK(envelope.payloadSize == 2);

    encrypted[4] = 4; // claims more bytes than the file has
    CHECK_FALSE(ReadBmdEnvelope(encrypted, envelope, error));
    CHECK_FALSE(ReadBmdEnvelope({'B', 'M', 'D', 0x0E, 0, 0, 0, 0}, envelope, error)); // Open2 cannot read 0x0E
    CHECK(error.find("14") != std::string::npos);
    CHECK_FALSE(ReadBmdEnvelope({'X', 'M', 'D', BMD_VERSION_PLAIN}, envelope, error));
    CHECK_FALSE(ReadBmdEnvelope({'B', 'M'}, envelope, error));
}

TEST_CASE("model bytes: a valid model passes and names its textures")
{
    ModelSpec spec;
    spec.meshes = 2;
    ModelSummary summary;
    std::string error;
    REQUIRE_MESSAGE(Check(BuildModel(spec), summary, error), error);
    CHECK(summary.meshes == 2);
    CHECK(summary.bones == 1);
    CHECK(summary.actions == 1);
    CHECK(summary.meshTextures == std::vector<std::string>{"tree.jpg", "tree.jpg"});
}

TEST_CASE("model bytes: counts and indices the engine would overflow are refused")
{
    const ModelLimits limits = EngineLimits();
    const auto refuses = [](auto change)
    {
        ModelSpec spec;
        change(spec);
        return !Accepts(spec);
    };
    CHECK(refuses([](ModelSpec& s) { s.meshes = 0; }));
    CHECK(refuses([&](ModelSpec& s) { s.meshes = limits.maxMeshes + 1; }));
    CHECK(refuses([&](ModelSpec& s) { s.bones = limits.maxBones + 1; }));
    CHECK(refuses([](ModelSpec& s) { s.actions = -1; }));
    CHECK(refuses([&](ModelSpec& s) { s.vertices = limits.maxVertices + 1; }));
    CHECK(refuses([](ModelSpec& s) { s.textureSlot = 1; })); // one mesh: slot 0 only
    CHECK(refuses(
        [](ModelSpec& s)
        {
            s.meshes = 2;
            s.textureSlot = 0; // the second mesh borrows the first mesh's slot
        }));
    CHECK(refuses([](ModelSpec& s) { s.corners = 5; }));
    CHECK(refuses([](ModelSpec& s) { s.cornerVertex = 3; })); // 3 vertices: 0..2
    CHECK(refuses([](ModelSpec& s) { s.keys = 0; }));
    CHECK(refuses([](ModelSpec& s) { s.boneParent = 1; })); // one bone: -1 or 0
    CHECK(refuses([](ModelSpec& s) { s.boneParent = -2; }));
    CHECK_FALSE(refuses([](ModelSpec& s) { s.bones = 0; }));
}

TEST_CASE("model bytes: every cut-short copy is refused without reading past it")
{
    const Bytes model = BuildModel(ModelSpec{});
    for (std::size_t size = 0; size < model.size(); ++size)
    {
        const Bytes cut(model.begin(), model.begin() + static_cast<std::ptrdiff_t>(size));
        ModelSummary summary;
        std::string error;
        CHECK_FALSE(Check(cut, summary, error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("texture names: kinds, hidden meshes and name matching like the client")
{
    CHECK(TextureKindOf("tree.jpg") == TextureKind::Jpeg);
    CHECK(TextureKindOf("Bark.JPG") == TextureKind::Jpeg);
    CHECK(TextureKindOf("leaf.tga") == TextureKind::Tga);
    CHECK(TextureKindOf("hide_mesh.jpg") == TextureKind::Hidden);
    CHECK(TextureKindOf("HIDDEN") == TextureKind::Hidden);
    CHECK(TextureKindOf("photo.jpeg") == TextureKind::Unsupported);
    CHECK(TextureKindOf("map.bmp") == TextureKind::Unsupported);
    CHECK(TextureKindOf("") == TextureKind::Unsupported);
    CHECK(SameTextureName("Tree_a.tga", "tree_A.TGA"));
    CHECK_FALSE(SameTextureName("tree.jpg", "tree2.jpg"));
}

TEST_CASE("texture files: found without regard to case, missing ones reported")
{
    TempTree tree("mu_editor_preflight_find");
    WriteBytes(tree.Root() / "TREE.ozj", BuildOzj(8, 8));
    const fs::path found = FindTextureContainer(tree.Root(), "tree.jpg");
    REQUIRE_FALSE(found.empty());
    CHECK(fs::equivalent(found, tree.Root() / "TREE.ozj")); // the spelling depends on the file system
    CHECK(FindTextureContainer(tree.Root(), "tree.tga").empty());
    CHECK(FindTextureContainer(tree.Root(), "bark.jpg").empty());
    CHECK(FindTextureContainer(tree.Root() / "absent", "tree.jpg").empty());
}

TEST_CASE("texture files: .OZT headers and sizes the client can and cannot load")
{
    TempTree tree("mu_editor_preflight_ozt");
    const fs::path file = tree.Root() / "leaf.OZT";
    TextureFile texture;

    WriteBytes(file, BuildOzt(4, 2, 32, 2, 4 * 2 * 4));
    REQUIRE(TextureAccepted(file, TextureKind::Tga, texture));
    CHECK(texture.width == 4);
    CHECK(texture.height == 2);

    WriteBytes(file, BuildOzt(4, 2, 24, 2, 4 * 2 * 4));
    CHECK_FALSE(TextureAccepted(file, TextureKind::Tga, texture));
    WriteBytes(file, BuildOzt(4, 2, 32, 10, 4 * 2 * 4)); // run-length encoded
    CHECK_FALSE(TextureAccepted(file, TextureKind::Tga, texture));
    WriteBytes(file, BuildOzt(4, 2, 32, 2, 4 * 2 * 4 - 1)); // the pixels are cut short
    CHECK_FALSE(TextureAccepted(file, TextureKind::Tga, texture));
    WriteBytes(file, BuildOzt(2048, 1, 32, 2, 2048 * 4));
    CHECK_FALSE(TextureAccepted(file, TextureKind::Tga, texture));
    Bytes withId = BuildOzt(4, 2, 32, 2, 4 * 2 * 4);
    withId[4] = 3; // an image ID the loader would read as pixels
    WriteBytes(file, withId);
    CHECK_FALSE(TextureAccepted(file, TextureKind::Tga, texture));
    WriteBytes(file, Bytes(10, 0));
    CHECK_FALSE(TextureAccepted(file, TextureKind::Tga, texture));
}

TEST_CASE("texture files: .OZJ must hold a readable JPEG of at most 1024 pixels")
{
    TempTree tree("mu_editor_preflight_ozj");
    const fs::path file = tree.Root() / "tree.OZJ";
    TextureFile texture;

    WriteBytes(file, BuildOzj(64, 32));
    REQUIRE(TextureAccepted(file, TextureKind::Jpeg, texture));
    CHECK(texture.width == 64);
    CHECK(texture.height == 32);

    WriteBytes(file, BuildOzj(1025, 8));
    CHECK_FALSE(TextureAccepted(file, TextureKind::Jpeg, texture));
    WriteBytes(file, Bytes(24, 0)); // the prefix only
    CHECK_FALSE(TextureAccepted(file, TextureKind::Jpeg, texture));
    WriteBytes(file, Bytes(200, 7));
    CHECK_FALSE(TextureAccepted(file, TextureKind::Jpeg, texture));
    CHECK_FALSE(TextureAccepted(tree.Root() / "absent.OZJ", TextureKind::Jpeg, texture));
}

TEST_CASE("model textures: each distinct texture once, every problem listed")
{
    TempTree tree("mu_editor_preflight_textures");
    WriteBytes(tree.Root() / "tree.OZJ", BuildOzj(16, 16));
    WriteBytes(tree.Root() / "leaf.OZT", BuildOzt(2, 2, 32, 2, 16));
    ModelSummary model;
    model.meshTextures = {"tree.jpg", "TREE.JPG", "leaf.tga", "hid_marker", "map.bmp", "gone.jpg"};
    std::vector<std::string> problems;
    const std::vector<TextureFile> textures = CheckModelTextures(tree.Root(), model, EngineLimits(), problems);

    REQUIRE(textures.size() == 5); // TREE.JPG is tree.jpg
    CHECK(textures[0].container.filename() == "tree.OZJ");
    CHECK(textures[0].width == 16);
    CHECK(textures[1].kind == TextureKind::Tga);
    CHECK(textures[2].kind == TextureKind::Hidden);
    CHECK(textures[2].container.empty());
    REQUIRE(problems.size() == 2);
    CHECK(problems[0].find("map.bmp") != std::string::npos);
    CHECK(problems[1].find("gone.OZJ is missing") != std::string::npos);

    ModelLimits tight = EngineLimits();
    tight.maxPathBytes = 8;
    problems.clear();
    CheckModelTextures(tree.Root(), ModelSummary{1, 0, 0, {"tree.jpg"}}, tight, problems);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].find("longer") != std::string::npos);
}

TEST_CASE("the shipped Lorencia models and their textures pass the preflight")
{
#ifdef MU_REPO_ROOT
    const fs::path folder = fs::path(MU_REPO_ROOT) / "src" / "bin" / "Data" / "Object1";
    int models = 0;
    for (const auto& entry : fs::directory_iterator(folder))
    {
        if (entry.path().extension() != ".bmd")
            continue;
        Bytes file = ReadBytes(entry.path());
        BmdEnvelope envelope;
        std::string error;
        REQUIRE_MESSAGE(ReadBmdEnvelope(file, envelope, error), entry.path().string());
        Bytes model(envelope.payloadSize);
        if (envelope.IsEncrypted())
            MapFileDecrypt(model.data(), file.data() + envelope.payloadOffset, (int)envelope.payloadSize);
        else
            std::memcpy(model.data(), file.data() + envelope.payloadOffset, model.size());
        ModelSummary summary;
        CHECK_MESSAGE(CheckModelBytes(model.data(), model.size(), EngineLimits(), summary, error),
                      (entry.path().string() + ": " + error));
        std::vector<std::string> problems;
        CheckModelTextures(folder, summary, EngineLimits(), problems);
        CHECK_MESSAGE(problems.empty(), entry.path().string());
        ++models;
    }
    CHECK(models >= 100);
#endif
}

TEST_CASE("A/B variants: folders, model paths and the materialize command")
{
    const fs::path repo = fs::path("repo");
    CHECK(VariantDataRoot(repo, AssetVariant::Current) == repo / "src" / "bin" / "Data");
    CHECK(VariantDataRoot(repo, AssetVariant::Original) == repo / "out" / "ab" / "original" / "Data");
    const auto file = VariantFile(repo, AssetVariant::Original, "src/bin/Data/Object1/Tree01.bmd");
    REQUIRE(file.has_value());
    CHECK(*file == (repo / "out" / "ab" / "original" / "Data" / "Object1" / "Tree01.bmd").make_preferred());
    CHECK_FALSE(VariantFile(repo, AssetVariant::Current, "assets-work/World1/Tree01.bmd").has_value());
    CHECK_FALSE(VariantFile(repo, AssetVariant::Current, "src/bin/Data/../secret.bmd").has_value());
    CHECK_FALSE(VariantFile(repo, AssetVariant::Current, "src/bin/Data/").has_value());
    // The script by its full path, so the command runs from any folder.
    const fs::path checkout = fs::path("/work") / "My Repo";
    const std::string script =
        Editor::Text::PathToUtf8((checkout / "tools" / "world_editor" / "materialize_variant.py").make_preferred());
#ifdef _WIN32
    CHECK(MaterializeCommand(checkout, AssetVariant::Original, 1) == "py -3 \"" + script + "\" original --world 1");
#else
    CHECK(MaterializeCommand(checkout, AssetVariant::Original, 1) == "python3 \"" + script + "\" original --world 1");
#endif
    CHECK(std::string(VariantName(AssetVariant::Current)) == "current");
}

TEST_CASE("A/B variants: the original files are missing, stale or ready")
{
    TempTree tree("mu_editor_variant_state");
    const fs::path repo = tree.Root();
    WriteText(CatalogFile(repo, 1), "{\"schema\": \"mu-world-catalog/1\"}\n");
    const std::string digest = Editor::Files::Sha256Hex(CatalogFile(repo, 1));
    const fs::path manifest = repo / "out" / "ab" / "original" / "manifest.json";

    CHECK(CheckVariant(repo, AssetVariant::Current, 1) == VariantState::Ready);
    CHECK(CheckVariant(repo, AssetVariant::Original, 1) == VariantState::Missing);
    WriteText(manifest, "{\"schema\": \"mu-ab-variant/1\", \"world\": 1, \"catalog_sha256\": \"" + digest + "\"}");
    CHECK(CheckVariant(repo, AssetVariant::Original, 1) == VariantState::Ready);
    CHECK(CheckVariant(repo, AssetVariant::Original, 3) == VariantState::Missing);
    WriteText(manifest, "{\"schema\": \"mu-ab-variant/1\", \"world\": 1, \"catalog_sha256\": \"00\"}");
    CHECK(CheckVariant(repo, AssetVariant::Original, 1) == VariantState::OutOfDate);
    WriteText(manifest, "not json");
    CHECK(CheckVariant(repo, AssetVariant::Original, 1) == VariantState::Missing);
}
