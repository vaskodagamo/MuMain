#pragma once

#ifdef _EDITOR

#include <string>

// Item Data Saving Operations
class ItemDataSaver
{
public:
    // Writes the table in the record layout it was loaded from (see
    // ItemFileSnapshot); items nobody edited keep their loaded bytes.
    static bool Save(wchar_t* fileName, std::string* outChangeLog = nullptr);

private:
    template <typename TFile> static bool SaveAs(wchar_t* fileName, std::string* outChangeLog);
};

#endif // _EDITOR
