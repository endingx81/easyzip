#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace EasyZip
{
    class EncodingHelper
    {
    public:
        static bool IsValidUtf8(const char* data, size_t length);
        static std::wstring DecodeSmart(const char* data, size_t length, UINT fallbackCodepage = 949);
        static std::wstring DecodeSmart(const std::string& str, UINT fallbackCodepage = 949);

        static UINT GetDefaultCodepage();
        static std::string GetDefaultCodepageName();
        static UINT NameToCodepage(const std::string& name);
        static std::string CodepageToName(UINT cp);
    };
}

