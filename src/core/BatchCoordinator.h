#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace EasyZip
{
    class BatchCoordinator
    {
    public:
        // Returns true if this process is the primary instance and should continue with collectedPaths.
        // Returns false if this process was a secondary instance and has forwarded its path to the primary.
        static bool Coordinate(
            const std::wstring& mode,
            const std::wstring& initialPath,
            std::vector<std::wstring>& outPaths,
            DWORD quietTimeoutMs = 250,
            DWORD maxWaitMs = 1500);
    };
}

