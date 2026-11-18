#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include <clocale>
#include "res/resource.h"
#include "PathHelper.h"
#include "AppSettings.h"
#include "ArchiveEngine.h"
#include "BatchCoordinator.h"
#include "FileAssociation.h"
#include "MainWindow.h"
#include "ProgressDlg.h"
#include "CompressDlg.h"

using namespace EasyZip;

static void PerformExtractHere(const std::vector<std::wstring>& archives)
{
    for (const auto& arc : archives)
    {
        if (!PathHelper::FileExists(arc)) continue;

        std::wstring parentDir = PathHelper::GetDirectory(arc);
        ProgressDlg progress(NULL, L"압축 푸는 중 - " + PathHelper::GetFileName(arc));
        progress.SetDestinationFolder(parentDir);
        progress.Show();

        std::wstring errMsg;
        bool ok = ArchiveEngine::ExtractAll(
            arc,
            parentDir,
            [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
            [&]() { return progress.IsCanceled(); },
            &errMsg);

        progress.Close();

        if (!ok && !errMsg.empty() && !progress.IsCanceled())
        {
            MessageBoxW(NULL, (L"압축 풀기 중 오류 또는 경고가 발생했습니다:\n\n" + errMsg).c_str(), L"EasyZip - 압축 풀기 알림", MB_OK | MB_ICONWARNING);
        }
    }
}

static void PerformSmartExtract(const std::vector<std::wstring>& archives)
{
    for (const auto& arc : archives)
    {
        if (!PathHelper::FileExists(arc)) continue;

        std::wstring parentDir = PathHelper::GetDirectory(arc);
        std::wstring baseName = PathHelper::GetArchiveBaseName(arc);

        std::wstring targetDir;
        std::wstring openTarget;
        std::wstring singleFolder;
        if (ArchiveEngine::IsSingleFolderArchive(arc, singleFolder))
        {
            // Already packed in a single root folder -> extract directly into parentDir
            targetDir = parentDir;
            openTarget = PathHelper::Combine(parentDir, singleFolder);
        }
        else
        {
            // Multiple items at root -> create folder named after archive
            targetDir = PathHelper::GetUniqueDirectoryPath(parentDir, baseName);
            openTarget = targetDir;
        }

        ProgressDlg progress(NULL, L"알아서 압축 푸는 중 - " + PathHelper::GetFileName(arc));
        progress.SetDestinationFolder(openTarget);
        progress.Show();

        std::wstring errMsg;
        bool ok = ArchiveEngine::ExtractAll(
            arc,
            targetDir,
            [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
            [&]() { return progress.IsCanceled(); },
            &errMsg);

        progress.Close();

        if (!ok && !errMsg.empty() && !progress.IsCanceled())
        {
            MessageBoxW(NULL, (L"압축 풀기 중 오류 또는 경고가 발생했습니다:\n\n" + errMsg).c_str(), L"EasyZip - 압축 풀기 알림", MB_OK | MB_ICONWARNING);
        }
    }
}

static void PerformExtractToFolder(const std::vector<std::wstring>& archives)
{
    for (const auto& arc : archives)
    {
        if (!PathHelper::FileExists(arc)) continue;

        std::wstring parentDir = PathHelper::GetDirectory(arc);
        std::wstring baseName = PathHelper::GetArchiveBaseName(arc);
        std::wstring targetDir = PathHelper::GetUniqueDirectoryPath(parentDir, baseName);

        ProgressDlg progress(NULL, L"압축 푸는 중 - " + PathHelper::GetFileName(arc));
        progress.SetDestinationFolder(targetDir);
        progress.Show();

        std::wstring errMsg;
        bool ok = ArchiveEngine::ExtractAll(
            arc,
            targetDir,
            [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
            [&]() { return progress.IsCanceled(); },
            &errMsg);

        progress.Close();

        if (!ok && !errMsg.empty() && !progress.IsCanceled())
        {
            MessageBoxW(NULL, (L"압축 풀기 중 오류 또는 경고가 발생했습니다:\n\n" + errMsg).c_str(), L"EasyZip - 압축 풀기 알림", MB_OK | MB_ICONWARNING);
        }
    }
}

static void PerformFastZip(const std::vector<std::wstring>& sources)
{
    if (sources.empty()) return;

    std::wstring parentDir = PathHelper::GetDirectory(sources[0]);
    std::wstring baseName;
    if (sources.size() == 1)
    {
        baseName = PathHelper::GetFileNameWithoutExtension(sources[0]);
    }
    else
    {
        baseName = PathHelper::GetFileName(parentDir);
        if (baseName.empty() || baseName.find(L':') != std::wstring::npos)
        {
            baseName = L"Archive";
        }
    }

    std::wstring destArchive = PathHelper::GetUniqueFilePath(parentDir, baseName, L".zip");

    ProgressDlg progress(NULL, L"빠른 ZIP 압축 - " + PathHelper::GetFileName(destArchive));
    progress.Show();

    std::wstring errMsg;
    ArchiveEngine::Compress(
        sources,
        destArchive,
        L"zip",
        5,
        [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
        [&]() { return progress.IsCanceled(); },
        &errMsg);

    progress.Close();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    // Set system default locale (Korean CP949 / UTF-8)
    std::setlocale(LC_ALL, "");

    // High DPI Support (Per-Monitor V2)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring commandMode;
    std::vector<std::wstring> inputFiles;

    if (argv && argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if (arg == L"-x" || arg == L"--extract-here")
            {
                commandMode = L"extract_here";
            }
            else if (arg == L"-xs" || arg == L"--smart-extract" || arg == L"-s")
            {
                commandMode = L"smart_extract";
            }
            else if (arg == L"-xd" || arg == L"--extract-folder" || arg == L"--extract-each")
            {
                commandMode = L"extract_folder";
            }
            else if (arg == L"-c" || arg == L"--compress")
            {
                commandMode = L"compress";
            }
            else if (arg == L"-cz" || arg == L"--fast-zip")
            {
                commandMode = L"fast_zip";
            }
            else if (arg == L"--newfolder" || arg == L"-newfolder")
            {
                std::wstring targetPath;
                if (i + 1 < argc)
                {
                    targetPath = argv[i + 1];
                }

                // targetPath가 없거나 비어있는 경우 Fallback
                if (targetPath.empty())
                {
                    wchar_t curDir[MAX_PATH] = { 0 };
                    GetCurrentDirectoryW(MAX_PATH, curDir);
                    targetPath = curDir;
                }

                // targetPath가 파일이면 상위 디렉터리 사용
                DWORD attr = GetFileAttributesW(targetPath.c_str());
                std::wstring targetDir = targetPath;
                if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    size_t pos = targetDir.find_last_of(L"\\/");
                    if (pos != std::wstring::npos)
                    {
                        targetDir = targetDir.substr(0, pos);
                    }
                }

                if (targetDir.empty())
                {
                    wchar_t deskPath[MAX_PATH] = { 0 };
                    if (SHGetSpecialFolderPathW(NULL, deskPath, CSIDL_DESKTOPDIRECTORY, FALSE))
                    {
                        targetDir = deskPath;
                    }
                }

                if (!targetDir.empty() && targetDir.back() != L'\\' && targetDir.back() != L'/')
                {
                    targetDir += L'\\';
                }

                std::wstring newFolder = targetDir + L"새 폴더";
                int counter = 2;
                while (PathFileExistsW(newFolder.c_str()))
                {
                    newFolder = targetDir + L"새 폴더 (" + std::to_wstring(counter++) + L")";
                }

                // 디렉터리 생성 및 셸 알림 (새 탐색기 창 절대 열지 않고 조용히 생성!)
                if (CreateDirectoryW(newFolder.c_str(), NULL))
                {
                    SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, newFolder.c_str(), NULL);
                    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, targetDir.c_str(), NULL);
                }

                LocalFree(argv);
                return 0;
            }
            else if (arg == L"--register")
            {
                FileAssociation::RegisterAll();
                LocalFree(argv);
                return 0;
            }
            else if (arg == L"--unregister")
            {
                FileAssociation::UnregisterAll();
                LocalFree(argv);
                return 0;
            }
            else if (!arg.empty() && arg[0] != L'-')
            {
                inputFiles.push_back(arg);
            }
        }
    }
    if (argv) LocalFree(argv);

    // 1. Handle Context Menu Actions with Batch Coordination
    if (!commandMode.empty() && !inputFiles.empty())
    {
        std::vector<std::wstring> batchedPaths;
        bool isPrimary = BatchCoordinator::Coordinate(commandMode, inputFiles[0], batchedPaths);
        if (!isPrimary)
        {
            // Successfully forwarded file to primary instance -> exit
            return 0;
        }

        // Add any additional files passed in the same command line
        for (size_t i = 1; i < inputFiles.size(); ++i)
        {
            batchedPaths.push_back(inputFiles[i]);
        }

        if (commandMode == L"extract_here")
        {
            std::vector<std::wstring> uniqueArchives = PathHelper::FilterMultiVolumeDuplicates(batchedPaths);
            PerformExtractHere(uniqueArchives);
            return 0;
        }
        else if (commandMode == L"smart_extract")
        {
            std::vector<std::wstring> uniqueArchives = PathHelper::FilterMultiVolumeDuplicates(batchedPaths);
            PerformSmartExtract(uniqueArchives);
            return 0;
        }
        else if (commandMode == L"extract_folder")
        {
            std::vector<std::wstring> uniqueArchives = PathHelper::FilterMultiVolumeDuplicates(batchedPaths);
            PerformExtractToFolder(uniqueArchives);
            return 0;
        }
        else if (commandMode == L"fast_zip")
        {
            PerformFastZip(batchedPaths);
            return 0;
        }
        else if (commandMode == L"compress")
        {
            CompressOptions opts;
            if (CompressDlg::Show(NULL, batchedPaths, opts))
            {
                ProgressDlg progress(NULL, L"압축하는 중 - " + PathHelper::GetFileName(opts.destArchivePath));
                progress.Show();

                std::wstring errMsg;
                ArchiveEngine::Compress(
                    opts.sourcePaths,
                    opts.destArchivePath,
                    opts.format,
                    opts.level,
                    [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
                    [&]() { return progress.IsCanceled(); },
                    &errMsg);

                progress.Close();
            }
            return 0;
        }
    }

    // 2. Handle files passed without command switches:
    // If multiple non-archive files are passed, open compress dialog.
    if (inputFiles.size() > 1 && !ArchiveEngine::IsSupportedArchive(inputFiles[0]))
    {
        std::vector<std::wstring> batchedPaths;
        bool isPrimary = BatchCoordinator::Coordinate(L"compress_multi", inputFiles[0], batchedPaths);
        if (!isPrimary) return 0;

        for (size_t i = 1; i < inputFiles.size(); ++i)
        {
            batchedPaths.push_back(inputFiles[i]);
        }

        CompressOptions opts;
        if (CompressDlg::Show(NULL, batchedPaths, opts))
        {
            ProgressDlg progress(NULL, L"압축하는 중 - " + PathHelper::GetFileName(opts.destArchivePath));
            progress.Show();

            std::wstring errMsg;
            ArchiveEngine::Compress(
                opts.sourcePaths,
                opts.destArchivePath,
                opts.format,
                opts.level,
                [&](const ProgressInfo& info) { progress.UpdateProgress(info); },
                [&]() { return progress.IsCanceled(); },
                &errMsg);

            progress.Close();
        }
        return 0;
    }

    // 3. Main GUI Mode
    MainWindow* pMain = MainWindow::Create();
    if (!pMain->Initialize(hInstance, nCmdShow))
    {
        delete pMain;
        return 1;
    }

    // If an archive was passed as argument, open it immediately
    if (!inputFiles.empty() && ArchiveEngine::IsSupportedArchive(inputFiles[0]))
    {
        pMain->OpenArchive(inputFiles[0]);
    }

    // Message Loop
    MSG msg;
    HACCEL hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDA_ACCEL));

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        if (!hAccel || !TranslateAcceleratorW(pMain->GetHwnd(), hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    delete pMain;
    return static_cast<int>(msg.wParam);
}
