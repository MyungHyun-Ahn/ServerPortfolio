using System.Text;

namespace PacketRuntime.CSharp
{
    public sealed class PacketWriter
    {
        private byte[] m_buffer = new byte[256];
        private int m_length;

        public int Length => m_length;

        public void Reset()
        {
            m_length = 0;
        }

        public byte[] ToArray()
        {
            if (m_length == 0)
            {
                return Array.Empty<byte>();
            }

            var buffer = new byte[m_length];
            Buffer.BlockCopy(m_buffer, 0, buffer, 0, m_length);
            return buffer;
        }

        public void WriteBoolean(bool value)
        {
            WriteByte(value ? (byte)1 : (byte)0);
        }

        public void WriteSByte(sbyte value)
        {
            WriteByte(unchecked((byte)value));
        }

        public void WriteByte(byte value)
        {
            EnsureCapacity(1);
            m_buffer[m_length++] = value;
        }

        public void WriteInt16(short value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteUInt16(ushort value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteInt32(int value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteUInt32(uint value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteInt64(long value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteUInt64(ulong value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteSingle(float value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteDouble(double value)
        {
            WriteLittleEndian(BitConverter.GetBytes(value));
        }

        public void WriteString(string value)
        {
            var text = value ?? string.Empty;
            byte[] encoded = Encoding.UTF8.GetBytes(text);
            WriteUInt32((uint)encoded.Length);
            WriteRawBytes(encoded);
        }

        public void WriteBytes(byte[]? value)
        {
            byte[] bytes = value ?? Array.Empty<byte>();
            WriteUInt32((uint)bytes.Length);
            WriteRawBytes(bytes);
        }

        public void WriteRawBytes(byte[] value)
        {
            if (value == null || value.Length == 0)
            {
                return;
            }

            EnsureCapacity(value.Length);
            Buffer.BlockCopy(value, 0, m_buffer, m_length, value.Length);
            m_length += value.Length;
        }

        private void EnsureCapacity(int additionalLength)
        {
            int requiredLength = checked(m_length + additionalLength);
            if (requiredLength <= m_buffer.Length)
            {
                return;
            }

            int nextCapacity = m_buffer.Length;
            while (nextCapacity < requiredLength)
            {
                nextCapacity = checked(nextCapacity * 2);
            }

            Array.Resize(ref m_buffer, nextCapacity);
        }

        private void WriteLittleEndian(byte[] bytes)
        {
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(bytes);
            }

            WriteRawBytes(bytes);
        }
    }
}
