using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using PacketRuntime.CSharp;

namespace ClientNetworkLib.CSharp
{
    public sealed class ContentTcpClient
    {
        private readonly SemaphoreSlim m_lifecycleLock = new SemaphoreSlim(1, 1);
        private readonly SemaphoreSlim m_sendLock = new SemaphoreSlim(1, 1);
        private readonly List<byte> m_receiveBuffer = new List<byte>();
        private readonly IContentPacketRegistry m_packetRegistry;

        private TcpClient? m_tcpClient;
        private NetworkStream? m_stream;
        private CancellationTokenSource? m_receiveCancellation;
        private Task? m_receiveTask;
        private byte m_packetKey;
        private int m_disconnectNotified;
        private bool m_disposed;

        public ContentTcpClient(IContentPacketRegistry packetRegistry)
        {
            m_packetRegistry = packetRegistry ?? throw new ArgumentNullException(nameof(packetRegistry));
        }

        public event Action<string>? SystemMessageReceived;

        public event Action<bool>? ConnectionStateChanged;

        public event Action<PacketReceivedEventArgs>? PacketReceived;

        public bool IsConnected => m_stream != null && m_tcpClient != null;

        public async Task ConnectAsync(ClientConnectionOptions options, CancellationToken cancellationToken = default)
        {
            ThrowIfDisposed();

            if (options == null)
            {
                throw new ArgumentNullException(nameof(options));
            }

            await m_lifecycleLock.WaitAsync(cancellationToken).ConfigureAwait(false);
            try
            {
                await DisconnectCoreAsync("Disconnected.", notifyDisconnection: false, awaitReceiveTask: false).ConfigureAwait(false);

                TcpClient tcpClient = new TcpClient
                {
                    NoDelay = true
                };

                try
                {
                    await ConnectTcpClientAsync(tcpClient, options.Host, options.Port, cancellationToken).ConfigureAwait(false);
                }
                catch
                {
                    tcpClient.Dispose();
                    throw;
                }

                m_packetKey = options.PacketKey;
                m_tcpClient = tcpClient;
                m_stream = tcpClient.GetStream();
                m_receiveBuffer.Clear();
                Interlocked.Exchange(ref m_disconnectNotified, 0);

                m_receiveCancellation = new CancellationTokenSource();
                m_receiveTask = Task.Run(
                    () => ReceiveLoopAsync(m_stream, options.ReceiveBufferSize, m_receiveCancellation.Token),
                    CancellationToken.None);
            }
            finally
            {
                m_lifecycleLock.Release();
            }

            EmitSystemMessage($"Connected to {options.Host}:{options.Port}.");
            ConnectionStateChanged?.Invoke(true);
        }

        public async Task DisconnectAsync(string reason = "Disconnected.")
        {
            ThrowIfDisposed();

            await m_lifecycleLock.WaitAsync().ConfigureAwait(false);
            try
            {
                await DisconnectCoreAsync(reason, notifyDisconnection: true, awaitReceiveTask: false).ConfigureAwait(false);
            }
            finally
            {
                m_lifecycleLock.Release();
            }
        }

        public async Task SendPacketAsync(IContentPacket packet, CancellationToken cancellationToken = default)
        {
            ThrowIfDisposed();

            if (packet == null)
            {
                throw new ArgumentNullException(nameof(packet));
            }

            byte[] encodedPacket = PacketTransportCodec.EncodePacket(packet, m_packetKey);

            await m_sendLock.WaitAsync(cancellationToken).ConfigureAwait(false);
            try
            {
                NetworkStream? stream = m_stream;
                if (stream == null)
                {
                    throw new InvalidOperationException("Not connected.");
                }

                await stream.WriteAsync(encodedPacket, 0, encodedPacket.Length, cancellationToken).ConfigureAwait(false);
            }
            catch (Exception exception) when (exception is IOException || exception is ObjectDisposedException || exception is SocketException)
            {
                EmitSystemMessage($"Send failed: {exception.Message}");
                throw;
            }
            finally
            {
                m_sendLock.Release();
            }
        }

        public async Task DisposeAsync()
        {
            if (m_disposed)
            {
                return;
            }

            m_disposed = true;

            try
            {
                await m_lifecycleLock.WaitAsync().ConfigureAwait(false);
                try
                {
                    await DisconnectCoreAsync("Client disposed.", notifyDisconnection: false, awaitReceiveTask: false).ConfigureAwait(false);
                }
                finally
                {
                    m_lifecycleLock.Release();
                }
            }
            catch
            {
            }

            m_sendLock.Dispose();
            m_lifecycleLock.Dispose();
        }

