using ChattingClientWinForms.Models;
using ChattingClientWinForms.Networking;

namespace ChattingClientWinForms;

internal sealed class MainForm : Form
{
    private readonly ChattingTcpClient m_client = new();

    private readonly TextBox m_hostTextBox = new() { Dock = DockStyle.Fill, Text = "127.0.0.1" };
    private readonly NumericUpDown m_portNumericUpDown = new() { Dock = DockStyle.Fill, Minimum = 1, Maximum = 65535, Value = 19100 };
    private readonly NumericUpDown m_packetKeyNumericUpDown = new() { Dock = DockStyle.Fill, Minimum = 0, Maximum = 255, Value = 55 };
    private readonly Label m_connectionStatusValueLabel = new() { Dock = DockStyle.Fill, Text = "Disconnected", AutoSize = true };
    private readonly Label m_currentUserValueLabel = new() { Dock = DockStyle.Fill, Text = "-", AutoSize = true };
    private readonly Label m_currentRoomValueLabel = new() { Dock = DockStyle.Fill, Text = "-", AutoSize = true };
    private readonly Button m_connectButton = new() { Dock = DockStyle.Fill, Text = "Connect" };
    private readonly Button m_disconnectButton = new() { Dock = DockStyle.Fill, Text = "Disconnect" };

    private readonly TextBox m_loginIdTextBox = new() { Dock = DockStyle.Fill };
    private readonly TextBox m_loginPasswordTextBox = new() { Dock = DockStyle.Fill, UseSystemPasswordChar = true };
    private readonly Button m_loginButton = new() { Dock = DockStyle.Fill, Text = "Login (Prototype)" };

    private readonly TextBox m_registerIdTextBox = new() { Dock = DockStyle.Fill };
    private readonly TextBox m_registerPasswordTextBox = new() { Dock = DockStyle.Fill, UseSystemPasswordChar = true };
    private readonly TextBox m_registerDisplayNameTextBox = new() { Dock = DockStyle.Fill };
    private readonly Button m_registerButton = new() { Dock = DockStyle.Fill, Text = "Register (Mock)" };

    private readonly Button m_refreshRoomsButton = new() { Dock = DockStyle.Fill, Text = "Refresh Rooms" };
    private readonly Button m_enterRoomButton = new() { Dock = DockStyle.Fill, Text = "Enter / Change Room" };
    private readonly ListView m_roomListView = new()
    {
        Dock = DockStyle.Fill,
        View = View.Details,
        FullRowSelect = true,
        HideSelection = false,
        MultiSelect = false
    };

    private readonly RichTextBox m_chatLogTextBox = new()
    {
        Dock = DockStyle.Fill,
        ReadOnly = true,
        BackColor = SystemColors.Window
    };
    private readonly TextBox m_chatInputTextBox = new() { Dock = DockStyle.Fill };
    private readonly Button m_sendChatButton = new() { Dock = DockStyle.Fill, Text = "Send" };

    private readonly Dictionary<uint, ChatRoomInfo> m_roomMap = [];

    private uint m_currentUserId;
    private uint m_currentRoomId;
    private bool m_loginAccepted;
    private bool m_isBusy;
    private bool m_isAwaitingChatAck;
    private string? m_pendingChatText;
    private ulong m_pendingMessageId;
    private ulong m_nextClientMessageId = 1;

    public MainForm()
    {
        Text = "ChattingClient WinForms Prototype";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(1200, 760);
        ClientSize = new Size(1280, 820);

        InitializeUi();
        WireEvents();
        UpdateUiState();
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        try
        {
            m_client.DisposeAsync().AsTask().GetAwaiter().GetResult();
        }
        catch
        {
        }

        base.OnFormClosed(e);
    }

