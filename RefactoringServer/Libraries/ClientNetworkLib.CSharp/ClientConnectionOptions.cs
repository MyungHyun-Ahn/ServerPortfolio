using System;

namespace ClientNetworkLib.CSharp
{
    public sealed class ClientConnectionOptions
    {
        public ClientConnectionOptions(string host, int port, byte packetKey, int receiveBufferSize = 4096)
        {
            if (string.IsNullOrWhiteSpace(host))
            {
                throw new ArgumentException("Host must not be empty.", nameof(host));
            }

            if (port < 1 || port > 65535)
            {
                throw new ArgumentOutOfRangeException(nameof(port), "Port must be in range 1..65535.");
            }

            if (receiveBufferSize <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(receiveBufferSize), "ReceiveBufferSize must be positive.");
            }

            Host = host.Trim();
            Port = port;
            PacketKey = packetKey;
            ReceiveBufferSize = receiveBufferSize;
        }

        public string Host { get; }

        public int Port { get; }

        public byte PacketKey { get; }

        public int ReceiveBufferSize { get; }
    }
}
