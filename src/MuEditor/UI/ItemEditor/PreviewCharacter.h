#pragma once

#ifdef _EDITOR

#include "Editing/PreviewOutfit.h"

#include <memory>

class CHARACTER;

// The character the Item Editor preview dresses for its "Equipped" view. It is a
// CHARACTER of its own, never one of the game's (CharactersClient, the hidden
// hero), the way the game's character photo (CUIPhotoViewer) keeps its own: the
// game's characters are never changed. It is dressed, animated and drawn by the
// engine's own character code (SetPlayerStop, MoveCharacter, RenderCharacter),
// so hands, back, fly pose and set parts are exactly the game's.
namespace Editor::ItemEditor
{
// The +level, excellent and ancient look given to every item the character wears.
struct ItemLook
{
    int level = 0;
    int excellentFlags = 0;
    int ancientDiscriminator = 0;

    bool operator==(const ItemLook&) const = default;
};

class CPreviewCharacter
{
public:
    CPreviewCharacter();
    ~CPreviewCharacter();
    CPreviewCharacter(const CPreviewCharacter&) = delete;
    CPreviewCharacter& operator=(const CPreviewCharacter&) = delete;

    // Dresses the character as `characterClass` in `outfit` (item types), standing
    // at `position`; `safeZone` puts weapons on the back as in a town.
    void Dress(const Preview::Outfit& outfit, CLASS_TYPE characterClass, const ItemLook& look,
               const float position[3], bool safeZone);

    // Advances the animation one frame and draws the character with the current
    // camera. Call inside an open offscreen capture.
    void AnimateAndRender();

    // Takes everything off (cloth, equipment effects) until the next Dress().
    void Release();

    bool IsDressed() const { return m_dressed; }

private:
    std::unique_ptr<CHARACTER> m_character; // made by the first Dress(), kept until the end
    bool m_dressed = false;
};
} // namespace Editor::ItemEditor

#endif // _EDITOR
