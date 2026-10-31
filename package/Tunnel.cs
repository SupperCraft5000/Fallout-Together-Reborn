// Trägt den UDP-Verkehr des Spiels und den TCP-Dienst des Starters über das Steam-Netz.
// Spiel und Server merken davon nichts, sie sprechen mit einem Port auf 127.0.0.1.
// Läuft als eigener Prozess des Starters, weil er vor dem Spiel gebraucht wird und Steam dafür die App-ID kennen muss.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Threading;

interface ITunnelLink
{
    bool Send(ulong aPeer, int aChannel, byte[] aData, int aSize, bool aReliable);
    void Receive(int aChannel, Action<ulong, byte[]> aOnMessage);
    void Pump();
}

sealed class TunnelCore
{
    const int ChannelUdp = 0;
    const int ChannelTcp = 1;
    const byte FrameOpen = 1;
    const byte FrameData = 2;
    const byte FrameClose = 3;
    const int FrameHeader = 5;
    const int UdpHeader = 2;
    const int HostSocketIdleMs = 120000;
    const int ChunkSize = 16 * 1024;
    const int MaxQueued = 16;
    const int SioUdpConnReset = -1744830452;

    sealed class Stream
    {
        public Socket Socket;
        public ulong Peer;
        public uint Id;
        public readonly Queue<byte[]> ToLink = new Queue<byte[]>();
        public readonly Queue<byte[]> ToSocket = new Queue<byte[]>();
        public int SocketOffset;
        public bool LocalClosed;
        public bool RemoteClosed;
    }

    readonly ITunnelLink m_link;
    // Jede Spielsitzung braucht beim Host einen eigenen Socket, sonst hält der Server eine neue Sitzung
    // für die alte, wenn deren Ende nicht bei ihm ankam, etwa nach dem harten Beenden des Spiels
    sealed class HostSocket
    {
        public Socket Socket;
        public ulong Peer;
        public int LastUse;
        public byte Tag0;
        public byte Tag1;
    }

    readonly ulong m_remote;
    readonly int m_targetUdp;
    readonly int m_targetTcp;
    readonly byte[] m_buffer = new byte[64 * 1024];
    readonly Dictionary<string, HostSocket> m_hostSockets = new Dictionary<string, HostSocket>();
    readonly Dictionary<string, Stream> m_streams = new Dictionary<string, Stream>();
    readonly List<string> m_finished = new List<string>();
    Socket m_gameSocket;
    int m_nextCleanup;
    TcpListener m_listener;
    uint m_nextStream;

    public long PacketsOut;
    public long PacketsIn;
    public int UdpPort { get; private set; }
    public int TcpPort { get; private set; }

    // aRemote ist die Steam-ID des Hosts, 0 bedeutet: Dieser PC ist der Host
    public TunnelCore(ITunnelLink aLink, ulong aRemote, int aTargetUdp, int aTargetTcp)
    {
        m_link = aLink;
        m_remote = aRemote;
        m_targetUdp = aTargetUdp;
        m_targetTcp = aTargetTcp;
        if (aRemote == 0)
            return;

        m_gameSocket = NewUdpSocket();
        UdpPort = ((IPEndPoint)m_gameSocket.LocalEndPoint).Port;
        m_listener = new TcpListener(IPAddress.Loopback, 0);
        m_listener.Start();
        TcpPort = ((IPEndPoint)m_listener.LocalEndpoint).Port;
    }

    static Socket NewUdpSocket()
    {
        var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
        socket.Bind(new IPEndPoint(IPAddress.Loopback, 0));
        socket.Blocking = false;
        // Sonst meldet Windows einen geschlossenen Zielport als Fehler beim nächsten Lesen
        socket.IOControl(SioUdpConnReset, new byte[] { 0, 0, 0, 0 }, null);
        return socket;
    }

    public void Step()
    {
        m_link.Pump();
        m_link.Receive(ChannelUdp, OnUdpFromLink);
        m_link.Receive(ChannelTcp, OnTcpFromLink);

        if (m_gameSocket != null)
            ReadGameSocket();
        foreach (var socket in m_hostSockets.Values)
            ReadHostSocket(socket);
        RemoveIdleHostSockets();

        if (m_listener != null)
            while (m_listener.Pending())
                AcceptStream();
        PumpStreams();
    }

