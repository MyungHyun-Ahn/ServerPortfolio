using System;
using PacketRuntime.CSharp;

namespace ClientNetworkLib.CSharp
{
    public sealed class PacketReceivedEventArgs : EventArgs
    {
        public PacketReceivedEventArgs(ushort opcode, IContentPacket? packet, byte[] body)
        {
            Opcode = opcode;
            Packet = packet;
            Body = body ?? Array.Empty<byte>();
        }

        public ushort Opcode { get; }

        public IContentPacket? Packet { get; }

        public byte[] Body { get; }

        public bool IsKnownPacket => Packet != null;

        public TPacket? GetPacket<TPacket>()
            where TPacket : class, IContentPacket
        {
            return Packet as TPacket;
        }
    }
}