    private void InitializeUi()
    {
        m_roomListView.Columns.Add("RoomId", 90);
        m_roomListView.Columns.Add("Name", 170);
        m_roomListView.Columns.Add("Participants", 100);
        m_roomListView.Columns.Add("Capacity", 80);
        m_roomListView.Columns.Add("Joinable", 80);

        TableLayoutPanel rootLayout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            Padding = new Padding(10)
        };
        rootLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 360));
        rootLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        TableLayoutPanel upperLayout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 3,
            RowCount = 1
        };
        upperLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 320));
        upperLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        upperLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));

        upperLayout.Controls.Add(BuildConnectionGroup(), 0, 0);
        upperLayout.Controls.Add(BuildAuthGroup(), 1, 0);
        upperLayout.Controls.Add(BuildRoomGroup(), 2, 0);

        rootLayout.Controls.Add(upperLayout, 0, 0);
        rootLayout.Controls.Add(BuildChatGroup(), 0, 1);

        Controls.Add(rootLayout);
    }

    private Control BuildConnectionGroup()
    {
        GroupBox groupBox = new()
        {
            Dock = DockStyle.Fill,
            Text = "Server Connection"
        };

        TableLayoutPanel layout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 8,
            Padding = new Padding(8)
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        for (int row = 0; row < 6; ++row)
        {
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
        }

        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        layout.Controls.Add(CreateFieldLabel("Host"), 0, 0);
        layout.Controls.Add(m_hostTextBox, 1, 0);
        layout.Controls.Add(CreateFieldLabel("Port"), 0, 1);
        layout.Controls.Add(m_portNumericUpDown, 1, 1);
        layout.Controls.Add(CreateFieldLabel("Packet Key"), 0, 2);
        layout.Controls.Add(m_packetKeyNumericUpDown, 1, 2);
        layout.Controls.Add(CreateFieldLabel("Status"), 0, 3);
        layout.Controls.Add(m_connectionStatusValueLabel, 1, 3);
        layout.Controls.Add(CreateFieldLabel("UserId"), 0, 4);
        layout.Controls.Add(m_currentUserValueLabel, 1, 4);
        layout.Controls.Add(CreateFieldLabel("Current Room"), 0, 5);
        layout.Controls.Add(m_currentRoomValueLabel, 1, 5);

        FlowLayoutPanel buttonPanel = new()
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.LeftToRight
        };
        buttonPanel.Controls.Add(m_connectButton);
        buttonPanel.Controls.Add(m_disconnectButton);
        layout.SetColumnSpan(buttonPanel, 2);
        layout.Controls.Add(buttonPanel, 0, 6);

        Label noteLabel = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            Text = "기본값은 현재 ChattingServer 설정과 맞춰져 있습니다.\r\n로그인은 어떤 입력이든 통과하며 임시 userId를 생성합니다."
        };
        layout.SetColumnSpan(noteLabel, 2);
        layout.Controls.Add(noteLabel, 0, 7);

        groupBox.Controls.Add(layout);
        return groupBox;
    }

    private Control BuildAuthGroup()
    {
        GroupBox groupBox = new()
        {
            Dock = DockStyle.Fill,
            Text = "Login / Register"
        };

        TabControl tabControl = new()
        {
            Dock = DockStyle.Fill
        };
        tabControl.TabPages.Add(BuildLoginTab());
        tabControl.TabPages.Add(BuildRegisterTab());
        groupBox.Controls.Add(tabControl);
        return groupBox;
    }

    private TabPage BuildLoginTab()
    {
        TabPage page = new("Login");
        TableLayoutPanel layout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 4,
            Padding = new Padding(8)
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        layout.Controls.Add(CreateFieldLabel("Login Id"), 0, 0);
        layout.Controls.Add(m_loginIdTextBox, 1, 0);
        layout.Controls.Add(CreateFieldLabel("Password"), 0, 1);
        layout.Controls.Add(m_loginPasswordTextBox, 1, 1);
        layout.SetColumnSpan(m_loginButton, 2);
        layout.Controls.Add(m_loginButton, 0, 2);

        Label infoLabel = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            Text = "현재는 입력값 검증 없이 mock 로그인합니다.\r\n서버에는 생성된 temporary userId만 전송됩니다."
        };
        layout.SetColumnSpan(infoLabel, 2);
        layout.Controls.Add(infoLabel, 0, 3);

        page.Controls.Add(layout);
        return page;
    }

    private TabPage BuildRegisterTab()
    {
        TabPage page = new("Register");
        TableLayoutPanel layout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 5,
            Padding = new Padding(8)
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        layout.Controls.Add(CreateFieldLabel("Login Id"), 0, 0);
        layout.Controls.Add(m_registerIdTextBox, 1, 0);
        layout.Controls.Add(CreateFieldLabel("Password"), 0, 1);
        layout.Controls.Add(m_registerPasswordTextBox, 1, 1);
        layout.Controls.Add(CreateFieldLabel("Nickname"), 0, 2);
        layout.Controls.Add(m_registerDisplayNameTextBox, 1, 2);
        layout.SetColumnSpan(m_registerButton, 2);
        layout.Controls.Add(m_registerButton, 0, 3);

        Label infoLabel = new()
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            Text = "회원가입은 현재 mock success 입니다.\r\n성공하면 로그인 탭 입력란을 자동으로 채워줍니다."
        };
        layout.SetColumnSpan(infoLabel, 2);
        layout.Controls.Add(infoLabel, 0, 4);

        page.Controls.Add(layout);
        return page;
    }

    private Control BuildRoomGroup()
    {
        GroupBox groupBox = new()
        {
            Dock = DockStyle.Fill,
            Text = "Room Selection"
        };

        TableLayoutPanel layout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            Padding = new Padding(8)
        };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        FlowLayoutPanel buttonPanel = new()
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.LeftToRight
        };
        buttonPanel.Controls.Add(m_refreshRoomsButton);
        buttonPanel.Controls.Add(m_enterRoomButton);
        layout.Controls.Add(buttonPanel, 0, 0);
        layout.Controls.Add(m_roomListView, 0, 1);

        groupBox.Controls.Add(layout);
        return groupBox;
    }

    private Control BuildChatGroup()
    {
        GroupBox groupBox = new()
        {
            Dock = DockStyle.Fill,
            Text = "Chat"
        };

        TableLayoutPanel layout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            Padding = new Padding(8)
        };
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));

        layout.Controls.Add(m_chatLogTextBox, 0, 0);

        TableLayoutPanel inputLayout = new()
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1
        };
        inputLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        inputLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140));
        inputLayout.Controls.Add(m_chatInputTextBox, 0, 0);
        inputLayout.Controls.Add(m_sendChatButton, 1, 0);

        layout.Controls.Add(inputLayout, 0, 1);
        groupBox.Controls.Add(layout);
        return groupBox;
    }

    private void WireEvents()
    {
        m_connectButton.Click += async (_, _) => await ConnectOnlyAsync();
        m_disconnectButton.Click += async (_, _) => await DisconnectAsync();
        m_loginButton.Click += async (_, _) => await LoginAsync();
        m_registerButton.Click += (_, _) => HandleRegisterMock();
        m_refreshRoomsButton.Click += async (_, _) => await RefreshRoomsAsync();
        m_enterRoomButton.Click += async (_, _) => await EnterSelectedRoomAsync();
        m_sendChatButton.Click += async (_, _) => await SendChatAsync();
        m_chatInputTextBox.TextChanged += (_, _) => UpdateUiState();
        m_chatInputTextBox.KeyDown += ChatInputTextBox_KeyDown;
        m_roomListView.SelectedIndexChanged += (_, _) => UpdateUiState();
        m_roomListView.DoubleClick += async (_, _) => await EnterSelectedRoomAsync();

        m_client.SystemMessageReceived += message => RunOnUiThread(() => AppendSystemMessage(message));
        m_client.ConnectionStateChanged += connected => RunOnUiThread(() => HandleConnectionStateChanged(connected));
        m_client.LoginResultReceived += result => RunOnUiThread(() => HandleLoginResult(result));
        m_client.RoomListReceived += rooms => RunOnUiThread(() => HandleRoomList(rooms));
        m_client.RoomChangeResultReceived += result => RunOnUiThread(() => HandleRoomChangeResult(result));
        m_client.ChattingResultReceived += result => RunOnUiThread(() => HandleChattingResult(result));
        m_client.BroadcastReceived += message => RunOnUiThread(() => HandleBroadcast(message));
    }

    private async Task ConnectOnlyAsync()
    {
        try
        {
            SetBusy(true);
            await m_client.ConnectAsync(BuildConnectionSettings()).ConfigureAwait(true);
        }
        catch (Exception exception)
        {
            AppendSystemMessage($"Connect failed: {exception.Message}");
        }
        finally
        {
            SetBusy(false);
        }
    }

    private async Task DisconnectAsync()
    {
        try
        {
            SetBusy(true);
            await m_client.DisconnectAsync("Disconnected by user.").ConfigureAwait(true);
        }
        catch (Exception exception)
        {
            AppendSystemMessage($"Disconnect failed: {exception.Message}");
        }
        finally
        {
            SetBusy(false);
        }
    }

    private async Task LoginAsync()
    {
        try
        {
            SetBusy(true);

            if (m_loginAccepted)
            {
                await m_client.DisconnectAsync("Reconnecting for a new login.").ConfigureAwait(true);
                ResetSessionState(clearChatLog: false);
            }

            if (!m_client.IsConnected)
            {
                await m_client.ConnectAsync(BuildConnectionSettings()).ConfigureAwait(true);
            }

            m_currentUserId = TemporaryUserIdFactory.Create(m_loginIdTextBox.Text, m_loginPasswordTextBox.Text);
            UpdateStatusLabels();

            AppendSystemMessage($"Prototype login request sent. temporaryUserId={m_currentUserId}");
            await m_client.SendLoginAsync(m_currentUserId).ConfigureAwait(true);
        }
        catch (Exception exception)
        {
            AppendSystemMessage($"Login request failed: {exception.Message}");
        }
        finally
        {
            SetBusy(false);
        }
    }

    private void HandleRegisterMock()
    {
        string registerId = string.IsNullOrWhiteSpace(m_registerIdTextBox.Text)
            ? $"user{Random.Shared.Next(1000, 9999)}"
            : m_registerIdTextBox.Text.Trim();
        string password = string.IsNullOrWhiteSpace(m_registerPasswordTextBox.Text)
            ? "prototype"
            : m_registerPasswordTextBox.Text;
        string displayName = string.IsNullOrWhiteSpace(m_registerDisplayNameTextBox.Text)
            ? registerId
            : m_registerDisplayNameTextBox.Text.Trim();

        m_registerIdTextBox.Text = registerId;
        m_registerPasswordTextBox.Text = password;
        m_registerDisplayNameTextBox.Text = displayName;

        m_loginIdTextBox.Text = registerId;
        m_loginPasswordTextBox.Text = password;
        AppendSystemMessage($"Mock register success. loginId={registerId}, nickname={displayName}");
    }

    private async Task RefreshRoomsAsync()
    {
        if (!m_loginAccepted)
        {
            AppendSystemMessage("Login first.");
            return;
        }

        try
        {
            await m_client.SendRoomListAsync().ConfigureAwait(true);
        }
        catch (Exception exception)
        {
            AppendSystemMessage($"Room list request failed: {exception.Message}");
        }
    }

    private async Task EnterSelectedRoomAsync()
    {
        if (!m_loginAccepted)
        {
            AppendSystemMessage("Login first.");
            return;
        }

        if (m_roomListView.SelectedItems.Count == 0 || m_roomListView.SelectedItems[0].Tag is not ChatRoomInfo roomInfo)
        {
            AppendSystemMessage("Select a room first.");
            return;
        }

        try
        {
            await m_client.SendRoomChangeAsync(roomInfo.RoomId).ConfigureAwait(true);
        }
        catch (Exception exception)
        {
            AppendSystemMessage($"Room change request failed: {exception.Message}");
        }
    }

    private async Task SendChatAsync()
    {
        if (!m_loginAccepted)
        {
            AppendSystemMessage("Login first.");
            return;
        }

        if (m_currentRoomId == 0)
        {
            AppendSystemMessage("Enter a room first.");
            return;
        }

        if (m_isAwaitingChatAck)
        {
            AppendSystemMessage("Only one outstanding chat is allowed right now.");
            return;
        }

        string chatText = m_chatInputTextBox.Text.Trim();
        if (string.IsNullOrEmpty(chatText))
        {
            return;
        }

        try
        {
            ulong messageId = m_nextClientMessageId++;
            m_isAwaitingChatAck = true;
            m_pendingMessageId = messageId;
            m_pendingChatText = chatText;
            UpdateUiState();

            await m_client.SendChattingAsync(
                    m_currentRoomId,
                    messageId,
                    unchecked((ulong)Environment.TickCount64),
                    chatText)
                .ConfigureAwait(true);

            m_chatInputTextBox.Clear();
        }
        catch (Exception exception)
        {
            m_isAwaitingChatAck = false;
            m_pendingMessageId = 0;
            m_pendingChatText = null;
            AppendSystemMessage($"Send failed: {exception.Message}");
            UpdateUiState();
        }
    }

    private void HandleConnectionStateChanged(bool connected)
    {
        if (!connected)
        {
            ResetSessionState(clearChatLog: false);
        }

        m_connectionStatusValueLabel.Text = connected ? "Connected" : "Disconnected";
        UpdateUiState();
    }

    private void HandleLoginResult(LoginResult result)
    {
        m_loginAccepted = result.Success;
        m_currentUserId = result.UserId;
        if (!result.Success)
        {
            m_currentRoomId = 0;
            AppendSystemMessage($"Login rejected. userId={result.UserId}");
        }
        else
        {
            AppendSystemMessage($"Login succeeded. userId={result.UserId}");
            _ = RefreshRoomsAsync();
        }

        UpdateStatusLabels();
        UpdateUiState();
    }

    private void HandleRoomList(IReadOnlyList<ChatRoomInfo> rooms)
    {
        m_roomMap.Clear();
        m_roomListView.Items.Clear();

        foreach (ChatRoomInfo room in rooms)
        {
            m_roomMap[room.RoomId] = room;
            ListViewItem item = new(room.RoomId.ToString());
            item.SubItems.Add(room.RoomName);
            item.SubItems.Add(room.ParticipantCount.ToString());
            item.SubItems.Add(room.Capacity.ToString());
            item.SubItems.Add(room.Joinable ? "Yes" : "No");
            item.Tag = room;
            m_roomListView.Items.Add(item);
        }

        AppendSystemMessage($"Room list received. roomCount={rooms.Count}");
        UpdateUiState();
    }

    private void HandleRoomChangeResult(RoomChangeResult result)
    {
        if (result.Success)
        {
            m_currentRoomId = result.CurrentRoomId;
            AppendSystemMessage($"Room change success. previous={result.PreviousRoomId}, current={result.CurrentRoomId}");
        }
        else
        {
            AppendSystemMessage(
                $"Room change failed. previous={result.PreviousRoomId}, current={result.CurrentRoomId}, result={result.ResultCode}");
        }

        UpdateStatusLabels();
        UpdateUiState();
    }

    private void HandleChattingResult(ChattingResult result)
    {
        string? pendingText = m_pendingChatText;
        ulong pendingMessageId = m_pendingMessageId;

        m_isAwaitingChatAck = false;
        m_pendingChatText = null;
        m_pendingMessageId = 0;
        UpdateUiState();

        if (!result.Success)
        {
            AppendSystemMessage($"ChattingRp rejected. messageId={pendingMessageId}");
            return;
        }

        if (!string.IsNullOrEmpty(pendingText))
        {
            AppendChatMessage($"me({m_currentUserId})", pendingText);
        }
    }

    private void HandleBroadcast(BroadcastMessage message)
    {
        if (m_currentRoomId != 0 && message.RoomId != m_currentRoomId)
        {
            AppendSystemMessage(
                $"Broadcast received from another room. currentRoom={m_currentRoomId}, packetRoom={message.RoomId}");
            return;
        }

        AppendChatMessage($"{message.SenderUserId}", message.PayloadText);
    }

    private ClientConnectionSettings BuildConnectionSettings()
    {
        return new ClientConnectionSettings(
            m_hostTextBox.Text.Trim(),
            Decimal.ToInt32(m_portNumericUpDown.Value),
            (byte)m_packetKeyNumericUpDown.Value);
    }

    private void ResetSessionState(bool clearChatLog)
    {
        m_loginAccepted = false;
        m_currentUserId = 0;
        m_currentRoomId = 0;
        m_isAwaitingChatAck = false;
        m_pendingChatText = null;
        m_pendingMessageId = 0;
        m_roomMap.Clear();
        m_roomListView.Items.Clear();
        if (clearChatLog)
        {
            m_chatLogTextBox.Clear();
        }

        UpdateStatusLabels();
        UpdateUiState();
    }

    private void UpdateUiState()
    {
        bool connected = m_client.IsConnected;
        bool roomSelected = m_roomListView.SelectedItems.Count > 0;
        bool canSendChat = m_loginAccepted &&
            m_currentRoomId != 0 &&
            !m_isBusy &&
            !m_isAwaitingChatAck &&
            !string.IsNullOrWhiteSpace(m_chatInputTextBox.Text);

        m_connectButton.Enabled = !m_isBusy && !connected;
        m_disconnectButton.Enabled = !m_isBusy && connected;
        m_loginButton.Enabled = !m_isBusy;
        m_registerButton.Enabled = !m_isBusy;
        m_refreshRoomsButton.Enabled = !m_isBusy && m_loginAccepted;
        m_enterRoomButton.Enabled = !m_isBusy && m_loginAccepted && roomSelected;
        m_chatInputTextBox.Enabled = m_loginAccepted && m_currentRoomId != 0 && !m_isBusy;
        m_sendChatButton.Enabled = canSendChat;
    }

    private void UpdateStatusLabels()
    {
        m_connectionStatusValueLabel.Text = m_client.IsConnected ? "Connected" : "Disconnected";
        m_currentUserValueLabel.Text = m_currentUserId == 0 ? "-" : m_currentUserId.ToString();
        m_currentRoomValueLabel.Text = m_currentRoomId == 0 ? "-" : m_currentRoomId.ToString();
    }

    private void AppendSystemMessage(string message)
    {
        AppendLogLine($"[System] {message}");
    }

    private void AppendChatMessage(string senderLabel, string message)
    {
        AppendLogLine($"[{senderLabel}] {message}");
    }

    private void AppendLogLine(string message)
    {
        string timestampedMessage = $"[{DateTime.Now:HH:mm:ss}] {message}";
        if (m_chatLogTextBox.TextLength > 0)
        {
            m_chatLogTextBox.AppendText(Environment.NewLine);
        }

        m_chatLogTextBox.AppendText(timestampedMessage);
        m_chatLogTextBox.SelectionStart = m_chatLogTextBox.TextLength;
        m_chatLogTextBox.ScrollToCaret();
    }

    private void SetBusy(bool isBusy)
    {
        m_isBusy = isBusy;
        UpdateUiState();
    }

    private void RunOnUiThread(Action action)
    {
        if (IsDisposed)
        {
            return;
        }

        if (InvokeRequired)
        {
            try
            {
                BeginInvoke(action);
            }
            catch (ObjectDisposedException)
            {
            }

            return;
        }

        action();
    }

    private void ChatInputTextBox_KeyDown(object? sender, KeyEventArgs e)
    {
        if (e.KeyCode != Keys.Enter)
        {
            return;
        }

        e.SuppressKeyPress = true;
        _ = SendChatAsync();
    }

    private static Label CreateFieldLabel(string text)
    {
        return new Label
        {
            Dock = DockStyle.Fill,
            Text = text,
            TextAlign = ContentAlignment.MiddleLeft,
            AutoSize = true
        };
    }
}
