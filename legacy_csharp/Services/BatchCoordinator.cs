using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EasyZip.Services
{
    public static class BatchCoordinator
    {
        private const string PipePrefix = "EasyZip_Batch_Pipe_";
        private const string MutexPrefix = "EasyZip_Batch_Mutex_";

        /// <summary>
        /// 윈도우 탐색기에서 파일 여러 개를 선택하고 우클릭 실행 시,
        /// 탐색기가 각 파일마다 프로세스를 따로 실행하는 특성을 단일 배치로 취합합니다.
        /// 보조 인스턴스는 경로를 주 인스턴스에 전송하고 즉시 null을 반환하여 종료하며,
        /// 주 인스턴스는 일정 시간(디바운스 250ms) 동안 수신된 모든 파일 목록을 반환합니다.
        /// </summary>
        public static List<string>? CoordinateBatch(string mode, string initialPath, int quietTimeoutMs = 250, int maxWaitMs = 2000)
        {
            if (string.IsNullOrWhiteSpace(initialPath))
                return new List<string>();

            // 세션별 격리를 위해 사용자명 해시 포함
            string sessionId = Environment.UserName.Replace('\\', '_').Replace('/', '_');
            string mutexName = $@"{MutexPrefix}{mode}_{sessionId}";
            string pipeName = $@"{PipePrefix}{mode}_{sessionId}";

            Mutex? mutex = null;
            bool isPrimary = false;

            try
            {
                mutex = new Mutex(true, mutexName, out isPrimary);
            }
            catch (AbandonedMutexException)
            {
                isPrimary = true;
            }

            if (!isPrimary)
            {
                // 보조 인스턴스: 주 인스턴스 파이프로 자신의 파일 경로 전송
                bool sent = false;
                for (int retry = 0; retry < 5; retry++)
                {
                    try
                    {
                        using var client = new NamedPipeClientStream(".", pipeName, PipeDirection.Out);
                        client.Connect(300);
                        using var writer = new StreamWriter(client, Encoding.UTF8);
                        writer.WriteLine(initialPath);
                        writer.Flush();
                        sent = true;
                        break;
                    }
                    catch
                    {
                        Thread.Sleep(40);
                    }
                }

                if (sent)
                {
                    // 주 인스턴스에 전달 완료 -> 이 프로세스는 종료해야 함
                    return null;
                }

                // 만약 파이프 통신 실패 시 단독으로 진행할 수 있도록 fallback
                return new List<string> { initialPath };
            }

            // 주 인스턴스 (Primary Coordinator)
            var collected = new List<string> { initialPath };
            using (mutex)
            {
                using var cts = new CancellationTokenSource(maxWaitMs);
                DateTime lastReceived = DateTime.UtcNow;

                var listenerTask = Task.Run(async () =>
                {
                    while (!cts.IsCancellationRequested)
                    {
                        try
                        {
                            using var server = new NamedPipeServerStream(
                                pipeName,
                                PipeDirection.In,
                                NamedPipeServerStream.MaxAllowedServerInstances,
                                PipeTransmissionMode.Byte,
                                PipeOptions.Asynchronous);

                            await server.WaitForConnectionAsync(cts.Token);

                            using var reader = new StreamReader(server, Encoding.UTF8);
                            string? line = await reader.ReadLineAsync();
                            if (!string.IsNullOrWhiteSpace(line))
                            {
                                lock (collected)
                                {
                                    if (!collected.Contains(line))
                                    {
                                        collected.Add(line);
                                    }
                                    lastReceived = DateTime.UtcNow;
                                }
                            }
                        }
                        catch
                        {
                            if (cts.IsCancellationRequested) break;
                        }
                    }
                }, cts.Token);

                // 디바운스 대기: 마지막 수신 후 quietTimeoutMs 동안 추가 유입이 없으면 취합 완료
                while (!cts.IsCancellationRequested)
                {
                    Thread.Sleep(30);
                    TimeSpan silence;
                    lock (collected)
                    {
                        silence = DateTime.UtcNow - lastReceived;
                    }

                    if (silence.TotalMilliseconds >= quietTimeoutMs)
                    {
                        break;
                    }
                }

                cts.Cancel();
                try { listenerTask.Wait(100); } catch { }
            }

            return collected;
        }
    }
}

