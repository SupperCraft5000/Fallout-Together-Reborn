// Prüft ohne Spiel, ob zwei PCs über das Steam-Netz miteinander reden können.
// Nutzt die steam_api64.dll aus dem Fallout-Ordner und ändert dort nichts. Bauen mit build.ps1.
//
//   SteamTest.exe [<SteamID64 der Gegenseite>] [--game <Ordner>] [--appid <id>] [--seconds <n>]
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using Microsoft.Win32;

static class Steam
{
    const string Dll = "steam_api64";

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern bool SetDllDirectory(string aPath);

    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_Init();
    [DllImport(Dll)] public static extern void SteamAPI_Shutdown();
    [DllImport(Dll)] public static extern void SteamAPI_RunCallbacks();
    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_IsSteamRunning();

    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamUser_v021();
    [DllImport(Dll)] public static extern ulong SteamAPI_ISteamUser_GetSteamID(IntPtr aUser);
    [DllImport(Dll)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool SteamAPI_ISteamUser_BLoggedOn(IntPtr aUser);

    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
    [DllImport(Dll)] public static extern void SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(IntPtr aUtils);
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus(IntPtr aUtils, IntPtr aDetails);

    [DllImport(Dll)] public static extern IntPtr SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingMessages_SendMessageToUser(IntPtr aMessages, byte[] aIdentity, byte[] aData, uint aSize, int aFlags, int aChannel);
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(IntPtr aMessages, int aChannel, IntPtr[] aOut, int aMax);
    [DllImport(Dll)] public static extern int SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo(IntPtr aMessages, byte[] aIdentity, byte[] aInfo, IntPtr aQuick);

    [DllImport(Dll)] public static extern void SteamAPI_SteamNetworkingIdentity_SetSteamID64(byte[] aIdentity, ulong aId);
    [DllImport(Dll)] public static extern void SteamAPI_SteamNetworkingMessage_t_Release(IntPtr aMessage);
}

static class Program
{
    const int FlagAutoRestart = 32;
    const int InfoSize = 696;
    const int InfoRelayPop = 172;
    const int InfoEndReason = 180;
    const int InfoEndDebug = 184;
    const int InfoDescription = 312;

    static readonly StringBuilder s_log = new StringBuilder();

    static void Log(string aText)
    {
        var line = DateTime.Now.ToString("HH:mm:ss") + "  " + aText;
        Console.WriteLine(line);
        s_log.AppendLine(line);
    }

    static string Option(string[] aArgs, string aName)
    {
        for (int i = 0; i + 1 < aArgs.Length; i++)
            if (aArgs[i] == aName)
                return aArgs[i + 1];
        return null;
    }

    static string ReadValue(RegistryHive aHive, string aKey, string aValue)
    {
        try
        {
            using (var root = RegistryKey.OpenBaseKey(aHive, RegistryView.Registry32))
            using (var sub = root.OpenSubKey(aKey))
                return sub == null ? null : sub.GetValue(aValue) as string;
        }
        catch (Exception)
        {
            return null;
        }
    }

