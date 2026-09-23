#pragma once

#ifdef _EDITOR

#include "ModelHotReload.h"

#include <memory>
#include <string>
#include <vector>

class BMD;

// A second copy of a loaded model, read from other files, that the Item Editor
// draws next to the model the client shows (A/B side by side). Models[type] is
// left alone: while one picture is drawn, ScopedModelSwap puts the copy's
// meshes, bones, actions and textures into Models[type], so the engine's own item
// and character code (which finds a model only by its type) draws the copy with
// every effect, and puts the client's model back right after.
//
// The copy is checked like a hot reload (CheckFiles: a broken file is a message,
// never a crash) and its textures get bitmap indices of their own, even for a file
// the client already shows: a hot reload of the client's model never changes the
// copy, and the copy never keeps a texture the client uses alive.
namespace Editor::Assets::HotReload
{
class ModelCopy
{
public:
    explicit ModelCopy(int type);
    ~ModelCopy();
    ModelCopy(const ModelCopy&) = delete;
    ModelCopy& operator=(const ModelCopy&) = delete;

    int Type() const { return m_type; }
    BMD& Model() { return *m_model; }

private:
    friend std::unique_ptr<ModelCopy> LoadCopy(const Request& request, std::string& message);

    int m_type;
    std::unique_ptr<BMD> m_model;
    std::vector<unsigned int> m_textures; // bitmap indices this copy loaded
};

// Loads `request` (a model type the hot reload may reload) as a copy. Null, with
// `message` saying why, when the files are refused or do not load; else
// `message` says what was loaded.
std::unique_ptr<ModelCopy> LoadCopy(const Request& request, std::string& message);

// Frees `copy` at the start of the frame after the next (HotReload::RunPending):
// pictures of this frame and of the next may still use its model and textures.
void RetireCopy(std::unique_ptr<ModelCopy> copy);

// Frees the copies whose time has come. HotReload::RunPending calls it at the
// start of every frame.
void ReleaseRetiredCopies();

// Frees every retired copy now (the editor shuts down; no picture is drawn any more).
void ReleaseAllCopies();

// True for a bitmap index a live copy loaded (the hot reload never reuses one).
bool IsCopyTexture(unsigned int bitmapIndex);

// While it lives, Models[copy.Type()] holds each copy's model data (and the copy
// the client's). Keep it to one picture's draw; nothing may reload the model
// meanwhile.
class ScopedModelSwap
{
public:
    explicit ScopedModelSwap(const std::vector<ModelCopy*>& copies);
    ~ScopedModelSwap();
    ScopedModelSwap(const ScopedModelSwap&) = delete;
    ScopedModelSwap& operator=(const ScopedModelSwap&) = delete;

private:
    std::vector<ModelCopy*> m_copies;
};
} // namespace Editor::Assets::HotReload

#endif // _EDITOR
