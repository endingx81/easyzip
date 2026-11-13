#include "EncodingHelper.h"
#include <vector>

namespace EasyZip
{
    bool EncodingHelper::IsValidUtf8(const char* data, size_t length)
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data);
        size_t i = 0;
        bool hasMultiByte = false;

        while (i < length)
        {
            if (bytes[i] <= 0x7F)
            {
                i++;
            }
            else if ((bytes[i] & 0xE0) == 0xC0)
            {
                if (i + 1 >= length || (bytes[i + 1] & 0xC0) != 0x80) return false;
                hasMultiByte = true;
                i += 2;
            }
            else if ((bytes[i] & 0xF0) == 0xE0)
            {
                if (i + 2 >= length ||
                    (bytes[i + 1] & 0xC0) != 0x80 ||
                    (bytes[i + 2] & 0xC0) != 0x80) return false;
                hasMultiByte = true;
                i += 3;
            }
            else if ((bytes[i] & 0xF8) == 0xF0)
            {
                if (i + 3 >= length ||
                    (bytes[i + 1] & 0xC0) != 0x80 ||
                    (bytes[i + 2] & 0xC0) != 0x80 ||
                    (bytes[i + 3] & 0xC0) != 0x80) return false;
                hasMultiByte = true;
                i += 4;
            }
            else
            {
                return false;
            }
        }
        return hasMultiByte;
    }

    std::wstring EncodingHelper::DecodeSmart(const char* data, size_t length, UINT fallbackCodepage)
    {
        if (!data || length == 0) return L"";

        // Check if string is ASCII only
        bool isAsciiOnly = true;
        for (size_t i = 0; i < length; ++i)
        {
            if (static_cast<unsigned char>(data[i]) > 0x7F)
            {
                isAsciiOnly = false;
                break;
            }
        }

        if (isAsciiOnly)
        {
            int len = MultiByteToWideChar(CP_ACP, 0, data, static_cast<int>(length), NULL, 0);
            if (len > 0)
            {
                std::vector<wchar_t> buf(len);
                MultiByteToWideChar(CP_ACP, 0, data, static_cast<int>(length), buf.data(), len);
                return std::wstring(buf.data(), len);
            }
        }

        // Test if valid UTF-8
        if (IsValidUtf8(data, length))
        {
            int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(length), NULL, 0);
            if (len > 0)
            {
                std::vector<wchar_t> buf(len);
                MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(length), buf.data(), len);
                return std::wstring(buf.data(), len);
            }
        }

        // Fallback to Korean CP949 / CP_ACP
        int len = MultiByteToWideChar(fallbackCodepage, 0, data, static_cast<int>(length), NULL, 0);
        if (len > 0)
        {
            std::vector<wchar_t> buf(len);
            MultiByteToWideChar(fallbackCodepage, 0, data, static_cast<int>(length), buf.data(), len);
            return std::wstring(buf.data(), len);
        }

        // Final fallback to CP_ACP
        len = MultiByteToWideChar(CP_ACP, 0, data, static_cast<int>(length), NULL, 0);
        if (len > 0)
        {
            std::vector<wchar_t> buf(len);
            MultiByteToWideChar(CP_ACP, 0, data, static_cast<int>(length), buf.data(), len);
            return std::wstring(buf.data(), len);
        }

        return L"";
    }

    std::wstring EncodingHelper::DecodeSmart(const std::string& str, UINT fallbackCodepage)
    {
        return DecodeSmart(str.data(), str.length(), fallbackCodepage);
    }

    UINT EncodingHelper::GetDefaultCodepage()
    {
        UINT acp = GetACP();
        if (acp == 0 || acp == 65001) acp = 949; // Default Korean CP949 on UTF-8 systems
        return acp;
    }

    std::string EncodingHelper::GetDefaultCodepageName()
    {
        return CodepageToName(GetDefaultCodepage());
    }

    UINT EncodingHelper::NameToCodepage(const std::string& name)
    {
        if (name == "UTF-8" || name == "utf-8" || name == "CP65001") return CP_UTF8;
        if (name == "CP949" || name == "cp949" || name == "EUC-KR" || name == "euc-kr") return 949;
        if (name == "CP932" || name == "cp932" || name == "Shift_JIS" || name == "sjis") return 932;
        if (name == "CP936" || name == "cp936" || name == "GBK" || name == "gb2312") return 936;
        if (name == "CP950" || name == "cp950" || name == "Big5") return 950;
        if (name == "CP1252" || name == "cp1252" || name == "Windows-1252") return 1252;

        if (name.rfind("CP", 0) == 0 && name.length() > 2)
        {
            try { return static_cast<UINT>(std::stoul(name.substr(2))); } catch (...) {}
        }
        return GetDefaultCodepage();
    }

    std::string EncodingHelper::CodepageToName(UINT cp)
    {
        if (cp == CP_UTF8 || cp == 65001) return "UTF-8";
        if (cp == 949) return "CP949";
        if (cp == 932) return "CP932";
        if (cp == 936) return "CP936";
        if (cp == 950) return "CP950";
        if (cp == 1252) return "CP1252";
        return "CP" + std::to_string(cp);
    }
}

