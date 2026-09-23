#pragma once

#ifdef _EDITOR

#include <string>
#include <vector>

// The model files a map loads for its object types. Every map but Lorencia loads type t
// from Data/Object{N}/Object{t + 1}.bmd (Object01.bmd, ..., Object160.bmd). Lorencia
// (Data/Object1) names its files instead, in a table written into
// CMapManager::LoadWorld (Tree01.bmd is type 0, House01.bmd type 115, ...). Its types are
// below 160, so a copy that renames each file to the generic name of its type loads the
// same model for the same type, and Lorencia's object file needs no change.
namespace Editor::NewMap
{
// The first type the generic loader fills and the first it does not.
constexpr int FIRST_MODEL_TYPE = 0;   // MODEL_WORLD_OBJECT
constexpr int MODEL_TYPE_LIMIT = 160; // MAX_WORLD_OBJECTS

// Types first to first + count - 1 load baseName01.bmd, baseName02.bmd, ...
struct NamedModelRun
{
    int firstType = 0;
    const char* baseName = "";
    int count = 0;
};

// Lorencia's table, as CMapManager::LoadWorld loads it.
const std::vector<NamedModelRun>& LorenciaModels();

// "Object01.bmd" ... "Object160.bmd": the file the generic loader reads for `type`.
std::string GenericModelFile(int type);

// "Tree01.bmd": the file of the `index`-th model (from 0) of a run.
std::string NamedModelFile(const NamedModelRun& run, int index);

// The Data/Object folder that loads models by name instead of by number (Lorencia's).
constexpr int NAMED_MODEL_FOLDER = 1;
} // namespace Editor::NewMap

#endif // _EDITOR
