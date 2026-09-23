#pragma once

#ifdef _EDITOR

#include <cstddef>
#include <string>

// Item Data Saving Operations
class ItemDataSaver
{
public:
    // Writes the table in the record layout it was loaded from (see
    // ItemFileSnapshot); items nobody edited keep their loaded bytes.
    static bool Save(wchar_t* fileName, std::string* outChangeLog = nullptr);

    // The UTF-8 bytes of a name (without its terminator) the file Save writes can
    // hold: 29 in the legacy layout, 49 in the current one.
    static size_t MaxNameBytes();

private:
    template <typename TFile> static bool SaveAs(wchar_t* fileName, std::string* outChangeLog);
};

#endif // _EDITOR
