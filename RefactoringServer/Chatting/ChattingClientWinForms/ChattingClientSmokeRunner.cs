using System.Text;
using ChattingClientWinForms.Models;
using ChattingClientWinForms.Networking;

namespace ChattingClientWinForms;

internal static class ChattingClientSmokeRunner
{
    private sealed record SmokeOptions(
        string LoginServerBaseUrl,
        string Host,
        int Port,
        byte PacketKey,
        int TimeoutSeconds,
        string? LoginId,
        string? Password,
        bool SkipRegister);

    public static bool IsSmokeMode(string[] args)
    {
        return args.Any(argument => string.Equals(argument, "--smoke", StringComparison.OrdinalIgnoreCase));
    }

    public static async Task<int> RunAsync(string[] args)
    {
        SmokeOptions options = ParseArguments(args);
        string logDirectory = Path.Combine(AppContext.BaseDirectory, "Smoke");
        Directory.CreateDirectory(logDirectory);
        string logPath = Path.Combine(logDirectory, $"chatting-client-smoke-{DateTime.Now:yyyyMMdd_HHmmss}.log");

        void Log(string message)
        {
            string line = $"[{DateTime.Now:HH:mm:ss}] {message}";
            try
            {
                Console.WriteLine(line);
            }
            catch
            {
            }

            File.AppendAllText(logPath, line + Environment.NewLine, new UTF8Encoding(false));
        }

        Log("Smoke mode started.");
        Log($"Log file: {logPath}");

        try
        {
            using CancellationTokenSource cancellationTokenSource = new(TimeSpan.FromSeconds(options.TimeoutSeconds));
            CancellationToken cancellationToken = cancellationTokenSource.Token;

            string loginId = options.LoginId ?? $"smoke_{DateTime.UtcNow:yyyyMMddHHmmssfff}";
            string password = options.Password ?? "test1234!";

            AuthServerSettings authSettings = new(options.LoginServerBaseUrl);
            AuthApiClient authApiClient = new();
            await using ChattingTcpClient client = new();

            TaskCompletionSource<LoginResult> loginTcs = CreateTcs<LoginResult>();
            TaskCompletionSource<IReadOnlyList<ChatRoomInfo>> roomListTcs = CreateTcs<IReadOnlyList<ChatRoomInfo>>();
            TaskCompletionSource<RoomChangeResult> roomChangeTcs = CreateTcs<RoomChangeResult>();
            TaskCompletionSource<ChattingResult> chattingTcs = CreateTcs<ChattingResult>();

            client.SystemMessageReceived += message => Log($"Client: {message}");
            client.ConnectionStateChanged += connected => Log($"ConnectionStateChanged: connected={connected}");
            client.LoginResultReceived += result =>
            {
                Log($"LoginResultReceived: userId={result.UserId}, success={result.Success}");
                loginTcs.TrySetResult(result);
            };
            client.RoomListReceived += rooms =>
            {
                Log($"RoomListReceived: roomCount={rooms.Count}");
                roomListTcs.TrySetResult(rooms);
            };
            client.RoomChangeResultReceived += result =>
            {
                Log($"RoomChangeResultReceived: previous={result.PreviousRoomId}, current={result.CurrentRoomId}, success={result.Success}, resultCode={result.ResultCode}");
                roomChangeTcs.TrySetResult(result);
            };
            client.ChattingResultReceived += result =>
            {
                Log($"ChattingResultReceived: success={result.Success}");
                chattingTcs.TrySetResult(result);
            };
            client.BroadcastReceived += message =>
            {
                Log($"BroadcastReceived: roomId={message.RoomId}, sender={message.SenderUserId}, bytes={message.Payload.Length}");
            };

            if (!options.SkipRegister)
            {
                try
                {
                    RegisterAccountResponse registerResponse = await authApiClient.RegisterAsync(
                        authSettings,
                        new RegisterAccountRequest(loginId, password, loginId),
                        cancellationToken);
                    Log($"RegisterAsync succeeded: userId={registerResponse.UserId}, nickname={registerResponse.Nickname}");
                }
                catch (AuthApiException exception) when (string.Equals(exception.ErrorCode, "LOGIN_ID_ALREADY_EXISTS", StringComparison.Ordinal))
                {
                    Log($"RegisterAsync skipped because loginId already exists: {loginId}");
                }
            }

            LoginAccountResponse loginResponse = await authApiClient.LoginAsync(
                authSettings,
                new LoginAccountRequest(loginId, password),
                cancellationToken);
            Log($"LoginAsync succeeded: userId={loginResponse.UserId}, chatServer={loginResponse.ChatServer.Ip}:{loginResponse.ChatServer.Port}, ticketExpiresIn={loginResponse.TicketExpiresInSeconds}s");

            string host = string.IsNullOrWhiteSpace(options.Host) ? loginResponse.ChatServer.Ip : options.Host;
            int port = options.Port > 0 ? options.Port : loginResponse.ChatServer.Port;

            await client.ConnectAsync(
                new ClientConnectionSettings(host, port, options.PacketKey),
                cancellationToken);
            await client.SendLoginAuthAsync(loginResponse.Ticket, cancellationToken);

            LoginResult loginResult = await loginTcs.Task.WaitAsync(cancellationToken);
            if (!loginResult.Success)
            {
                throw new InvalidOperationException("ChattingServer rejected LoginAuth.");
            }

            await client.SendRoomListAsync(cancellationToken);
            IReadOnlyList<ChatRoomInfo> rooms = await roomListTcs.Task.WaitAsync(cancellationToken);
            ChatRoomInfo targetRoom = rooms.FirstOrDefault(static room => room.Joinable) ??
                throw new InvalidOperationException("No joinable room returned from RoomListRp.");

            await client.SendRoomChangeAsync(targetRoom.RoomId, cancellationToken);
            RoomChangeResult roomChangeResult = await roomChangeTcs.Task.WaitAsync(cancellationToken);
            if (!roomChangeResult.Success)
            {
                throw new InvalidOperationException($"RoomChangeRp failed. resultCode={roomChangeResult.ResultCode}");
            }

            string chatText = $"smoke-message-{DateTime.UtcNow:yyyyMMddHHmmss}";
            await client.SendChattingAsync(
                roomChangeResult.CurrentRoomId,
                clientMessageId: 1,
                sentTick: unchecked((ulong)Environment.TickCount64),
                text: chatText,
                cancellationToken: cancellationToken);

            ChattingResult chattingResult = await chattingTcs.Task.WaitAsync(cancellationToken);
            if (!chattingResult.Success)
            {
                throw new InvalidOperationException("ChattingRp returned success=false.");
            }

            await client.DisconnectAsync("Smoke completed successfully.");
            Log("Smoke completed successfully.");
            return 0;
        }
        catch (Exception exception)
        {
            Log($"Smoke failed: {exception}");
            return 1;
        }
    }

