#pragma once

#ifdef _EDITOR

#include "MapFileCodec.h"

#include <cstddef>
#include <string>

// A map's three EncTerrain files as the client loads them. Each starts with a version
// byte and the map's folder number: LoadWorld closes the client ("file corrupted") when
// that number is not the folder the file lies in, so a copy into another folder must be
// renumbered.
//   EncTerrain{N}.map: MapFileEncrypt(version, N, layer 1, layer 2, opacity)
//   EncTerrain{N}.att: MapFileEncrypt(BuxConvert(version, N, 255, 255, one or two bytes a tile))
//   EncTerrain{N}.obj: MapFileEncrypt(version, N, record count, records)
namespace Editor::NewMap
{
enum class EncTerrainKind
{
    Mapping,
    Attribute,
    Objects,
};

constexpr std::size_t MAP_NUMBER_OFFSET = 1;

// ".map", ".att" or ".obj".
const char* Extension(EncTerrainKind kind);

// The plain bytes of a file of `kind`, and the file for plain bytes.
Bytes DecodeEncTerrain(EncTerrainKind kind, const Bytes& file, const MapFileCodec& codec);
Bytes EncodeEncTerrain(EncTerrainKind kind, const Bytes& plain, const MapFileCodec& codec);

// True when `plain` has the layout the client loads for `kind`; otherwise the reason.
bool CheckPlainLayout(EncTerrainKind kind, const Bytes& plain, std::string& error);

// `file` with its folder number set to `folder`; `oldFolder` gets the number it held.
// False with the reason when the file does not have the layout the client loads.
bool Renumber(EncTerrainKind kind, const Bytes& file, int folder, const MapFileCodec& codec, Bytes& out, int& oldFolder,
              std::string& error);
} // namespace Editor::NewMap

#endif // _EDITOR