    void OnUdpFromLink(ulong aPeer, byte[] aData)
    {
        if (aData.Length < UdpHeader)
            return;

        PacketsIn++;
        try
        {
            if (m_remote != 0)
            {
                // Beim Mitspieler ist die Kennung der Port des Spiels
                var game = new IPEndPoint(IPAddress.Loopback, BitConverter.ToUInt16(aData, 0));
                m_gameSocket.SendTo(aData, UdpHeader, aData.Length - UdpHeader, SocketFlags.None, game);
                return;
            }

            var key = aPeer + ":" + BitConverter.ToUInt16(aData, 0);
            HostSocket socket;
            if (!m_hostSockets.TryGetValue(key, out socket))
            {
                socket = new HostSocket { Socket = NewUdpSocket(), Peer = aPeer, Tag0 = aData[0], Tag1 = aData[1] };
                m_hostSockets[key] = socket;
            }
            socket.LastUse = Environment.TickCount;
            socket.Socket.SendTo(aData, UdpHeader, aData.Length - UdpHeader, SocketFlags.None, new IPEndPoint(IPAddress.Loopback, m_targetUdp));
        }
        catch (SocketException) { }
    }

    void ReadGameSocket()
    {
        while (m_gameSocket.Available > 0)
        {
            EndPoint from = new IPEndPoint(IPAddress.Any, 0);
            int size;
            try
            {
                size = m_gameSocket.ReceiveFrom(m_buffer, UdpHeader, m_buffer.Length - UdpHeader, SocketFlags.None, ref from);
            }
            catch (SocketException)
            {
                return;
            }
            BitConverter.GetBytes((ushort)((IPEndPoint)from).Port).CopyTo(m_buffer, 0);
            PacketsOut++;
            m_link.Send(m_remote, ChannelUdp, m_buffer, size + UdpHeader, false);
        }
    }

    void ReadHostSocket(HostSocket aSocket)
    {
        while (aSocket.Socket.Available > 0)
        {
            int size;
            try
            {
                size = aSocket.Socket.Receive(m_buffer, UdpHeader, m_buffer.Length - UdpHeader, SocketFlags.None);
            }
            catch (SocketException)
            {
                return;
            }
            m_buffer[0] = aSocket.Tag0;
            m_buffer[1] = aSocket.Tag1;
            aSocket.LastUse = Environment.TickCount;
            PacketsOut++;
            m_link.Send(aSocket.Peer, ChannelUdp, m_buffer, size + UdpHeader, false);
        }
    }

    void RemoveIdleHostSockets()
    {
        int now = Environment.TickCount;
        if (now - m_nextCleanup < 0)
            return;
        m_nextCleanup = now + 10000;

        foreach (var pair in new List<KeyValuePair<string, HostSocket>>(m_hostSockets))
            if (now - pair.Value.LastUse > HostSocketIdleMs)
            {
                pair.Value.Socket.Close();
                m_hostSockets.Remove(pair.Key);
            }
    }

    static string Key(ulong aPeer, uint aId)
    {
        return aPeer + ":" + aId;
    }

    static byte[] Frame(byte aType, uint aId, byte[] aData, int aSize)
    {
        var frame = new byte[FrameHeader + aSize];
        frame[0] = aType;
        BitConverter.GetBytes(aId).CopyTo(frame, 1);
        if (aSize > 0)
            Buffer.BlockCopy(aData, 0, frame, FrameHeader, aSize);
        return frame;
    }

    void AcceptStream()
    {
        var socket = m_listener.AcceptSocket();
        socket.Blocking = false;
        socket.NoDelay = true;
        var stream = new Stream { Socket = socket, Peer = m_remote, Id = ++m_nextStream };
        stream.ToLink.Enqueue(Frame(FrameOpen, stream.Id, null, 0));
        m_streams[Key(stream.Peer, stream.Id)] = stream;
    }

