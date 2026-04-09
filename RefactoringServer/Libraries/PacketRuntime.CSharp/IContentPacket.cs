namespace PacketRuntime.CSharp
{
    public interface IContentPacket
    {
        ushort Opcode { get; }

        void Serialize(PacketWriter writer);

        bool Deserialize(PacketReader reader);
    }
}
