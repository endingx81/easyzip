#include "BatchCoordinator.h"
#include <chrono>

namespace EasyZip
{
    static std::wstring GetSessionId()
    {
        wchar_t user[256] = { 0 };
        DWORD userLen = _countof(user);
        if (!GetUserNameW(user, &userLen))
        {
            wcsncpy_s(user, L"DefaultUser", _TRUNCATE);
        }
        for (wchar_t* p = user; *p; ++p)
        {
            if (*p == L'\\' || *p == L'/' || *p == L':') *p = L'_';
        }
        return user;
    }

    bool BatchCoordinator::Coordinate(
        const std::wstring& mode,
        const std::wstring& initialPath,
        std::vector<std::wstring>& outPaths,
        DWORD quietTimeoutMs,
        DWORD maxWaitMs)
    {
        if (initialPath.empty())
        {
            return true;
        }

        std::wstring sess = GetSessionId();
        std::wstring mutexName = L"Global\\EasyZip_Batch_Mutex_" + mode + L"_" + sess;
        std::wstring pipeName = L"\\\\.\\pipe\\EasyZip_Batch_Pipe_" + mode + L"_" + sess;

        HANDLE hMutex = CreateMutexW(NULL, TRUE, mutexName.c_str());
        DWORD dwErr = GetLastError();

        if (hMutex == NULL || dwErr == ERROR_ALREADY_EXISTS)
        {
            // Secondary instance: send initialPath to primary pipe
            bool sent = false;
            for (int retry = 0; retry < 8; ++retry)
            {
                HANDLE hPipe = CreateFileW(
                    pipeName.c_str(),
                    GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL);

                if (hPipe != INVALID_HANDLE_VALUE)
                {
                    DWORD bytesWritten = 0;
                    DWORD dataLen = static_cast<DWORD>((initialPath.length() + 1) * sizeof(wchar_t));
                    WriteFile(hPipe, initialPath.c_str(), dataLen, &bytesWritten, NULL);
                    CloseHandle(hPipe);
                    sent = true;
                    break;
                }
                Sleep(25);
            }

            if (hMutex) CloseHandle(hMutex);

            if (sent)
            {
                // Forwarded successfully to primary -> this instance can safely exit
                return false;
            }

            // Fallback if pipe connection failed: run alone
            outPaths.push_back(initialPath);
            return true;
        }

        // Primary instance
        outPaths.clear();
        outPaths.push_back(initialPath);

        HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        auto startTime = std::chrono::steady_clock::now();

        DWORD currentWaitMs = 150; // default 150ms debounce
        if (quietTimeoutMs > 0 && quietTimeoutMs < currentWaitMs) currentWaitMs = quietTimeoutMs;

        while (true)
        {
            auto now = std::chrono::steady_clock::now();
            auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            if (totalElapsed >= maxWaitMs)
            {
                break;
            }

            HANDLE hPipe = CreateNamedPipeW(
                pipeName.c_str(),
                PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                PIPE_UNLIMITED_INSTANCES,
                4096,
                4096,
                50,
                NULL);

            if (hPipe == INVALID_HANDLE_VALUE)
            {
                break;
            }

            ResetEvent(hEvent);
            OVERLAPPED ol = { 0 };
            ol.hEvent = hEvent;

            BOOL connected = ConnectNamedPipe(hPipe, &ol);
            if (!connected)
            {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING)
                {
                    DWORD waitRes = WaitForSingleObject(hEvent, currentWaitMs);
                    if (waitRes == WAIT_OBJECT_0)
                    {
                        connected = TRUE;
                    }
                    else
                    {
                        CancelIo(hPipe);
                        CloseHandle(hPipe);
                        break; // No more clients within quiet timeout
                    }
                }
                else if (err == ERROR_PIPE_CONNECTED)
                {
                    connected = TRUE;
                }
            }

            if (connected)
            {
                wchar_t buf[MAX_PATH * 2] = { 0 };
                DWORD bytesRead = 0;
                OVERLAPPED readOl = { 0 };
                readOl.hEvent = hEvent;
                ResetEvent(hEvent);

                if (ReadFile(hPipe, buf, sizeof(buf) - sizeof(wchar_t), &bytesRead, &readOl) ||
                    GetLastError() == ERROR_IO_PENDING)
                {
                    GetOverlappedResult(hPipe, &readOl, &bytesRead, TRUE);
                    if (bytesRead > 0 && wcslen(buf) > 0)
                    {
                        outPaths.push_back(buf);
                    }
                }
                DisconnectNamedPipe(hPipe);
            }

            CloseHandle(hPipe);
        }

        CloseHandle(hEvent);
        CloseHandle(hMutex);
        return true;
    }
}