    private static TaskCompletionSource<T> CreateTcs<T>()
    {
        return new TaskCompletionSource<T>(TaskCreationOptions.RunContinuationsAsynchronously);
    }

    private static SmokeOptions ParseArguments(string[] args)
    {
        string loginServerBaseUrl = "http://127.0.0.1:18080";
        string host = string.Empty;
        int port = 0;
        byte packetKey = 55;
        int timeoutSeconds = 30;
        string? loginId = null;
        string? password = null;
        bool skipRegister = false;

        for (int index = 0; index < args.Length; ++index)
        {
            string argument = args[index];
            if (string.Equals(argument, "--smoke", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            if (string.Equals(argument, "--login-server", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                loginServerBaseUrl = args[++index];
                continue;
            }

            if (string.Equals(argument, "--host", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                host = args[++index];
                continue;
            }

            if (string.Equals(argument, "--port", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                port = int.Parse(args[++index]);
                continue;
            }

            if (string.Equals(argument, "--packet-key", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                packetKey = byte.Parse(args[++index]);
                continue;
            }

            if (string.Equals(argument, "--timeout-seconds", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                timeoutSeconds = int.Parse(args[++index]);
                continue;
            }

            if (string.Equals(argument, "--login-id", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                loginId = args[++index];
                continue;
            }

            if (string.Equals(argument, "--password", StringComparison.OrdinalIgnoreCase) && index + 1 < args.Length)
            {
                password = args[++index];
                continue;
            }

            if (string.Equals(argument, "--skip-register", StringComparison.OrdinalIgnoreCase))
            {
                skipRegister = true;
                continue;
            }

            throw new InvalidOperationException($"Unknown argument: {argument}");
        }

        return new SmokeOptions(loginServerBaseUrl, host, port, packetKey, timeoutSeconds, loginId, password, skipRegister);
    }
}