    static string FindGame()
    {
        var candidates = new List<string>();
        var bethesda = ReadValue(RegistryHive.LocalMachine, @"SOFTWARE\Bethesda Softworks\Fallout4", "Installed Path");
        if (bethesda != null)
            candidates.Add(bethesda);

        var steam = ReadValue(RegistryHive.CurrentUser, @"Software\Valve\Steam", "SteamPath");
        if (steam != null)
        {
            candidates.Add(Path.Combine(steam, @"steamapps\common\Fallout 4"));
            var libraries = Path.Combine(steam, @"steamapps\libraryfolders.vdf");
            if (File.Exists(libraries))
                foreach (Match m in Regex.Matches(File.ReadAllText(libraries), "\"path\"\\s+\"([^\"]+)\""))
                    candidates.Add(Path.Combine(m.Groups[1].Value.Replace(@"\\", @"\"), @"steamapps\common\Fallout 4"));
        }

        foreach (var dir in candidates)
            if (File.Exists(Path.Combine(dir, "steam_api64.dll")))
                return dir;
        return null;
    }

    static string Text(byte[] aInfo, int aOffset, int aMax)
    {
        int end = Array.IndexOf(aInfo, (byte)0, aOffset, aMax);
        return Encoding.ASCII.GetString(aInfo, aOffset, (end < 0 ? aOffset + aMax : end) - aOffset);
    }

    static int Main(string[] aArgs)
    {
        Console.OutputEncoding = Encoding.UTF8;
        int result = 1;
        try
        {
            result = Run(aArgs);
        }
        catch (Exception e)
        {
            Log("FEHLER: " + e.GetType().Name + ": " + e.Message);
        }

        var path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "steam-test-ergebnis.txt");
        try
        {
            File.WriteAllText(path, s_log.ToString());
            Console.WriteLine();
            Console.WriteLine("Ergebnis gespeichert: " + path);
        }
        catch (Exception) { }

        Console.WriteLine("Enter zum Beenden.");
        Console.ReadLine();
        return result;
    }

    static int Run(string[] aArgs)
    {
        var game = Option(aArgs, "--game") ?? FindGame();
        var appId = Option(aArgs, "--appid") ?? "377160";
        int seconds = int.Parse(Option(aArgs, "--seconds") ?? "60");
        if (game == null || !File.Exists(Path.Combine(game, "steam_api64.dll")))
        {
            Log("Fallout 4 nicht gefunden. Starte mit: SteamTest.exe --game \"<Ordner>\"");
            return 1;
        }

        Log("Spielordner: " + game);
        Log("App-ID: " + appId);
        Steam.SetDllDirectory(game);
        Environment.SetEnvironmentVariable("SteamAppId", appId);
        Environment.SetEnvironmentVariable("SteamGameId", appId);

        if (!Steam.SteamAPI_IsSteamRunning())
            Log("Hinweis: Steam meldet sich nicht als laufend.");
        if (!Steam.SteamAPI_Init())
        {
            Log("ERGEBNIS: SteamAPI_Init fehlgeschlagen. Läuft Steam, und gehört dem Konto Fallout 4?");
            return 2;
        }

        try
        {
            var user = Steam.SteamAPI_SteamUser_v021();
            ulong own = Steam.SteamAPI_ISteamUser_GetSteamID(user);
            Log("Deine Steam-ID: " + own + (Steam.SteamAPI_ISteamUser_BLoggedOn(user) ? "" : "  (nicht angemeldet)"));

            var utils = Steam.SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
            Steam.SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(utils);
            Log("Relay-Netz wird vorbereitet ...");

            var clock = Stopwatch.StartNew();
            int relay = 0;
            while (clock.Elapsed.TotalSeconds < 20)
            {
                Steam.SteamAPI_RunCallbacks();
                relay = Steam.SteamAPI_ISteamNetworkingUtils_GetRelayNetworkStatus(utils, IntPtr.Zero);
                if (relay == 100 || relay <= -100)
                    break;
                Thread.Sleep(100);
            }
            Log("Relay-Status: " + relay + (relay == 100 ? " (verfügbar)" : " (nicht bereit, 100 wäre gut)"));

            var other = aArgs.Length > 0 && !aArgs[0].StartsWith("--") ? aArgs[0] : null;
            if (other == null)
            {
                Console.WriteLine();
                Console.WriteLine("Schicke deinem Mitspieler deine Steam-ID (oben) und trage seine hier ein.");
                Console.Write("Steam-ID der Gegenseite: ");
                other = Console.ReadLine();
            }
            ulong remote;
            if (!ulong.TryParse((other ?? "").Trim(), out remote) || remote == 0)
            {
                Log("Das ist keine gültige Steam-ID.");
                return 3;
            }
            if (remote == own)
            {
                Log("Das ist deine eigene Steam-ID.");
                return 3;
            }

            return Talk(remote, seconds);
        }
        finally
        {
            Steam.SteamAPI_Shutdown();
        }
    }

    static int Talk(ulong aRemote, int aSeconds)
    {
        var messages = Steam.SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
        var identity = new byte[136];
        Steam.SteamAPI_SteamNetworkingIdentity_SetSteamID64(identity, aRemote);

        // Beide Seiten senden sich gegenseitig an, das nimmt die Sitzung ohne Rückruf an
        Log("Verbinde mit " + aRemote + ", läuft " + aSeconds + " Sekunden. Beide müssen gleichzeitig laufen.");
        var clock = Stopwatch.StartNew();
        var sent = new Dictionary<int, double>();
        var rtts = new List<double>();
        var buffer = new IntPtr[32];
        var info = new byte[InfoSize];
        int seq = 0, received = 0, lastState = -1;
        double nextPing = 0, nextReport = 5;
        string description = "";

        while (clock.Elapsed.TotalSeconds < aSeconds)
        {
            Steam.SteamAPI_RunCallbacks();
            double now = clock.Elapsed.TotalMilliseconds;

            if (now >= nextPing)
            {
                nextPing = now + 500;
                var ping = new byte[5];
                ping[0] = 1;
                BitConverter.GetBytes(++seq).CopyTo(ping, 1);
                sent[seq] = now;
                Steam.SteamAPI_ISteamNetworkingMessages_SendMessageToUser(messages, identity, ping, 5, FlagAutoRestart, 0);
            }

            int count = Steam.SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel(messages, 0, buffer, buffer.Length);
            for (int i = 0; i < count; i++)
            {
                IntPtr data = Marshal.ReadIntPtr(buffer[i], 0);
                int size = Marshal.ReadInt32(buffer[i], 8);
                if (size == 5)
                {
                    var bytes = new byte[5];
                    Marshal.Copy(data, bytes, 0, 5);
                    if (bytes[0] == 1)
                    {
                        received++;
                        bytes[0] = 2;
                        Steam.SteamAPI_ISteamNetworkingMessages_SendMessageToUser(messages, identity, bytes, 5, FlagAutoRestart, 0);
                    }
                    else
                    {
                        double start;
                        if (sent.TryGetValue(BitConverter.ToInt32(bytes, 1), out start))
                            rtts.Add(clock.Elapsed.TotalMilliseconds - start);
                    }
                }
                Steam.SteamAPI_SteamNetworkingMessage_t_Release(buffer[i]);
            }

            int state = Steam.SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo(messages, identity, info, IntPtr.Zero);
            if (state != lastState)
            {
                lastState = state;
                description = Text(info, InfoDescription, 128);
                Log("Sitzung: Zustand " + state + "  Relay " + BitConverter.ToUInt32(info, InfoRelayPop).ToString("X8") + "  " + description);
                if (state == 4 || state == 5)
                    Log("Grund " + BitConverter.ToInt32(info, InfoEndReason) + ": " + Text(info, InfoEndDebug, 128));
            }

            if (clock.Elapsed.TotalSeconds >= nextReport)
            {
                nextReport += 5;
                Log(string.Format("Gesendet {0}, Antworten {1}, von der Gegenseite {2}", seq, rtts.Count, received));
            }
            Thread.Sleep(5);
        }

        Log("");
        Log("Pakete gesendet: " + seq);
        Log("Antworten erhalten: " + rtts.Count);
        Log("Pakete der Gegenseite: " + received);
        Log("Letzter Zustand: " + lastState + "  Relay " + BitConverter.ToUInt32(info, InfoRelayPop).ToString("X8") + "  " + description);
        if (rtts.Count == 0)
        {
            Log("ERGEBNIS: keine Verbindung.");
            return 4;
        }

        rtts.Sort();
        Log(string.Format("Laufzeit in ms: min {0:F0}, Median {1:F0}, max {2:F0}", rtts[0], rtts[rtts.Count / 2], rtts[rtts.Count - 1]));
        Log(string.Format("Verlust: {0:F1} %", 100.0 * (seq - rtts.Count) / seq));
        Log("ERGEBNIS: Verbindung über Steam funktioniert.");
        return 0;
    }
}
