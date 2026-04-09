using System.Text;
using ChattingClientWinForms.Models;
using ClientNetworkLib.CSharp;
using Generated.Packets;
using Generated.Packets.Chatting;
using Generated.Packets.Login;

namespace ChattingClientWinForms.Networking;

internal sealed class ChattingTcpClient : IAsyncDisposable
{
    private readonly ContentTcpClient m_client = new(new GeneratedPacketRegistry());

    public ChattingTcpClient()
    {
        m_client.SystemMessageReceived += message => SystemMessageReceived?.Invoke(message);
        m_client.ConnectionStateChanged += connected => ConnectionStateChanged?.Invoke(connected);
        m_client.PacketReceived += HandlePacketReceived;
    }

    public event Action<string>? SystemMessageReceived;

    public event Action<bool>? ConnectionStateChanged;

    public event Action<LoginResult>? LoginResultReceived;

    public event Action<IReadOnlyList<ChatRoomInfo>>? RoomListReceived;

    public event Action<RoomChangeResult>? RoomChangeResultReceived;

    public event Action<ChattingResult>? ChattingResultReceived;

    public event Action<BroadcastMessage>? BroadcastReceived;

    public bool IsConnected => m_client.IsConnected;

    public Task ConnectAsync(ClientConnectionSettings settings, CancellationToken cancellationToken = default)
    {
        if (settings == null)
        {
            throw new ArgumentNullException(nameof(settings));
        }

        return m_client.ConnectAsync(
            new ClientConnectionOptions(settings.Host, settings.Port, settings.PacketKey),
            cancellationToken);
    }

    public Task DisconnectAsync(string reason = "Disconnected.")
    {
        return m_client.DisconnectAsync(reason);
    }

    public Task SendLoginAsync(uint userId, CancellationToken cancellationToken = default)
    {
        return m_client.SendPacketAsync(new LoginRqPacket
        {
            UserId = userId
        }, cancellationToken);
    }

    public Task SendLoginAuthAsync(string ticket, CancellationToken cancellationToken = default)
    {
        return m_client.SendPacketAsync(new LoginAuthRqPacket
        {
            Ticket = ticket ?? string.Empty
        }, cancellationToken);
    }

    public Task SendRoomListAsync(CancellationToken cancellationToken = default)
    {
        return m_client.SendPacketAsync(new RoomListRqPacket(), cancellationToken);
    }

    public Task SendRoomChangeAsync(uint targetRoomId, CancellationToken cancellationToken = default)
    {
        return m_client.SendPacketAsync(new RoomChangeRqPacket
        {
            TargetRoomId = targetRoomId
        }, cancellationToken);
    }

    public Task SendChattingAsync(
        uint roomId,
        ulong clientMessageId,
        ulong sentTick,
        string text,
        CancellationToken cancellationToken = default)
    {
        return m_client.SendPacketAsync(new ChattingRqPacket
        {
            RoomId = roomId,
            ClientMessageId = clientMessageId,
            SentTick = sentTick,
            Payload = Encoding.UTF8.GetBytes(text ?? string.Empty)
        }, cancellationToken);
    }

    public async ValueTask DisposeAsync()
    {
        await m_client.DisposeAsync().ConfigureAwait(false);
    }

    private void HandlePacketReceived(PacketReceivedEventArgs eventArgs)
    {
        switch (eventArgs.Packet)
        {
        case LoginRpPacket loginPacket:
            LoginResultReceived?.Invoke(new LoginResult(loginPacket.UserId, loginPacket.Success));
            return;
        case LoginAuthRpPacket loginAuthPacket:
            LoginResultReceived?.Invoke(new LoginResult(loginAuthPacket.UserId, loginAuthPacket.Success));
            return;
        case RoomListRpPacket roomListPacket:
            if (TryCreateRoomList(roomListPacket, out IReadOnlyList<ChatRoomInfo>? rooms))
            {
                RoomListReceived?.Invoke(rooms);
            }
            else
            {
                EmitSystemMessage("Failed to map RoomListRp packet to UI model.");
            }

            return;
        case RoomChangeRpPacket roomChangePacket:
            RoomChangeResultReceived?.Invoke(new RoomChangeResult(
                roomChangePacket.PreviousRoomId,
                roomChangePacket.CurrentRoomId,
                roomChangePacket.Success,
                MapRoomFlowResultCode(roomChangePacket.ResultCode)));
            return;
        case ChattingRpPacket chattingPacket:
            ChattingResultReceived?.Invoke(new ChattingResult(chattingPacket.Success));
            return;
        case BroadcastBroadcastPacket broadcastPacket:
            BroadcastReceived?.Invoke(new BroadcastMessage(
                broadcastPacket.RoomId,
                broadcastPacket.SenderUserId,
                broadcastPacket.MessageId,
                broadcastPacket.SentTick,
                broadcastPacket.Payload));
            return;
        default:
            EmitSystemMessage($"Unhandled opcode received: {eventArgs.Opcode}");
            return;
        }
    }

    private bool TryCreateRoomList(RoomListRpPacket packet, out IReadOnlyList<ChatRoomInfo> rooms)
    {
        rooms = Array.Empty<ChatRoomInfo>();

        int roomCount = packet.RoomIds.Count;
        if (roomCount != packet.RoomNames.Count ||
            roomCount != packet.ParticipantCounts.Count ||
            roomCount != packet.Capacities.Count ||
            roomCount != packet.JoinableFlags.Count)
        {
            return false;
        }

        List<ChatRoomInfo> parsedRooms = new List<ChatRoomInfo>(roomCount);
        for (int index = 0; index < roomCount; ++index)
        {
            parsedRooms.Add(new ChatRoomInfo(
                packet.RoomIds[index],
                packet.RoomNames[index],
                packet.ParticipantCounts[index],
                packet.Capacities[index],
                packet.JoinableFlags[index] != 0));
        }

        rooms = parsedRooms;
        return true;
    }

    private static RoomFlowResultCode MapRoomFlowResultCode(ushort resultCode)
    {
        return Enum.IsDefined(typeof(RoomFlowResultCode), resultCode)
            ? (RoomFlowResultCode)resultCode
            : RoomFlowResultCode.InternalError;
    }

    private void EmitSystemMessage(string message)
    {
        SystemMessageReceived?.Invoke(message);
    }
}