    void OnTcpFromLink(ulong aPeer, byte[] aData)
    {
        if (aData.Length < FrameHeader)
            return;

        uint id = BitConverter.ToUInt32(aData, 1);
        var key = Key(aPeer, id);
        Stream stream;
        m_streams.TryGetValue(key, out stream);

        switch (aData[0])
        {
            case FrameOpen:
                if (m_remote != 0 || stream != null)
                    return;
                stream = new Stream { Peer = aPeer, Id = id };
                try
                {
                    stream.Socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                    stream.Socket.Connect(IPAddress.Loopback, m_targetTcp);
                    stream.Socket.Blocking = false;
                    stream.Socket.NoDelay = true;
                }
                catch (SocketException)
                {
                    stream.Socket.Close();
                    m_link.Send(aPeer, ChannelTcp, Frame(FrameClose, id, null, 0), FrameHeader, true);
                    return;
                }
                m_streams[key] = stream;
                break;

            case FrameData:
                if (stream != null)
                {
                    var data = new byte[aData.Length - FrameHeader];
                    Buffer.BlockCopy(aData, FrameHeader, data, 0, data.Length);
                    stream.ToSocket.Enqueue(data);
                }
                break;

            case FrameClose:
                if (stream != null)
                    stream.RemoteClosed = true;
                break;
        }
    }

    void PumpStreams()
    {
        foreach (var pair in m_streams)
        {
            var stream = pair.Value;
            WriteToSocket(stream);
            ReadFromSocket(stream);

            while (stream.ToLink.Count > 0)
            {
                var frame = stream.ToLink.Peek();
                if (!m_link.Send(stream.Peer, ChannelTcp, frame, frame.Length, true))
                    break;
                stream.ToLink.Dequeue();
            }

            bool drained = stream.ToSocket.Count == 0 && stream.ToLink.Count == 0;
            if (drained && (stream.RemoteClosed || stream.LocalClosed))
            {
                if (stream.RemoteClosed && !stream.LocalClosed)
                    stream.Socket.Shutdown(SocketShutdown.Both);
                stream.Socket.Close();
                m_finished.Add(pair.Key);
            }
        }

        foreach (var key in m_finished)
            m_streams.Remove(key);
        m_finished.Clear();
    }

    void WriteToSocket(Stream aStream)
    {
        while (aStream.ToSocket.Count > 0 && !aStream.LocalClosed)
        {
            var data = aStream.ToSocket.Peek();
            SocketError error;
            int sent = aStream.Socket.Send(data, aStream.SocketOffset, data.Length - aStream.SocketOffset, SocketFlags.None, out error);
            if (error == SocketError.WouldBlock)
                return;
            if (error != SocketError.Success)
            {
                CloseLocal(aStream);
                return;
            }

            aStream.SocketOffset += sent;
            if (aStream.SocketOffset < data.Length)
                return;
            aStream.ToSocket.Dequeue();
            aStream.SocketOffset = 0;
        }
    }

    void ReadFromSocket(Stream aStream)
    {
        while (!aStream.LocalClosed && aStream.ToLink.Count < MaxQueued)
        {
            SocketError error;
            int size = aStream.Socket.Receive(m_buffer, 0, ChunkSize, SocketFlags.None, out error);
            if (error == SocketError.WouldBlock)
                return;
            if (error != SocketError.Success || size == 0)
            {
                CloseLocal(aStream);
                return;
            }
            aStream.ToLink.Enqueue(Frame(FrameData, aStream.Id, m_buffer, size));
        }
    }

    void CloseLocal(Stream aStream)
    {
        aStream.LocalClosed = true;
        aStream.ToSocket.Clear();
        aStream.ToLink.Enqueue(Frame(FrameClose, aStream.Id, null, 0));
    }
}

