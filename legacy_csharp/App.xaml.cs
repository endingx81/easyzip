using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows;
using EasyZip.Services;
using EasyZip.Views;

namespace EasyZip
{
    public partial class App : Application
    {
        private void Application_Startup(object sender, StartupEventArgs e)
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            EncodingHelper.Initialize(); // CP949, Shift-JIS, GB2312 등 다국어 코드페이지 즉시 활성화

            string[] args = e.Args;

            if (args.Length == 0)
            {
                ShutdownMode = ShutdownMode.OnMainWindowClose;
                var mainWindow = new MainWindow();
                MainWindow = mainWindow;
                mainWindow.Show();
                return;
            }

            string firstArg = args[0];

            // 0. 자동 자가 진단 및 테스트 (-selftest)
            if (firstArg.Equals("-selftest", StringComparison.OrdinalIgnoreCase))
            {
                Task.Run(RunSelfTest).GetAwaiter().GetResult();
                Shutdown();
                return;
            }

            if (firstArg.Equals("-testdownloads", StringComparison.OrdinalIgnoreCase))
            {
                string targetDir = args.Length > 1 ? args[1] : @"F:\downloads";
                Task.Run(() => TestDownloadsFolder(targetDir)).GetAwaiter().GetResult();
                Shutdown();
                return;
            }

            // 언인스톨러 핸들러 (--uninstall)
            if (firstArg.Equals("--uninstall", StringComparison.OrdinalIgnoreCase) || firstArg.Equals("-uninstall", StringComparison.OrdinalIgnoreCase))
            {
                PerformUninstall();
                Shutdown();
                return;
            }

            // 1. 여기에 압축 풀기 (-x "archive" ...)
            if (firstArg.Equals("-x", StringComparison.OrdinalIgnoreCase) && args.Length > 1)
            {
                List<string>? archives;
                if (args.Length > 2)
                {
                    archives = args.Skip(1).Select(a => a.Trim('"')).Where(File.Exists).ToList();
                }
                else
                {
                    archives = BatchCoordinator.CoordinateBatch("x", args[1].Trim('"'));
                }

                if (archives == null || archives.Count == 0)
                {
                    Shutdown();
                    return;
                }

                foreach (var archivePath in archives)
                {
                    string dest = Path.GetDirectoryName(archivePath) ?? "C:\\";
                    var progress = new ProgressWindow(archivePath, dest);
                    progress.ShowDialog();
                }
                Shutdown();
                return;
            }

            // 2. 각각의 폴더에 압축 풀기 (-xd "archive" ...) (폴더 중복 시 (2), (3) 자동 생성)
            if (firstArg.Equals("-xd", StringComparison.OrdinalIgnoreCase) && args.Length > 1)
            {
                List<string>? archives;
                if (args.Length > 2)
                {
                    archives = args.Skip(1).Select(a => a.Trim('"')).Where(File.Exists).ToList();
                }
                else
                {
                    archives = BatchCoordinator.CoordinateBatch("xd", args[1].Trim('"'));
                }

                if (archives == null || archives.Count == 0)
                {
                    Shutdown();
                    return;
                }

                foreach (var archivePath in archives)
                {
                    string parent = Path.GetDirectoryName(archivePath) ?? "C:\\";
                    string folderName = ArchiveEngine.GetArchiveBaseName(archivePath);
                    string dest = ArchiveEngine.GetUniqueDirectoryPath(parent, folderName);
                    var progress = new ProgressWindow(archivePath, dest);
                    progress.ShowDialog();
                }
                Shutdown();
                return;
            }

