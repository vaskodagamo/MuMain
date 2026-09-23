#include "stdafx.h"

#ifdef _EDITOR

#include "PreviewCharacter.h"

#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Skills/SummonSystem.h"
#include "World/MapInfra/MapManager.h"

// The engine moves and animates a character with these; the game's character
// photo (CUIPhotoViewer::RenderPhotoCharacter) calls them the same way.
extern void MoveCharacter(CHARACTER* c, OBJECT* o);
extern void MoveCharacterVisual(CHARACTER* c, OBJECT* o);

namespace Editor::ItemEditor
{
namespace
{
// The tile CreateCharacterPointer is given; the position is set right after.
constexpr unsigned char ANY_TILE = 0;
constexpr float FACING_ANGLE = 0.0f;
// The hero's own light on top of the ground's (CreateHero).
constexpr float HERO_LIGHT = 0.3f;

constexpr int BARE_PARTS[Preview::ARMOUR_PART_COUNT] = {MODEL_BODY_HELM, MODEL_BODY_ARMOR, MODEL_BODY_PANTS,
                                                         MODEL_BODY_GLOVES, MODEL_BODY_BOOTS};

int ModelOf(int itemType)
{
    return itemType == Preview::NO_ITEM ? -1 : MODEL_ITEM + itemType;
}

void Wear(PART_t& part, int itemType, const ItemLook& look)
{
    part.Type = static_cast<short>(ModelOf(itemType));
    part.Level = static_cast<BYTE>(look.level);
    part.ExcellentFlags = static_cast<BYTE>(look.excellentFlags);
    part.AncientDiscriminator = static_cast<BYTE>(look.ancientDiscriminator);
}

// The class's bare body (CreateHero), then each armour part of the outfit.
void DressBody(CHARACTER& character, const Preview::Outfit& outfit, const ItemLook& look)
{
    const ItemLook bare;
    for (int part = 0; part < Preview::ARMOUR_PART_COUNT; ++part)
    {
        PART_t& bodyPart = character.BodyPart[BODYPART_HELM + part];
        if (outfit.armour[part] == Preview::NO_ITEM)
        {
            Wear(bodyPart, Preview::NO_ITEM, bare);
            bodyPart.Type = static_cast<short>(BARE_PARTS[part] + character.SkinIndex);
            continue;
        }
        Wear(bodyPart, outfit.armour[part], look);
    }
}

void DressHandsAndBack(CHARACTER& character, const Preview::Outfit& outfit, const ItemLook& look)
{
    const ItemLook plain;
    const bool rightIsPreviewed = outfit.rightHand != ITEM_ARROWS || outfit.wearing == Preview::Wearing::Ammunition;
    const bool leftIsPreviewed = outfit.leftHand != ITEM_BOLT || outfit.wearing == Preview::Wearing::Ammunition;
    Wear(character.Weapon[0], outfit.rightHand, rightIsPreviewed ? look : plain);
    Wear(character.Weapon[1], outfit.leftHand, leftIsPreviewed ? look : plain);
    Wear(character.Wing, outfit.wings, look);
    character.Helper.Type = -1;
}
} // namespace

CPreviewCharacter::CPreviewCharacter() = default;

// The engine may already be gone at exit: only the bones are freed here; the
// editor's shutdown calls Release() while the engine still runs.
CPreviewCharacter::~CPreviewCharacter()
{
    if (m_character)
    {
        delete[] m_character->Object.BoneTransform; // CreateCharacterPointer's allocation
        m_character->Object.BoneTransform = nullptr;
    }
}

void CPreviewCharacter::Dress(const Preview::Outfit& outfit, CLASS_TYPE characterClass, const ItemLook& look,
                              const float position[3], bool safeZone)
{
    Release();
    // One CHARACTER for the whole session: effects the engine made for it keep a
    // pointer to its object, so it is dressed again in place, never freed early.
    if (!m_character)
        m_character = std::make_unique<CHARACTER>();
    CHARACTER& character = *m_character;
    vec34_t* const bones = character.Object.BoneTransform;
    character.Initialize(); // forgets the bones; CreateCharacterPointer replaces them
    character.Object.BoneTransform = bones;
    CreateCharacterPointer(&character, MODEL_PLAYER, ANY_TILE, ANY_TILE, FACING_ANGLE);

    OBJECT& object = character.Object;
    VectorCopy(position, object.Position);
    Vector(HERO_LIGHT, HERO_LIGHT, HERO_LIGHT, object.Light);
    character.Class = characterClass;
    character.SkinIndex = gCharacterManager.GetSkinModelIndex(characterClass);
    character.SafeZone = safeZone;
    character.HideShadow = true;

    DressBody(character, outfit, look);
    DressHandsAndBack(character, outfit, look);
    SetCharacterScale(&character);
    SetPlayerStop(&character);
    m_dressed = true;
}

void CPreviewCharacter::AnimateAndRender()
{
    if (!m_dressed)
        return;
    CHARACTER* character = m_character.get();
    OBJECT* object = &character->Object;
    // The game's characters may still be animating on the pool's threads (the
    // studio does not draw them, so nothing else waits for them).
    WaitCharactersAnimation();
    MoveCharacter(character, object);
    MoveCharacterVisual(character, object);
    RenderCharacter(character, object);
}

void CPreviewCharacter::Release()
{
    if (!m_dressed)
        return;
    OBJECT& object = m_character->Object;
    g_SummonSystem.RemoveEquipEffects(m_character.get());
    DeleteCloth(m_character.get(), &object);
    object.Live = false; // effects that follow it end
    m_dressed = false;
}
} // namespace Editor::ItemEditor

#endif // _EDITOR