        private async Task ReceiveLoopAsync(NetworkStream stream, int receiveBufferSize, CancellationToken cancellationToken)
        {
            string disconnectReason = "Connection closed by server.";

            try
            {
                byte[] readBuffer = new byte[receiveBufferSize];
                while (!cancellationToken.IsCancellationRequested)
                {
                    int receivedBytes = await stream.ReadAsync(readBuffer, 0, readBuffer.Length, cancellationToken).ConfigureAwait(false);
                    if (receivedBytes == 0)
                    {
                        break;
                    }

                    AppendReceivedBytes(readBuffer, receivedBytes);
                    while (true)
                    {
                        if (!PacketTransportCodec.TryExtractNextPacket(
                                m_receiveBuffer,
                                m_packetKey,
                                m_packetRegistry,
                                out PacketReceivedEventArgs? packetEvent,
                                out string? errorMessage))
                        {
                            if (!string.IsNullOrEmpty(errorMessage))
                            {
                                throw new InvalidOperationException(errorMessage);
                            }

                            break;
                        }

                        if (packetEvent != null)
                        {
                            DispatchPacket(packetEvent);
                        }
                    }
                }
            }
            catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
            {
                disconnectReason = "Disconnected.";
            }
            catch (Exception exception)
            {
                disconnectReason = $"Connection lost: {exception.Message}";
                EmitSystemMessage(disconnectReason);
            }
            finally
            {
                await CompleteReceiveLoopAsync(stream, disconnectReason).ConfigureAwait(false);
            }
        }

        private void DispatchPacket(PacketReceivedEventArgs packetEvent)
        {
            try
            {
                PacketReceived?.Invoke(packetEvent);
            }
            catch (Exception exception)
            {
                EmitSystemMessage($"Packet handler failed: {exception.Message}");
            }
        }

        private async Task CompleteReceiveLoopAsync(NetworkStream stream, string disconnectReason)
        {
            await m_lifecycleLock.WaitAsync().ConfigureAwait(false);
            try
            {
                if (!ReferenceEquals(m_stream, stream))
                {
                    return;
                }

                CancellationTokenSource? receiveCancellation = m_receiveCancellation;
                TcpClient? tcpClient = m_tcpClient;

                m_receiveCancellation = null;
                m_receiveTask = null;
                m_stream = null;
                m_tcpClient = null;
                m_receiveBuffer.Clear();

                TryDispose(receiveCancellation);
                TryDispose(stream);
                TryDispose(tcpClient);
            }
            finally
            {
                m_lifecycleLock.Release();
            }

            NotifyDisconnected(disconnectReason);
        }

        private async Task DisconnectCoreAsync(string reason, bool notifyDisconnection, bool awaitReceiveTask)
        {
            CancellationTokenSource? receiveCancellation = m_receiveCancellation;
            Task? receiveTask = m_receiveTask;
            NetworkStream? stream = m_stream;
            TcpClient? tcpClient = m_tcpClient;

            m_receiveCancellation = null;
            m_receiveTask = null;
            m_stream = null;
            m_tcpClient = null;
            m_receiveBuffer.Clear();

            TryCancel(receiveCancellation);
            TryDispose(stream);
            TryDispose(tcpClient);

            if (awaitReceiveTask && receiveTask != null)
            {
                try
                {
                    await receiveTask.ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                }
                catch (ObjectDisposedException)
                {
                }
                catch (IOException)
                {
                }
            }

            TryDispose(receiveCancellation);

            if (notifyDisconnection)
            {
                NotifyDisconnected(reason);
            }
        }

        private void AppendReceivedBytes(byte[] readBuffer, int receivedBytes)
        {
            for (int index = 0; index < receivedBytes; ++index)
            {
                m_receiveBuffer.Add(readBuffer[index]);
            }
        }

        private void NotifyDisconnected(string reason)
        {
            if (Interlocked.Exchange(ref m_disconnectNotified, 1) != 0)
            {
                return;
            }

            EmitSystemMessage(reason);
            ConnectionStateChanged?.Invoke(false);
        }

        private void EmitSystemMessage(string message)
        {
            SystemMessageReceived?.Invoke(message);
        }

        private void ThrowIfDisposed()
        {
            if (m_disposed)
            {
                throw new ObjectDisposedException(nameof(ContentTcpClient));
            }
        }

        private static async Task ConnectTcpClientAsync(TcpClient tcpClient, string host, int port, CancellationToken cancellationToken)
        {
            using (cancellationToken.Register(static state => TryDispose((TcpClient?)state), tcpClient))
            {
                try
                {
                    await tcpClient.ConnectAsync(host, port).ConfigureAwait(false);
                }
                catch (Exception exception) when ((exception is ObjectDisposedException || exception is SocketException) && cancellationToken.IsCancellationRequested)
                {
                    throw new OperationCanceledException("Connection attempt was canceled.", exception, cancellationToken);
                }
            }

            cancellationToken.ThrowIfCancellationRequested();
        }

        private static void TryCancel(CancellationTokenSource? cancellationTokenSource)
        {
            try
            {
                cancellationTokenSource?.Cancel();
            }
            catch
            {
            }
        }

        private static void TryDispose(IDisposable? disposable)
        {
            try
            {
                disposable?.Dispose();
            }
            catch
            {
            }
        }
    }
}