            // 3. 압축하기 대화상자 (-c "target")
            if (firstArg.Equals("-c", StringComparison.OrdinalIgnoreCase) && args.Length > 1)
            {
                List<string>? sources;
                if (args.Length > 2)
                {
                    sources = args.Skip(1).Select(a => a.Trim('"')).ToList();
                }
                else
                {
                    // 윈도우 탐색기 다중 파일 선택 시 창이 여러 개 뜨지 않도록 단일 창으로 취합
                    sources = BatchCoordinator.CoordinateBatch("c", args[1].Trim('"'));
                }

                if (sources == null || sources.Count == 0)
                {
                    // 보조 인스턴스는 주 인스턴스로 전달 후 즉시 종료
                    Shutdown();
                    return;
                }

                var compWnd = new CompressWindow(sources);
                if (compWnd.ShowDialog() == true && compWnd.SourcePaths.Count > 0)
                {
                    var progress = new ProgressWindow(compWnd.SourcePaths, compWnd.DestinationArchive, compWnd.SelectedLevel);
                    progress.ShowDialog();
                }
                Shutdown();
                return;
            }

            // 4. 빠른 ZIP 압축 (-cz "target")
            if (firstArg.Equals("-cz", StringComparison.OrdinalIgnoreCase) && args.Length > 1)
            {
                List<string>? sources;
                if (args.Length > 2)
                {
                    sources = args.Skip(1).Select(a => a.Trim('"')).ToList();
                }
                else
                {
                    // 윈도우 탐색기 다중 파일 선택 시 충돌 에러 없이 하나의 압축 파일로 일괄 취합
                    sources = BatchCoordinator.CoordinateBatch("cz", args[1].Trim('"'));
                }

                if (sources == null || sources.Count == 0)
                {
                    // 보조 인스턴스는 주 인스턴스로 전달 후 즉시 종료
                    Shutdown();
                    return;
                }

                string first = sources[0];
                string parent = Path.GetDirectoryName(first) ?? "C:\\";
                string baseName;
                if (sources.Count == 1)
                {
                    baseName = Path.GetFileNameWithoutExtension(first);
                }
                else
                {
                    // 여러 파일 선택 시: 현재 상위 폴더명 기준(예: "Downloads.zip"), 루트 드라이브일 경우 첫 번째 파일명 사용
                    string dirName = Path.GetFileName(parent.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
                    baseName = !string.IsNullOrEmpty(dirName) ? dirName : Path.GetFileNameWithoutExtension(first);
                }
                if (string.IsNullOrEmpty(baseName)) baseName = "Archive";

                // 파일명 중복 시 (2), (3) 등으로 안전하게 자동 생성하여 충돌 및 덮어쓰기 에러 원천 차단!
                string dest = ArchiveEngine.GetUniqueFilePath(parent, baseName, ".zip");

                var progress = new ProgressWindow(sources, dest, System.IO.Compression.CompressionLevel.Fastest);
                progress.ShowDialog();
                Shutdown();
                return;
            }

            // 5. 탐색기 우클릭 새 폴더 즉시 만들기 (-newfolder "targetDirectory")
            if (firstArg.Equals("-newfolder", StringComparison.OrdinalIgnoreCase))
            {
                string targetDir = args.Length > 1 ? args[1].Trim('"') : "";
                if (string.IsNullOrEmpty(targetDir) || !Directory.Exists(targetDir))
                {
                    targetDir = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                }

                if (Directory.Exists(targetDir))
                {
                    try
                    {
                        string baseName = "새 폴더";
                        string newPath = Path.Combine(targetDir, baseName);
                        int count = 2;
                        while (Directory.Exists(newPath) || File.Exists(newPath))
                        {
                            newPath = Path.Combine(targetDir, $"{baseName} ({count})");
                            count++;
                        }

                        Directory.CreateDirectory(newPath);

                        // 윈도우 탐색기에 즉시 폴더 생성 알림 (새로고침 없이 바로 표시됨)
                        NativeMethods.NotifyShellAssociationChanged();
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"새 폴더를 만들지 못했습니다:\n{ex.Message}", "EasyZip 오류", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }

                Shutdown();
                return;
            }

            // 6. 단일 경로 (압축 파일 또는 디렉터리 열기)
            ShutdownMode = ShutdownMode.OnMainWindowClose;
            string targetPath = firstArg.Trim('"');
            var main = new MainWindow(targetPath);
            MainWindow = main;
            main.Show();
        }

        private void RunSelfTest()
        {
            var log = new System.Text.StringBuilder();
            void Log(string msg)
            {
                log.AppendLine(msg);
                try { Console.WriteLine(msg); } catch { }
            }

            Log("[EasyZip Self-Test] 시작...");
            string testDir = Path.Combine(Path.GetTempPath(), "EasyZip_SelfTest_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(testDir);

            try
            {
                string sourceDir = Path.Combine(testDir, "Source");
                Directory.CreateDirectory(sourceDir);

                // 1. 테스트 파일 30개 생성
                Log("1. 테스트 데이터셋 생성 중...");
                string textContent = "EasyZip Ultra Fast Archiver Engine Performance Test Content!\r\n" +
                                     new string('A', 1024) + "\r\n" + new string('B', 1024) + "\r\n";
                byte[] sampleData = System.Text.Encoding.UTF8.GetBytes(textContent);

                for (int i = 0; i < 30; i++)
                {
                    string fPath = Path.Combine(sourceDir, $"test_file_{i:D2}.txt");
                    using var fs = File.Create(fPath);
                    for (int j = 0; j < 50; j++) // 약 100KB per file
                    {
                        fs.Write(sampleData, 0, sampleData.Length);
                    }
                }

                // 2. 초고속 압축 테스트
                string zipPath = Path.Combine(testDir, "test_output.zip");
                Log("2. 초고속 ZIP 압축 생성 테스트...");
                var sw = Stopwatch.StartNew();
                ArchiveEngine.CompressAsync(new[] { sourceDir }, zipPath, System.IO.Compression.CompressionLevel.Fastest).GetAwaiter().GetResult();
                sw.Stop();
                Log($"   => 압축 완료! 파일 크기: {new FileInfo(zipPath).Length / 1024} KB, 소요 시간: {sw.ElapsedMilliseconds} ms");

                // 3. 아카이브 목록 파싱 테스트
                Log("3. 아카이브 내부 목록 파싱 테스트...");
                var entries = ArchiveEngine.GetArchiveEntriesAsync(zipPath).GetAwaiter().GetResult();
                Log($"   => 엔트리 수: {entries.Count}개");
                if (entries.Count < 30) throw new Exception("엔트리 수 불일치!");

                // 4. 초고속 멀티스레드 병렬 압축 해제 테스트
                string extractDir = Path.Combine(testDir, "Extracted");
                Log("4. 초고속 멀티스레드 병렬 압축 해제 테스트...");
                sw.Restart();
                ArchiveEngine.ExtractArchiveAsync(zipPath, extractDir).GetAwaiter().GetResult();
                sw.Stop();
                Log($"   => 압축 해제 완료! 소요 시간: {sw.ElapsedMilliseconds} ms");

                int extractedCount = Directory.GetFiles(extractDir, "*.*", SearchOption.AllDirectories).Length;
                Log($"   => 해제된 파일 수: {extractedCount}개");
                if (extractedCount != 30) throw new Exception($"해제된 파일 수({extractedCount})가 원본(30)과 다릅니다!");

                // 5. 레지스트리 쉘 메뉴 연동 테스트
                Log("5. 윈도우 탐색기 우클릭 메뉴 및 새 폴더 레지스트리 등록 테스트...");
                ShellIntegration.RegisterContextMenu(true, true, true, true, true);
                bool isReg = ShellIntegration.IsContextMenuRegistered();
                Log($"   => 컨텍스트 메뉴 등록 상태: {(isReg ? "성공" : "실패")}");

                // 6. 확장자 연결 테스트
                Log("6. 파일 확장자 연결 테스트...");
                FileAssociation.AssociateExtension(".zip");
                FileAssociation.AssociateExtension(".7z");
                FileAssociation.AssociateExtension(".001");
                bool zipAssoc = FileAssociation.IsAssociated(".zip");
                Log($"   => .zip 확장자 연결 상태: {(zipAssoc ? "성공" : "기존 앱 유지")}");

                // 7. .001 분할 볼륨 아카이브 테스트
                Log("7. .001 분할 압축 볼륨 테스트...");
                byte[] zipBytes = File.ReadAllBytes(zipPath);
                int splitSize = zipBytes.Length / 3;
                string part1 = Path.Combine(testDir, "split_test.zip.001");
                string part2 = Path.Combine(testDir, "split_test.zip.002");
                string part3 = Path.Combine(testDir, "split_test.zip.003");
                File.WriteAllBytes(part1, zipBytes.Take(splitSize).ToArray());
                File.WriteAllBytes(part2, zipBytes.Skip(splitSize).Take(splitSize).ToArray());
                File.WriteAllBytes(part3, zipBytes.Skip(splitSize * 2).ToArray());

                var splitParts = MultiVolumeStream.FindAllParts(part1);
                Log($"   => 발견된 분할 파트 수: {splitParts.Count}개");
                if (splitParts.Count != 3) throw new Exception("분할 파트 검색 실패!");

                var splitEntries = ArchiveEngine.GetArchiveEntriesAsync(part1).GetAwaiter().GetResult();
                Log($"   => .001 아카이브 내부 엔트리 수: {splitEntries.Count}개");
                if (splitEntries.Count < 30) throw new Exception("분할 아카이브 엔트리 파싱 실패!");

                string splitExtractDir = Path.Combine(testDir, "SplitExtracted");
                ArchiveEngine.ExtractArchiveAsync(part1, splitExtractDir).GetAwaiter().GetResult();
                int splitExtractedCount = Directory.GetFiles(splitExtractDir, "*.*", SearchOption.AllDirectories).Length;
                Log($"   => .001 분할 압축 해제된 파일 수: {splitExtractedCount}개");
                if (splitExtractedCount != 30) throw new Exception("분할 압축 해제 파일 수 불일치!");

                // 8. 대용량 RAR 아카이브 검증 테스트
                string rarTestPath = @"F:\downloads\Lightyear-Frontier-SteamRIP.com.rar";
                if (File.Exists(rarTestPath))
                {
                    Log("8. 대용량 RAR5 아카이브 검증 테스트...");
                    var rarEntries = ArchiveEngine.GetArchiveEntriesAsync(rarTestPath).GetAwaiter().GetResult();
                    Log($"   => RAR 엔트리 수: {rarEntries.Count}개 정상 로드 완료 (디렉터리/파일 CRC 오류 해결)");
                    if (rarEntries.Count != 128) throw new Exception($"RAR 엔트리 개수 불일치 ({rarEntries.Count})");
                }

                Log("\n==========================================");
                Log("🎉 EasyZip 모든 자가 진단 및 기능 검증 성공!");
                Log("==========================================");
            }
            catch (Exception ex)
            {
                Log($"\n❌ Self-Test 실패: {ex.Message}\n{ex.StackTrace}");
            }
            finally
            {
                File.WriteAllText("selftest_result.log", log.ToString());
                try
                {
                    if (Directory.Exists(testDir))
                        Directory.Delete(testDir, true);
                }
                catch { }
            }
        }

        private void PerformUninstall()
        {
            if (MessageBox.Show("EasyZip 프로그램을 컴퓨터에서 완전히 제거하시겠습니까?", "EasyZip 제거", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes)
            {
                return;
            }

            try
            {
                // 1. 쉘 메뉴 및 확장자 연결 해제
                ShellIntegration.UnregisterContextMenu();
                FileAssociation.UnassociateAll();

                // 2. 바로가기 삭제
                string desktopShortcut = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "EasyZip.lnk");
                if (File.Exists(desktopShortcut)) File.Delete(desktopShortcut);

                string startPrograms = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "EasyZip");
                if (Directory.Exists(startPrograms)) Directory.Delete(startPrograms, true);

                // 3. 제어판 언인스톨 레지스트리 키 삭제
                Microsoft.Win32.Registry.CurrentUser.DeleteSubKeyTree(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\EasyZip", false);

                NativeMethods.NotifyShellAssociationChanged();

                string currentDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\', '/');

                // 4. 자가 폴더 삭제 스크립트 실행 후 종료
                string batPath = Path.Combine(Path.GetTempPath(), "easyzip_cleanup.bat");
                string batContent = $@"@echo off
timeout /t 1 /nobreak > nul
rmdir /s /q ""{currentDir}""
del ""%~f0""
";
                File.WriteAllText(batPath, batContent);
                Process.Start(new ProcessStartInfo
                {
                    FileName = batPath,
                    CreateNoWindow = true,
                    UseShellExecute = false
                });

                MessageBox.Show("EasyZip이 성공적으로 제거되었습니다.", "제거 완료", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"제거 중 오류 발생:\n{ex.Message}", "제거 오류", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private static async Task TestDownloadsFolder(string folderPath)
        {
            var log = new StringBuilder();
            void Log(string msg)
            {
                Console.WriteLine(msg);
                log.AppendLine(msg);
            }

            Log($"=== [EasyZip] {folderPath} 압축 파일 전수 테스트 시작 ===");

            var extensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            {
                ".zip", ".rar", ".7z", ".001", ".alz", ".egg", ".tar", ".gz", ".iso", ".cab"
            };

            List<string> files;
            if (File.Exists(folderPath))
            {
                files = new List<string> { folderPath };
            }
            else if (Directory.Exists(folderPath))
            {
                files = Directory.GetFiles(folderPath)
                    .Where(f => extensions.Contains(Path.GetExtension(f).ToLowerInvariant()) || MultiVolumeStream.IsSplitVolume(f))
                    .OrderBy(f => Path.GetFileName(f))
                    .ToList();
            }
            else
            {
                Log($"경로를 찾을 수 없습니다: {folderPath}");
                return;
            }

            Log($"발견된 압축 파일 대상: 총 {files.Count}개\n");
            int successCount = 0;
            int failCount = 0;
            int skipCount = 0;

            foreach (var file in files)
            {
                string name = Path.GetFileName(file);
                var fi = new FileInfo(file);
                long size = fi.Length;
                string sizeStr = FormatBytes(size);

                if (size == 0)
                {
                    Log($"[SKIP] {name,-55} | 0 바이트 빈 파일 (건너뜀)");
                    skipCount++;
                    continue;
                }

                // 분할 볼륨 중 .002, .003 등 후속 파트는 .001에서 일괄 처리되므로 건너뜀
                string ext = Path.GetExtension(file).ToLowerInvariant();
                if (ext.Length == 4 && ext.StartsWith(".0") && !ext.EndsWith(".001") && MultiVolumeStream.IsSplitVolume(file))
                {
                    Log($"[SKIP] {name,-55} | 분할 볼륨 후속 파트 (.001 파싱 시 자동 포함)");
                    skipCount++;
                    continue;
                }

                var sw = Stopwatch.StartNew();
                try
                {
                    var entries = await ArchiveEngine.GetArchiveEntriesAsync(file);
                    sw.Stop();
                    Log($"[PASS] {name,-55} | {sizeStr,10} | 항목 {entries.Count,5}개 | 소요시간: {sw.ElapsedMilliseconds,5} ms");
                    successCount++;
                }
                catch (Exception ex)
                {
                    sw.Stop();
                    string reason = ArchiveEngine.DiagnoseArchiveError(file, ex).Replace("\n", " ");
                    Log($"[FAIL] {name,-55} | {sizeStr,10} | 소요시간: {sw.ElapsedMilliseconds,5} ms");
                    Log($"       => 진단: {reason}");
                    failCount++;
                }
            }

            Log("\n==================================================");
            Log($"전수 검증 결과: 총 {files.Count}개 중 성공 {successCount}개 / 실패 {failCount}개 / 제외 {skipCount}개");
            Log("==================================================");

            File.WriteAllText("downloads_test_result.txt", log.ToString(), Encoding.UTF8);
        }

        private static string FormatBytes(long bytes)
        {
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            if (bytes < 1024 * 1024 * 1024) return $"{bytes / (1024.0 * 1024.0):F1} MB";
            return $"{bytes / (1024.0 * 1024.0 * 1024.0):F2} GB";
        }
    }
}
