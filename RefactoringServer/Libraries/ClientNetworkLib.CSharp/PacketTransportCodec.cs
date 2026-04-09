using System;
using System.Collections.Generic;
using PacketRuntime.CSharp;

namespace ClientNetworkLib.CSharp
{
    internal static class PacketTransportCodec
    {
        private const int TransportHeaderSize = 4;
        private const int ContentHeaderSize = 2;

        private static readonly object s_randomLock = new object();
        private static readonly Random s_random = new Random();

        public static byte[] EncodePacket(IContentPacket packet, byte packetKey)
        {
            if (packet == null)
            {
                throw new ArgumentNullException(nameof(packet));
            }

            PacketWriter payloadWriter = new PacketWriter();
            payloadWriter.WriteUInt16(packet.Opcode);
            packet.Serialize(payloadWriter);

            byte[] payload = payloadWriter.ToArray();
            if (payload.Length > ushort.MaxValue)
            {
                throw new InvalidOperationException($"Packet payload is too large. opcode={packet.Opcode}, length={payload.Length}");
            }

            byte randomKey = NextRandomKey();
            EncodeInPlace(payload, randomKey, packetKey);
            byte checksum = CalculateChecksum(payload);

            ushort payloadLength = checked((ushort)payload.Length);
            byte[] framedPacket = new byte[TransportHeaderSize + payloadLength];
            framedPacket[0] = (byte)(payloadLength & 0xFF);
            framedPacket[1] = (byte)(payloadLength >> 8);
            framedPacket[2] = randomKey;
            framedPacket[3] = checksum;

            if (payloadLength > 0)
            {
                Buffer.BlockCopy(payload, 0, framedPacket, TransportHeaderSize, payloadLength);
            }

            return framedPacket;
        }

        public static bool TryExtractNextPacket(
            List<byte> receiveBuffer,
            byte packetKey,
            IContentPacketRegistry packetRegistry,
            out PacketReceivedEventArgs? packetEvent,
            out string? errorMessage)
        {
            packetEvent = null;
            errorMessage = null;

            if (receiveBuffer == null)
            {
                throw new ArgumentNullException(nameof(receiveBuffer));
            }

            if (packetRegistry == null)
            {
                throw new ArgumentNullException(nameof(packetRegistry));
            }

            if (receiveBuffer.Count < TransportHeaderSize)
            {
                return false;
            }

            ushort payloadLength = (ushort)(receiveBuffer[0] | (receiveBuffer[1] << 8));
            int packetLength = TransportHeaderSize + payloadLength;
            if (receiveBuffer.Count < packetLength)
            {
                return false;
            }

            byte randomKey = receiveBuffer[2];
            byte checksum = receiveBuffer[3];
            byte[] encodedPayload = new byte[payloadLength];
            if (payloadLength > 0)
            {
                receiveBuffer.CopyTo(TransportHeaderSize, encodedPayload, 0, payloadLength);
            }

            receiveBuffer.RemoveRange(0, packetLength);

            byte actualChecksum = CalculateChecksum(encodedPayload);
            if (actualChecksum != checksum)
            {
                errorMessage = $"Packet checksum mismatch. expected={checksum}, actual={actualChecksum}";
                return false;
            }

            DecodeInPlace(encodedPayload, randomKey, packetKey);
            if (encodedPayload.Length < ContentHeaderSize)
            {
                errorMessage = "Content payload is shorter than opcode header.";
                return false;
            }

            ushort opcode = (ushort)(encodedPayload[0] | (encodedPayload[1] << 8));
            int bodyLength = encodedPayload.Length - ContentHeaderSize;
            byte[] body = new byte[bodyLength];
            if (bodyLength > 0)
            {
                Buffer.BlockCopy(encodedPayload, ContentHeaderSize, body, 0, bodyLength);
            }

            IContentPacket? packet = packetRegistry.CreatePacket(opcode);
            if (packet != null)
            {
                PacketReader reader = new PacketReader(body);
                if (!packet.Deserialize(reader))
                {
                    errorMessage = $"Failed to deserialize packet body. opcode={opcode}";
                    return false;
                }
            }

            packetEvent = new PacketReceivedEventArgs(opcode, packet, body);
            return true;
        }

        private static byte NextRandomKey()
        {
            lock (s_randomLock)
            {
                return (byte)s_random.Next(0, byte.MaxValue + 1);
            }
        }

        private static void EncodeInPlace(byte[] buffer, byte randomKey, byte packetKey)
        {
            byte plainState = 0;
            byte encodedState = 0;
            byte randomKeyPlusOne = (byte)(randomKey + 1);
            byte packetKeyPlusOne = (byte)(packetKey + 1);

            for (int index = 0; index < buffer.Length; ++index)
            {
                byte plainValue = buffer[index];
                plainState = (byte)(plainValue ^ (byte)(plainState + randomKeyPlusOne + index));
                encodedState = (byte)(plainState ^ (byte)(encodedState + packetKeyPlusOne + index));
                buffer[index] = encodedState;
            }
        }

        private static void DecodeInPlace(byte[] buffer, byte randomKey, byte packetKey)
        {
            byte previousPlainState = 0;
            byte previousEncodedState = 0;
            byte randomKeyPlusOne = (byte)(randomKey + 1);
            byte packetKeyPlusOne = (byte)(packetKey + 1);

            for (int index = 0; index < buffer.Length; ++index)
            {
                byte encodedValue = buffer[index];
                byte plainState = (byte)(encodedValue ^ (byte)(previousEncodedState + packetKeyPlusOne + index));
                byte decodedValue = (byte)(plainState ^ (byte)(previousPlainState + randomKeyPlusOne + index));
                buffer[index] = decodedValue;
                previousEncodedState = encodedValue;
                previousPlainState = plainState;
            }
        }

        private static byte CalculateChecksum(byte[] buffer)
        {
            uint sum = 0;
            for (int index = 0; index < buffer.Length; ++index)
            {
                sum += buffer[index];
            }

            return (byte)(sum & 0xFF);
        }
    }
}
