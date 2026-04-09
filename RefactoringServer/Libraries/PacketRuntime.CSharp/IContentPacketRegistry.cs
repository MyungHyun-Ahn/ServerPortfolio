namespace PacketRuntime.CSharp
{
    public interface IContentPacketRegistry
    {
        IContentPacket? CreatePacket(ushort opcode);
    }
}