static class SteamNative
{
    const string Dll = "steam_api64";

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern bool SetDllDirectory(string aPath);
    [DllImport("winmm.dll")] public static extern uint timeBeginPeriod(uint aPeriod);

    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_Init();
    [DllImport(Dll)] public static extern void SteamAPI_Shutdown();
    [DllImport(Dll)] public static extern void SteamAPI_RunCallbacks();
    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamUser_v021();
    [DllImport(Dll)] public static extern ulong SteamAPI_ISteamUser_GetSteamID(IntPtr aUser);

    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
    [DllImport(Dll)] public static extern void SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(IntPtr aUtils);
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus(IntPtr aUtils, IntPtr aDetails);
    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32(IntPtr aUtils, int aValue, int aData);
    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr(IntPtr aUtils, int aValue, IntPtr aData);

    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamNetworkingSockets_SteamAPI_v012();
    [DllImport(Dll)] public static extern void SteamAPI_ISteamNetworkingSockets_RunCallbacks(IntPtr aSockets);

    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingMessages_SendMessageToUser(IntPtr aMessages, byte[] aIdentity, byte[] aData, uint aSize, int aFlags, int aChannel);
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(IntPtr aMessages, int aChannel, IntPtr[] aOut, int aMax);
    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser(IntPtr aMessages, IntPtr aIdentity);

    [DllImport(Dll)] public static extern void SteamAPI_SteamNetworkingIdentity_SetSteamID64(byte[] aIdentity, ulong aId);
    [DllImport(Dll)] public static extern ulong SteamAPI_SteamNetworkingIdentity_GetSteamID64(IntPtr aIdentity);
    [DllImport(Dll)] public static extern void SteamAPI_SteamNetworkingMessage_t_Release(IntPtr aMessage);
}

sealed class SteamLink : ITunnelLink
{
    const int ResultOk = 1;
    const int FlagNoNagle = 1;
    const int FlagReliable = 8;
    const int FlagAutoRestart = 32;
    const int ConfigSendBuffer = 9;
    const int ConfigSendRateMin = 10;
    const int ConfigSendRateMax = 11;
    const int ConfigIceEnable = 104;
    const int ConfigSessionRequest = 204;
    const int MessagePeerOffset = 16;

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate void SessionRequest(IntPtr aRequest);

    readonly IntPtr m_messages;
    readonly IntPtr m_sockets;
    readonly IntPtr[] m_received = new IntPtr[64];
    readonly Dictionary<ulong, byte[]> m_identities = new Dictionary<ulong, byte[]>();
    // Muss leben, solange Steam den Zeiger hält
    readonly SessionRequest m_onRequest;

    public SteamLink()
    {
        m_messages = SteamNative.SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
        m_sockets = SteamNative.SteamAPI_SteamNetworkingSockets_SteamAPI_v012();
        var utils = SteamNative.SteamAPI_SteamNetworkingUtils_SteamAPI_v004();

        // Nimmt jede Sitzung an, geschützt ist der Host durch das Passwort von Server und Dienst
        m_onRequest = delegate(IntPtr aRequest) { SteamNative.SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser(m_messages, aRequest); };
        SteamNative.SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValuePtr(utils, ConfigSessionRequest, Marshal.GetFunctionPointerForDelegate(m_onRequest));

        // Direkte Verbindungen laufen über steamwebrtc64.dll, die im Test abstürzte
        SteamNative.SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32(utils, ConfigIceEnable, 0);
        SteamNative.SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32(utils, ConfigSendRateMin, 512 * 1024);
        SteamNative.SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32(utils, ConfigSendRateMax, 8 * 1024 * 1024);
        SteamNative.SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32(utils, ConfigSendBuffer, 4 * 1024 * 1024);
    }

    byte[] Identity(ulong aPeer)
    {
        byte[] identity;
        if (!m_identities.TryGetValue(aPeer, out identity))
        {
            identity = new byte[136];
            SteamNative.SteamAPI_SteamNetworkingIdentity_SetSteamID64(identity, aPeer);
            m_identities[aPeer] = identity;
        }
        return identity;
    }

    public bool Send(ulong aPeer, int aChannel, byte[] aData, int aSize, bool aReliable)
    {
        int flags = FlagNoNagle | FlagAutoRestart | (aReliable ? FlagReliable : 0);
        return SteamNative.SteamAPI_ISteamNetworkingMessages_SendMessageToUser(m_messages, Identity(aPeer), aData, (uint)aSize, flags, aChannel) == ResultOk;
    }

