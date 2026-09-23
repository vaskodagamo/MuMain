#include "stdafx.h"

#ifdef _EDITOR

#include "ModelCopy.h"

#include "ModelFileCheck.h"

#include "Assets/EditorText.h"
#include "Core/EditorFiles.h" // PathToUtf8

#include "Core/Globals/_TextureIndex.h"  // BITMAP_HIDE, BITMAP_UNKNOWN
#include "Render/Models/ZzzBMD.h"        // Models, BMD
#include "Render/Sprites/GlobalBitmap.h" // Bitmaps

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Editor::Assets::HotReload
{
namespace
{
constexpr std::string_view PROBLEM_SEPARATOR = "; ";
// A retired copy lives through this many frame starts: the Item Editor's preview
// may still draw it once in the frame after the one it was retired in (its UI
// passed the copy on before the A/B controls changed it).
constexpr int RETIRE_FRAMES = 2;

struct RetiredCopy
{
    std::unique_ptr<ModelCopy> copy;
    int framesLeft = RETIRE_FRAMES;
};

std::vector<RetiredCopy> s_retired;
std::unordered_set<unsigned int> s_copyTextures;

// A model that did not open may hold an index table of zeros; its release must not
// drop references of bitmap 0.
void ForgetTextures(BMD& model)
{
    if (model.IndexTexture != nullptr)
        std::fill_n(model.IndexTexture, model.NumMeshs, static_cast<GLuint>(BITMAP_UNKNOWN));
}

// The copy's texture for `texture`: loaded once into an index of its own, and each
// further mesh that names it takes one more reference (BMD::Release drops one per mesh).
GLuint CopyTexture(const TextureFile& texture, const ModelRange& range, std::unordered_map<std::string, GLuint>& loaded,
                   std::vector<unsigned int>& owned, std::vector<std::string>& problems)
{
    if (texture.kind == TextureKind::Hidden)
        return BITMAP_HIDE;
    const std::wstring path = EngineTexturePath(texture);
    const std::string key = Editor::Text::FoldCase(texture.name);
    if (const auto it = loaded.find(key); it != loaded.end())
        return Bitmaps.LoadImage(it->second, path, range.textureFilter, range.textureWrap) ? it->second : BITMAP_UNKNOWN;
    const GLuint index = Bitmaps.LoadSeparateImage(path, range.textureFilter, range.textureWrap);
    if (index == BITMAP_UNKNOWN)
    {
        problems.push_back(texture.name + " could not be loaded and draws white");
        return index;
    }
    MarkSkinAndHair(index, texture.name);
    loaded.emplace(key, index);
    owned.push_back(index);
    s_copyTextures.insert(index);
    return index;
}

void CopyTextures(BMD& model, const std::vector<TextureFile>& textures, const ModelRange& range,
                  std::vector<unsigned int>& owned, std::vector<std::string>& problems)
{
    std::unordered_map<std::string, GLuint> loaded;
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const std::string name = MeshTextureName(model, i);
        const auto texture = std::find_if(textures.begin(), textures.end(),
                                          [&](const TextureFile& file) { return SameTextureName(file.name, name); });
        model.IndexTexture[i] = texture != textures.end() ? CopyTexture(*texture, range, loaded, owned, problems)
                                                          : static_cast<GLuint>(BITMAP_UNKNOWN);
    }
}

std::string Describe(const Request& request, const CheckedModel& checked, const std::vector<std::string>& problems)
{
    std::string text = request.modelName + ": loaded " + Editor::Files::PathToUtf8(request.bmdFile) + " and " +
                       std::to_string(checked.textures.size()) + " texture(s) for the side-by-side picture.";
    if (!problems.empty())
        text += " " + Editor::Text::Join(problems, PROBLEM_SEPARATOR) + ".";
    return text;
}

// The model data a picture draws; the per-draw state (lights, bone palette, body
// height) stays with Models[type].
void SwapModelData(BMD& a, BMD& b)
{
    std::swap(a.Name, b.Name);
    std::swap(a.Version, b.Version);
    std::swap(a.NumBones, b.NumBones);
    std::swap(a.NumMeshs, b.NumMeshs);
    std::swap(a.NumActions, b.NumActions);
    std::swap(a.Meshs, b.Meshs);
    std::swap(a.Bones, b.Bones);
    std::swap(a.Actions, b.Actions);
    std::swap(a.Textures, b.Textures);
    std::swap(a.IndexTexture, b.IndexTexture);
    std::swap(a.BoneHead, b.BoneHead);
    std::swap(a.StreamMesh, b.StreamMesh);
    // The skinned-vertex scratch arrays belong to whichever model was transformed
    // last; neither may take the other's for its own.
    a.m_SkinStamp = 0;
    b.m_SkinStamp = 0;
}
} // namespace

ModelCopy::ModelCopy(int type) : m_type(type), m_model(std::make_unique<BMD>())
{
}

ModelCopy::~ModelCopy()
{
    for (const unsigned int index : m_textures)
        s_copyTextures.erase(index);
    // ~BMD releases the model and drops one reference per mesh on its textures.
}

std::unique_ptr<ModelCopy> LoadCopy(const Request& request, std::string& message)
{
    const ModelRange* range = FindRange(request.type);
    if (range == nullptr || !CanReload(request.type))
    {
        message = request.modelName + ": type " + std::to_string(request.type) + " is not a loaded model the editor may copy";
        return nullptr;
    }
    CheckedModel checked;
    std::string refusal;
    if (!CheckFiles(request, checked, refusal))
    {
        message = request.modelName + " (" + VariantName(request.variant) + ") not loaded: " + refusal;
        return nullptr;
    }
    auto copy = std::make_unique<ModelCopy>(request.type);
    BMD& model = copy->Model();
    model.m_iBMDSeqID = request.type;
    if (!OpenModelFile(model, request.bmdFile))
    {
        ForgetTextures(model);
        message = request.modelName + ": " + Editor::Files::PathToUtf8(request.bmdFile) +
                  " passed the check but did not load.";
        return nullptr;
    }
    RestoreTuning(model, SaveTuning(::Models[request.type]));
    std::vector<std::string> problems;
    CopyTextures(model, checked.textures, *range, copy->m_textures, problems);
    message = Describe(request, checked, problems);
    return copy;
}

void RetireCopy(std::unique_ptr<ModelCopy> copy)
{
    if (copy)
        s_retired.push_back({std::move(copy), RETIRE_FRAMES});
}

void ReleaseRetiredCopies()
{
    for (RetiredCopy& retired : s_retired)
        --retired.framesLeft;
    std::erase_if(s_retired, [](const RetiredCopy& retired) { return retired.framesLeft <= 0; });
}

void ReleaseAllCopies()
{
    s_retired.clear();
}

bool IsCopyTexture(unsigned int bitmapIndex)
{
    return s_copyTextures.contains(bitmapIndex);
}

ScopedModelSwap::ScopedModelSwap(const std::vector<ModelCopy*>& copies) : m_copies(copies)
{
    for (ModelCopy* copy : m_copies)
        SwapModelData(::Models[copy->Type()], copy->Model());
}

ScopedModelSwap::~ScopedModelSwap()
{
    for (auto copy = m_copies.rbegin(); copy != m_copies.rend(); ++copy)
        SwapModelData(::Models[(*copy)->Type()], (*copy)->Model());
}
} // namespace Editor::Assets::HotReload

#endif // _EDITOR
