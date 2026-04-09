using System.Text;

namespace PacketRuntime.CSharp
{
    public sealed class PacketReader
    {
        private readonly byte[] m_buffer;
        private readonly int m_endOffset;
        private int m_offset;

        public PacketReader(byte[] buffer)
            : this(buffer, 0, buffer?.Length ?? 0)
        {
        }

        public PacketReader(byte[] buffer, int offset, int length)
        {
            m_buffer = buffer ?? Array.Empty<byte>();
            m_offset = Math.Max(0, offset);
            m_endOffset = Math.Min(m_buffer.Length, checked(m_offset + Math.Max(0, length)));
        }

        public int RemainingLength => m_endOffset - m_offset;

        public bool IsAtEnd => m_offset == m_endOffset;

        public bool TryReadBoolean(out bool value)
        {
            if (!TryReadByte(out byte byteValue))
            {
                value = default;
                return false;
            }

            value = byteValue != 0;
            return true;
        }

        public bool TryReadSByte(out sbyte value)
        {
            if (!TryReadByte(out byte byteValue))
            {
                value = default;
                return false;
            }

            value = unchecked((sbyte)byteValue);
            return true;
        }

        public bool TryReadByte(out byte value)
        {
            if (!CanRead(1))
            {
                value = default;
                return false;
            }

            value = m_buffer[m_offset++];
            return true;
        }

        public bool TryReadInt16(out short value)
        {
            return TryReadFixed(sizeof(short), bytes => BitConverter.ToInt16(bytes, 0), out value);
        }

        public bool TryReadUInt16(out ushort value)
        {
            return TryReadFixed(sizeof(ushort), bytes => BitConverter.ToUInt16(bytes, 0), out value);
        }

        public bool TryReadInt32(out int value)
        {
            return TryReadFixed(sizeof(int), bytes => BitConverter.ToInt32(bytes, 0), out value);
        }

        public bool TryReadUInt32(out uint value)
        {
            return TryReadFixed(sizeof(uint), bytes => BitConverter.ToUInt32(bytes, 0), out value);
        }

        public bool TryReadInt64(out long value)
        {
            return TryReadFixed(sizeof(long), bytes => BitConverter.ToInt64(bytes, 0), out value);
        }

        public bool TryReadUInt64(out ulong value)
        {
            return TryReadFixed(sizeof(ulong), bytes => BitConverter.ToUInt64(bytes, 0), out value);
        }

        public bool TryReadSingle(out float value)
        {
            return TryReadFixed(sizeof(float), bytes => BitConverter.ToSingle(bytes, 0), out value);
        }

        public bool TryReadDouble(out double value)
        {
            return TryReadFixed(sizeof(double), bytes => BitConverter.ToDouble(bytes, 0), out value);
        }

        public bool TryReadString(out string value)
        {
            if (!TryReadUInt32(out uint wireLength) || !TryReadCountFromWire(wireLength, out int length) || !CanRead(length))
            {
                value = string.Empty;
                return false;
            }

            value = length == 0
                ? string.Empty
                : Encoding.UTF8.GetString(m_buffer, m_offset, length);
            m_offset += length;
            return true;
        }

        public bool TryReadBytes(out byte[] value)
        {
            if (!TryReadUInt32(out uint wireLength) || !TryReadCountFromWire(wireLength, out int length) || !CanRead(length))
            {
                value = Array.Empty<byte>();
                return false;
            }

            if (length == 0)
            {
                value = Array.Empty<byte>();
                return true;
            }

            value = new byte[length];
            Buffer.BlockCopy(m_buffer, m_offset, value, 0, length);
            m_offset += length;
            return true;
        }

        public bool TryReadCount(out int count)
        {
            if (!TryReadUInt32(out uint wireCount))
            {
                count = 0;
                return false;
            }

            return TryReadCountFromWire(wireCount, out count);
        }

        private bool CanRead(int length)
        {
            return length >= 0 && checked(m_offset + length) <= m_endOffset;
        }

        private bool TryReadCountFromWire(uint wireCount, out int count)
        {
            if (wireCount > int.MaxValue)
            {
                count = 0;
                return false;
            }

            count = (int)wireCount;
            return true;
        }

        private bool TryReadFixed<TValue>(int size, Func<byte[], TValue> converter, out TValue value)
        {
            if (!CanRead(size))
            {
                value = default!;
                return false;
            }

            var bytes = new byte[size];
            Buffer.BlockCopy(m_buffer, m_offset, bytes, 0, size);
            m_offset += size;

            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(bytes);
            }

            value = converter(bytes);
            return true;
        }
    }
}