    public void Receive(int aChannel, Action<ulong, byte[]> aOnMessage)
    {
        int count;
        do
        {
            count = SteamNative.SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(m_messages, aChannel, m_received, m_received.Length);
            for (int i = 0; i < count; i++)
            {
                var message = m_received[i];
                var data = new byte[Marshal.ReadInt32(message, 8)];
                Marshal.Copy(Marshal.ReadIntPtr(message, 0), data, 0, data.Length);
                ulong peer = SteamNative.SteamAPI_SteamNetworkingIdentity_GetSteamID64(new IntPtr(message.ToInt64() + MessagePeerOffset));
                SteamNative.SteamAPI_SteamNetworkingMessage_t_Release(message);
                aOnMessage(peer, data);
            }
        } while (count == m_received.Length);
    }

    public void Pump()
    {
        SteamNative.SteamAPI_RunCallbacks();
        SteamNative.SteamAPI_ISteamNetworkingSockets_RunCallbacks(m_sockets);
    }
}

// Aufruf: --tunnel <Spielordner> <Eltern-PID> <Steam-ID des Hosts oder 0> <UDP-Ziel> <TCP-Ziel>
// Meldet auf stdout: READY <eigene Steam-ID> <UDP-Port> <TCP-Port>, oder ERROR <Text>
static class TunnelProcess
{
    const string AppId = "377160";
    const int RelayAvailable = 100;

    public static int Run(string[] aArgs)
    {
        var log = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"FalloutTogetherReborn\tunnel.log");
        using (var writer = new StreamWriter(log, false) { AutoFlush = true })
        {
            try
            {
                return Loop(aArgs, writer);
            }
            catch (Exception e)
            {
                writer.WriteLine(e);
                Console.Out.WriteLine("ERROR " + e.Message);
                Console.Out.Flush();
                return 1;
            }
        }
    }

    static void Log(TextWriter aLog, string aText)
    {
        aLog.WriteLine(DateTime.Now.ToString("HH:mm:ss") + "  " + aText);
    }

    static int Loop(string[] aArgs, TextWriter aLog)
    {
        var game = aArgs[1];
        var parent = Process.GetProcessById(int.Parse(aArgs[2]));
        ulong remote = ulong.Parse(aArgs[3]);
        int targetUdp = int.Parse(aArgs[4]);
        int targetTcp = int.Parse(aArgs[5]);

        SteamNative.timeBeginPeriod(1);
        SteamNative.SetDllDirectory(game);
        Environment.SetEnvironmentVariable("SteamAppId", AppId);
        Environment.SetEnvironmentVariable("SteamGameId", AppId);
        if (!SteamNative.SteamAPI_Init())
            throw new InvalidOperationException("Steam ist nicht bereit. Läuft Steam, und gehört dem Konto Fallout 4?");

        try
        {
            ulong own = SteamNative.SteamAPI_ISteamUser_GetSteamID(SteamNative.SteamAPI_SteamUser_v021());
            var link = new SteamLink();
            var utils = SteamNative.SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
            SteamNative.SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(utils);

            var clock = Stopwatch.StartNew();
            int relay;
            do
            {
                link.Pump();
                relay = SteamNative.SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus(utils, IntPtr.Zero);
                Thread.Sleep(50);
            } while (relay != RelayAvailable && relay > -100 && clock.Elapsed.TotalSeconds < 30);
            Log(aLog, "Relay-Status " + relay);
            if (relay != RelayAvailable)
                throw new InvalidOperationException("Das Steam-Netz ist nicht erreichbar (Status " + relay + ").");

            var core = new TunnelCore(link, remote, targetUdp, targetTcp);
            Console.Out.WriteLine(string.Format("READY {0} {1} {2}", own, core.UdpPort, core.TcpPort));
            Console.Out.Flush();
            Log(aLog, (remote == 0 ? "Host" : "Mitspieler") + ", UDP " + core.UdpPort + ", TCP " + core.TcpPort);

            double nextReport = 30;
            while (!parent.HasExited)
            {
                core.Step();
                if (clock.Elapsed.TotalSeconds >= nextReport)
                {
                    nextReport += 30;
                    Log(aLog, "Pakete raus " + core.PacketsOut + ", rein " + core.PacketsIn);
                }
                Thread.Sleep(1);
            }
            Log(aLog, "Starter beendet");
            return 0;
        }
        finally
        {
            SteamNative.SteamAPI_Shutdown();
        }
    }
}
