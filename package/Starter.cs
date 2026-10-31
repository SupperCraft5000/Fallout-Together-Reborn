// Einzel-exe für Mitspieler: entpackt den Client, findet Fallout 4 und startet es.
// Bauen mit build-package.ps1 (csc aus .NET Framework 4, also C# 5).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: AssemblyTitle("Fallout Together Reborn")]
[assembly: AssemblyProduct("Fallout Together Reborn")]

static class Program
{
    public const string Title = "Fallout Together Reborn";
    public const int DefaultPort = 10578;

    [STAThread]
    static int Main(string[] args)
    {
        if (args.Length > 0 && args[0] == "--tunnel")
            return TunnelProcess.Run(args);

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        string game = null;
        for (int i = 0; i + 1 < args.Length; i++)
            if (args[i] == "--game")
                game = args[i + 1];
        Application.Run(new MainForm(game));
        return 0;
    }
}

static class Game
{
    public static string Find()
    {
        foreach (string dir in Candidates())
            if (IsGameDir(dir))
                return dir;
        return null;
    }

    public static bool IsGameDir(string dir)
    {
        return !string.IsNullOrEmpty(dir) && File.Exists(Path.Combine(dir, "Fallout4.exe"));
    }

    // "1.11.240.0" wird zu "1-11-240-0", wie im Namen der Adressdatei
    public static string Version(string dir)
    {
        var info = FileVersionInfo.GetVersionInfo(Path.Combine(dir, "Fallout4.exe"));
        return string.Format("{0}-{1}-{2}-{3}", info.FileMajorPart, info.FileMinorPart, info.FileBuildPart, info.FilePrivatePart);
    }

    static IEnumerable<string> Candidates()
    {
        var bethesda = Read(RegistryHive.LocalMachine, @"SOFTWARE\Bethesda Softworks\Fallout4", "Installed Path");
        if (bethesda != null)
            yield return bethesda;

        var steam = Read(RegistryHive.CurrentUser, @"Software\Valve\Steam", "SteamPath");
        if (steam == null)
            yield break;

        yield return Path.Combine(steam, @"steamapps\common\Fallout 4");

        var libraries = Path.Combine(steam, @"steamapps\libraryfolders.vdf");
        if (!File.Exists(libraries))
            yield break;

        foreach (Match m in Regex.Matches(File.ReadAllText(libraries), "\"path\"\\s+\"([^\"]+)\""))
            yield return Path.Combine(m.Groups[1].Value.Replace(@"\\", @"\"), @"steamapps\common\Fallout 4");
    }

    static string Read(RegistryHive hive, string key, string value)
    {
        try
        {
            using (var root = RegistryKey.OpenBaseKey(hive, RegistryView.Registry32))
            using (var sub = root.OpenSubKey(key))
                return sub == null ? null : sub.GetValue(value) as string;
        }
        catch (Exception)
        {
            return null;
        }
    }
}

static class Payload
{
    static string s_build;

    public static string Build()
    {
        if (s_build == null)
        {
            var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("build.txt");
            if (stream == null)
                return s_build = "dev";
            using (var reader = new StreamReader(stream))
                s_build = reader.ReadToEnd().Trim();
        }
        return s_build;
    }

    static void CopyTree(string aFrom, string aTo, bool aOverwrite)
    {
        Directory.CreateDirectory(aTo);
        foreach (var file in Directory.GetFiles(aFrom))
        {
            var target = Path.Combine(aTo, Path.GetFileName(file));
            if (aOverwrite || !File.Exists(target))
                File.Copy(file, target, true);
        }
        foreach (var dir in Directory.GetDirectories(aFrom))
            CopyTree(dir, Path.Combine(aTo, Path.GetFileName(dir)), aOverwrite);
    }

    // Entfernt die Ordner älterer Versionen. Figur, Einstellungen und Host-Liste liegen außerhalb und bleiben unberührt,
    // aus den Ordnern selbst bleiben die letzten Logs und eine Server-Konfiguration erhalten. Was noch gesperrt ist,
    // versucht der nächste Start erneut.
    public static void RemoveOldBuilds(string aRoot, string aCurrent)
    {
        var old = new List<DirectoryInfo>();
        foreach (var dir in new DirectoryInfo(aRoot).GetDirectories())
            if (Regex.IsMatch(dir.Name, "^[0-9a-f]{8}-[0-9A-F]{8}$") && dir.Name != Path.GetFileName(aCurrent))
                old.Add(dir);

        // die neueste zuerst, ihre Dateien gewinnen
        old.Sort((aLeft, aRight) => aRight.LastWriteTimeUtc.CompareTo(aLeft.LastWriteTimeUtc));
        var newest = true;
        foreach (var dir in old)
        {
            try
            {
                var logs = Path.Combine(dir.FullName, "logs");
                if (Directory.Exists(logs))
                    CopyTree(logs, Path.Combine(aRoot, "logs-previous"), newest);
                newest = false;
                var config = Path.Combine(dir.FullName, @"server-run\config");
                if (Directory.Exists(config))
                    CopyTree(config, Path.Combine(aCurrent, @"server-run\config"), false);
                dir.Delete(true);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }

    // Der Ordnername hängt am Build, damit eine neue exe nie alte Dateien mitbenutzt
    public static string Prepare(Action<int> aProgress)
    {
        var assembly = Assembly.GetExecutingAssembly();
        var build = Build();

        var root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "FalloutTogetherReborn");
        var target = Path.Combine(root, build);
        var marker = Path.Combine(target, ".complete");
        if (File.Exists(marker))
            return target;

        if (Directory.Exists(target))
            Directory.Delete(target, true);
        Directory.CreateDirectory(target);

        using (var stream = assembly.GetManifestResourceStream("payload.zip"))
        using (var zip = new ZipArchive(stream, ZipArchiveMode.Read))
        {
            int done = 0;
            foreach (var entry in zip.Entries)
            {
                aProgress(100 * done++ / zip.Entries.Count);
                var path = Path.Combine(target, entry.FullName.Replace('/', '\\'));
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                if (entry.Name.Length != 0)
                    entry.ExtractToFile(path, true);
            }
        }

        File.WriteAllText(marker, build);
        return target;
    }
}

// Der Starter des Hosts beantwortet über TCP die Versionsfrage und gibt einen Spielstand frei,
// auf derselben Portnummer wie das Spiel, das nur UDP nutzt.
static class SaveShare
{
    const string Magic = "FTR2";
    const long MaxSize = 1L << 30;
    public static int Port = Program.DefaultPort;

    public static string SavesDir()
    {
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), @"My Games\Fallout4\Saves");
    }

    static string HostsFile()
    {
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"FalloutTogetherReborn\hosts.txt");
    }

    // "Save12_E0D11350_..." liefert die Figuren-ID E0D11350
    static string CharacterId(string aFile)
    {
        var match = Regex.Match(Path.GetFileName(aFile), "^[A-Za-z]+[0-9]*_([0-9A-Fa-f]{8})_");
        return match.Success ? match.Groups[1].Value.ToUpperInvariant() : null;
    }

    // Spielstände einer übernommenen Figur gehören dem Host, zum Beispiel Autosaves aus seiner Welt
    public static void RememberHost(string aFile)
    {
        var id = CharacterId(aFile);
        if (id != null)
            File.AppendAllText(HostsFile(), id + Environment.NewLine);
    }

    public static string Newest()
    {
        var dir = new DirectoryInfo(SavesDir());
        if (!dir.Exists)
            return null;
        var hosts = new HashSet<string>(File.Exists(HostsFile()) ? File.ReadAllLines(HostsFile()) : new string[0]);
        FileInfo best = null;
        foreach (var file in dir.GetFiles("*.fos"))
            if ((best == null || file.LastWriteTimeUtc > best.LastWriteTimeUtc) && !hosts.Contains(CharacterId(file.Name) ?? ""))
                best = file;
        return best == null ? null : best.FullName;
    }

    static string ReadLine(Stream aStream)
    {
        var bytes = new List<byte>();
        while (bytes.Count < 1024)
        {
            int b = aStream.ReadByte();
            if (b < 0 || b == 10)
                break;
            bytes.Add((byte)b);
        }
        return Encoding.UTF8.GetString(bytes.ToArray());
    }

    static void WriteLine(Stream aStream, string aText)
    {
        var bytes = Encoding.UTF8.GetBytes(aText + "\n");
        aStream.Write(bytes, 0, bytes.Length);
    }

    public static TcpListener Serve(string aFile, string aPassword, string aBuild, string aGameVersion)
    {
        var listener = new TcpListener(IPAddress.Any, Port);
        listener.Start();
        new Thread(() =>
        {
            try
            {
                while (true)
                {
                    var client = listener.AcceptTcpClient();
                    new Thread(() => Send(client, aFile, aPassword, aBuild, aGameVersion)) { IsBackground = true }.Start();
                }
            }
            catch (SocketException) { }
            catch (ObjectDisposedException) { }
        }) { IsBackground = true }.Start();
        return listener;
    }

    static void Send(TcpClient aClient, string aFile, string aPassword, string aBuild, string aGameVersion)
    {
        try
        {
            using (aClient)
            using (var stream = aClient.GetStream())
            {
                aClient.ReceiveTimeout = 10000;
                var request = ReadLine(stream);
                if (request == Magic + " HELLO")
                {
                    WriteLine(stream, aBuild + " " + aGameVersion);
                    return;
                }
                if (aFile == null || request != Magic + " SAVE " + aPassword)
                {
                    WriteLine(stream, "NO");
                    return;
                }
                using (var file = new FileStream(aFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    WriteLine(stream, "OK " + file.Length + " " + Path.GetFileName(aFile));
                    file.CopyTo(stream);
                }
            }
        }
        catch (IOException) { }
        catch (SocketException) { }
    }

    // Das Spiel wertet den Dateinamen aus (Figur, Ort, Zeit), er bleibt also der des Hosts. Die Zeit darin wird
    // aktualisiert, damit "Weiter" diese Datei nimmt und kein Spielstand des Mitspielers überschrieben wird.
    static string LocalName(string aName)
    {
        var name = Path.GetFileName(aName);
        if (!name.EndsWith(".fos", StringComparison.OrdinalIgnoreCase) || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            throw new IOException("Der Host hat einen ungültigen Dateinamen gesendet.");
        var now = DateTime.UtcNow.ToString("yyyyMMddHHmmss");
        return Regex.IsMatch(name, "_\\d{14}_") ? Regex.Replace(name, "_\\d{14}_", "_" + now + "_") : now + "_" + name;
    }

    static TcpClient Connect(string aHost)
    {
        var client = new TcpClient();
        var connect = client.BeginConnect(aHost, Port, null, null);
        if (!connect.AsyncWaitHandle.WaitOne(10000))
        {
            client.Close();
            throw new IOException("Der Host antwortet nicht. Hat er schon auf Spielen geklickt, und ist TCP-Port " + Program.DefaultPort + " in seiner Firewall frei?");
        }
        try
        {
            client.EndConnect(connect);
        }
        catch (SocketException)
        {
            client.Close();
            throw new IOException("Der Host antwortet nicht. Hat er schon auf Spielen geklickt, und ist TCP-Port " + Program.DefaultPort + " in seiner Firewall frei?");
        }
        return client;
    }

    // Prüft vor dem Spielstart, ob Host und Mitspieler zusammenpassen. Der Server prüft es beim Verbinden noch einmal.
    public static void CheckHost(string aHost, string aBuild, string aGameVersion)
    {
        using (var client = Connect(aHost))
        using (var stream = client.GetStream())
        {
            // Über Steam braucht der erste Aufbau einige Sekunden
            client.ReceiveTimeout = 30000;
            WriteLine(stream, Magic + " HELLO");
            var answer = ReadLine(stream).Split(' ');
            if (answer.Length != 2)
                throw new IOException("Der Host nutzt eine ältere Version von Fallout Together Reborn. Ihr braucht beide dieselbe exe.");
            if (answer[0] != aBuild)
                throw new IOException("Verbindung verweigert: Der Host nutzt Version " + answer[0] + ", du Version " + aBuild + ". Ihr braucht beide dieselbe exe.");
            if (answer[1] != aGameVersion)
                throw new IOException("Verbindung verweigert: Der Host spielt Fallout 4 " + answer[1].Replace('-', '.') + ", du " + aGameVersion.Replace('-', '.') + ". Ihr braucht dieselbe Spielversion.");
        }
    }

    public static string Fetch(string aHost, string aPassword, string aDir, Action<long, long> aProgress)
    {
        var temp = Path.Combine(aDir, "ftr-download.part");
        try
        {
            string name;
            using (var client = Connect(aHost))
            {
                client.ReceiveTimeout = 60000;
                using (var stream = client.GetStream())
                {
                    WriteLine(stream, Magic + " SAVE " + aPassword);
                    var answer = ReadLine(stream).Split(new[] { ' ' }, 3);
                    long size;
                    if (answer.Length != 3 || answer[0] != "OK" || !long.TryParse(answer[1], out size) || size <= 0 || size > MaxSize)
                        throw new IOException("Der Host gibt keinen Spielstand frei, oder das Passwort stimmt nicht.");
                    name = LocalName(answer[2]);

                    Directory.CreateDirectory(aDir);
                    using (var file = File.Create(temp))
                    {
                        var buffer = new byte[1 << 16];
                        int read;
                        while ((read = stream.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            file.Write(buffer, 0, read);
                            aProgress(file.Length, size);
                        }
                        if (file.Length != size)
                            throw new IOException("Die Übertragung wurde abgebrochen.");
                    }
                }
            }

            using (var file = File.OpenRead(temp))
            {
                var magic = new byte[12];
                if (file.Read(magic, 0, 12) != 12 || Encoding.ASCII.GetString(magic) != "FO4_SAVEGAME")
                    throw new IOException("Die empfangene Datei ist kein Fallout-4-Spielstand.");
            }

            var target = Path.Combine(aDir, name);
            File.Move(temp, target);
            return target;
        }
        finally
        {
            if (File.Exists(temp))
                File.Delete(temp);
        }
    }
}

static class Pip
{
    public static readonly Color Back = Color.FromArgb(7, 18, 11);
    public static readonly Color Panel = Color.FromArgb(10, 28, 16);
    public static readonly Color Green = Color.FromArgb(46, 255, 128);
    public static readonly Color Dim = Color.FromArgb(24, 120, 66);
    public static readonly Color Red = Color.FromArgb(255, 96, 72);
    public const string FontName = "Consolas";

    public static Font Text(float aSize, FontStyle aStyle = FontStyle.Regular)
    {
        return new Font(FontName, aSize, aStyle, GraphicsUnit.Pixel);
    }

    public static void Lines(Graphics aGraphics, Rectangle aArea)
    {
        using (var pen = new Pen(Color.FromArgb(16, 46, 255, 128)))
            for (int y = aArea.Top; y < aArea.Bottom; y += 3)
                aGraphics.DrawLine(pen, aArea.Left, y, aArea.Right, y);
    }
}

sealed class PipCheck : CheckBox
{
    public PipCheck(string aText)
    {
        Text = aText;
        AutoSize = false;
        Size = new Size(360, 22);
        Font = Pip.Text(14f);
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(Pip.Back);
        var color = Enabled ? Pip.Green : Pip.Dim;
        using (var pen = new Pen(color))
        using (var brush = new SolidBrush(color))
        {
            var box = new Rectangle(1, 4, 13, 13);
            e.Graphics.DrawRectangle(pen, box);
            if (Checked)
                e.Graphics.FillRectangle(brush, 4, 7, 8, 8);
            e.Graphics.DrawString(Text, Font, brush, 22, 3);
        }
    }

    protected override void OnEnabledChanged(EventArgs e)
    {
        base.OnEnabledChanged(e);
        Invalidate();
    }
}

sealed class PipButton : Button
{
    public PipButton(string aText)
    {
        Text = aText;
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderColor = Pip.Green;
        FlatAppearance.MouseOverBackColor = Pip.Dim;
        FlatAppearance.MouseDownBackColor = Pip.Green;
        BackColor = Pip.Back;
        ForeColor = Pip.Green;
        Font = Pip.Text(14f);
        UseCompatibleTextRendering = true;
        Cursor = Cursors.Hand;
    }

    protected override void OnEnabledChanged(EventArgs e)
    {
        base.OnEnabledChanged(e);
        FlatAppearance.BorderColor = Enabled ? Pip.Green : Pip.Dim;
    }
}

enum StepState { Pending, Active, Done, Failed }

sealed class Step
{
    public string Key;
    public string Text;
    public StepState State;
}

// Zeigt die Schritte des Starts, damit immer sichtbar ist, was gerade passiert und was noch kommt
sealed class StatusView : Control
{
    static readonly string[] Spinner = { "|", "/", "-", "\\" };
    readonly List<Step> m_steps = new List<Step>();
    readonly System.Windows.Forms.Timer m_timer = new System.Windows.Forms.Timer { Interval = 90 };
    string m_detail = "BEREIT.";
    int m_tick;
    int m_percent = -1;

    public StatusView()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        Font = Pip.Text(13f);
        m_timer.Tick += delegate { m_tick++; Invalidate(); };
        m_timer.Start();
    }

    public bool Busy
    {
        get
        {
            foreach (var step in m_steps)
                if (step.State == StepState.Active)
                    return true;
            return false;
        }
    }

    public void SetSteps(string[] aSteps)
    {
        m_steps.Clear();
        foreach (var text in aSteps)
        {
            var parts = text.Split(new[] { '|' }, 2);
            m_steps.Add(new Step { Key = parts[0], Text = parts[1] });
        }
        m_percent = -1;
        Invalidate();
    }

    Step Find(string aKey)
    {
        foreach (var step in m_steps)
            if (step.Key == aKey)
                return step;
        return null;
    }

    public void Mark(string aKey, StepState aState)
    {
        var step = Find(aKey);
        if (step == null)
            return;
        // Was davor noch offen ist, ist damit erledigt, auch wenn das Spiel es nicht gemeldet hat
        if (aState == StepState.Active || aState == StepState.Done)
            foreach (var other in m_steps)
            {
                if (other == step)
                    break;
                if (other.State == StepState.Active || other.State == StepState.Pending)
                    other.State = StepState.Done;
            }
        step.State = aState;
        m_percent = -1;
        Invalidate();
    }

    public void FailActive()
    {
        foreach (var step in m_steps)
            if (step.State == StepState.Active)
                step.State = StepState.Failed;
        Invalidate();
    }

    public void StopActive()
    {
        foreach (var step in m_steps)
            if (step.State == StepState.Active)
                step.State = StepState.Done;
        Invalidate();
    }

    public void Detail(string aText, int aPercent = -1)
    {
        m_detail = aText;
        m_percent = aPercent;
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.Clear(Pip.Panel);
        Pip.Lines(g, ClientRectangle);
        // langsam wandernder Bildschirmstreifen
        int band = (m_tick * 3) % (Height + 40) - 40;
        using (var brush = new SolidBrush(Color.FromArgb(14, 46, 255, 128)))
            g.FillRectangle(brush, 0, band, Width, 40);

        using (var green = new SolidBrush(Pip.Green))
        using (var dim = new SolidBrush(Pip.Dim))
        using (var red = new SolidBrush(Pip.Red))
        {
            int y = 8;
            foreach (var step in m_steps)
            {
                string mark;
                Brush brush;
                switch (step.State)
                {
                    case StepState.Active: mark = "[" + Spinner[m_tick % 4] + "]"; brush = green; break;
                    case StepState.Done: mark = "[X]"; brush = green; break;
                    case StepState.Failed: mark = "[!]"; brush = red; break;
                    default: mark = "[ ]"; brush = dim; break;
                }
                g.DrawString(mark + " " + step.Text, Font, brush, 10, y);
                y += 17;
            }

            var failed = false;
            foreach (var step in m_steps)
                failed |= step.State == StepState.Failed;
            var format = new StringFormat { Trimming = StringTrimming.EllipsisCharacter };
            g.DrawString("> " + m_detail + (Busy && m_tick / 6 % 2 == 0 ? "_" : ""), Font, failed ? red : green,
                new RectangleF(10, Height - 56, Width - 20, 34), format);
        }

        var bar = new Rectangle(10, Height - 18, Width - 20, 10);
        using (var pen = new Pen(Pip.Dim))
            g.DrawRectangle(pen, bar);
        using (var brush = new SolidBrush(Pip.Green))
        {
            const int cells = 40;
            int cell = (bar.Width - 2) / cells;
            for (int i = 0; i < cells; i++)
            {
                bool on = m_percent >= 0 ? i * 100 / cells < m_percent : Busy && ((i + cells * 8 - m_tick) % 14) < 5;
                if (on)
                    g.FillRectangle(brush, bar.X + 2 + i * cell, bar.Y + 2, cell - 2, bar.Height - 3);
            }
        }
    }

    protected override void Dispose(bool aDisposing)
    {
        if (aDisposing)
            m_timer.Dispose();
        base.Dispose(aDisposing);
    }
}

sealed class MainForm : Form
{
    [DllImport("user32.dll")]
    static extern bool ReleaseCapture();
    [DllImport("user32.dll")]
    static extern IntPtr SendMessage(IntPtr aWindow, int aMessage, IntPtr aParam1, IntPtr aParam2);

    readonly TextBox m_address = new TextBox();
    readonly TextBox m_password = new TextBox { UseSystemPasswordChar = true };
    readonly PipCheck m_host = new PipCheck("Server auf diesem PC starten");
    readonly PipCheck m_share = new PipCheck("Meinen Spielstand freigeben") { Enabled = false };
    readonly PipCheck m_takeSave = new PipCheck("Spielstand vom Host übernehmen");
    readonly PipCheck m_keepFigure = new PipCheck("Eigene Figur mitnehmen") { Enabled = false, Checked = true };
    readonly PipCheck m_mainMenu = new PipCheck("Hauptmenü anzeigen");
    readonly PipCheck m_steam = new PipCheck("Über Steam erreichbar") { Enabled = false, Size = new Size(240, 22) };
    readonly PipButton m_pickSave = new PipButton("Wählen");
    readonly Label m_gameLabel = new Label { AutoEllipsis = true };
    readonly Label m_saveLabel = new Label { AutoEllipsis = true };
    readonly PipButton m_play = new PipButton("SPIELEN");
    readonly StatusView m_view = new StatusView();
    readonly string m_settingsPath;
    string m_game;
    Process m_server;
    TcpListener m_shareListener;
    Process m_tunnel;
    string m_sharedSave;

    public MainForm(string aGame)
    {
        Text = Program.Title;
        ClientSize = new Size(560, 708);
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Pip.Back;
        ForeColor = Pip.Green;
        Font = Pip.Text(14f);
        DoubleBuffered = true;
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch (Exception) { }

        var root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "FalloutTogetherReborn");
        Directory.CreateDirectory(root);
        m_settingsPath = Path.Combine(root, "settings.txt");

        var close = new PipButton("X") { Bounds = new Rectangle(522, 12, 26, 26), TabStop = false };
        close.Click += delegate { Close(); };
        var minimize = new PipButton("_") { Bounds = new Rectangle(490, 12, 26, 26), TabStop = false };
        minimize.Click += delegate { WindowState = FormWindowState.Minimized; };
        Controls.Add(close);
        Controls.Add(minimize);

        MouseDown += delegate(object aSender, MouseEventArgs aArgs)
        {
            if (aArgs.Button != MouseButtons.Left || aArgs.Y > 76)
                return;
            ReleaseCapture();
            SendMessage(Handle, 0xA1, (IntPtr)2, IntPtr.Zero);
        };

        var change = new PipButton("Ändern") { Bounds = new Rectangle(452, 96, 92, 26) };
        change.Click += delegate { PickGame(); };
        m_gameLabel.Bounds = new Rectangle(18, 100, 425, 20);
        Controls.Add(m_gameLabel);
        Controls.Add(change);

        Controls.Add(new Label { Text = "IP/STEAM-ID", Bounds = new Rectangle(18, 155, 100, 20) });
        Controls.Add(Boxed(m_address, new Rectangle(118, 150, 426, 26)));
        Controls.Add(new Label { Text = "PASSWORT", Bounds = new Rectangle(18, 189, 96, 20) });
        Controls.Add(Boxed(m_password, new Rectangle(118, 184, 426, 26)));

        m_host.Location = new Point(18, 243);
        m_host.CheckedChanged += delegate { UpdateOptions(); };
        m_steam.Location = new Point(300, 243);
        Controls.Add(m_steam);
        m_share.Location = new Point(38, 265);
        m_share.CheckedChanged += delegate { UpdateOptions(); };
        m_takeSave.Location = new Point(18, 291);
        m_takeSave.CheckedChanged += delegate { UpdateOptions(); };
        m_keepFigure.Location = new Point(38, 313);
        m_keepFigure.CheckedChanged += delegate { UpdateOptions(); };
        m_mainMenu.Location = new Point(18, 339);
        foreach (var box in new[] { m_host, m_share, m_takeSave, m_keepFigure, m_mainMenu })
            Controls.Add(box);

        m_saveLabel.Bounds = new Rectangle(18, 372, 425, 20);
        Controls.Add(m_saveLabel);
        m_pickSave.Bounds = new Rectangle(452, 368, 92, 26);
        m_pickSave.Click += delegate { PickSave(); };
        Controls.Add(m_pickSave);

        m_play.Bounds = new Rectangle(16, 404, 528, 40);
        m_play.Font = Pip.Text(18f, FontStyle.Bold);
        m_play.Click += delegate { Start(); };
        Controls.Add(m_play);

        m_view.Bounds = new Rectangle(16, 472, 528, 222);
        Controls.Add(m_view);

        LoadSettings();
        // Ein per --game übergebener Ordner gilt nur für diesen Start
        m_game = Game.IsGameDir(aGame) ? aGame : (Game.IsGameDir(m_game) ? m_game : Game.Find());
        ShowGame();
        UpdateOptions();
        FormClosed += delegate { StopServer(); StopShare(); StopTunnel(); };
    }

    static Panel Boxed(TextBox aBox, Rectangle aBounds)
    {
        var frame = new Panel { Bounds = aBounds, BackColor = Pip.Dim };
        var inner = new Panel { Bounds = new Rectangle(1, 1, aBounds.Width - 2, aBounds.Height - 2), BackColor = Pip.Panel };
        aBox.BorderStyle = BorderStyle.None;
        aBox.BackColor = Pip.Panel;
        aBox.ForeColor = Pip.Green;
        aBox.Bounds = new Rectangle(6, 5, inner.Width - 12, 18);
        inner.Controls.Add(aBox);
        frame.Controls.Add(inner);
        return frame;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        using (var green = new SolidBrush(Pip.Green))
        using (var dim = new SolidBrush(Pip.Dim))
        using (var pen = new Pen(Pip.Green))
        using (var title = Pip.Text(26f, FontStyle.Bold))
        using (var small = Pip.Text(13f))
        {
            g.DrawRectangle(pen, 0, 0, ClientSize.Width - 1, ClientSize.Height - 1);
            g.DrawString("FALLOUT TOGETHER REBORN", title, green, 14, 12);
            g.DrawString("CO-OP-TERMINAL  //  VERSION " + Payload.Build(), small, dim, 16, 48);
            g.DrawLine(pen, 12, 74, ClientSize.Width - 13, 74);
            Heading(g, "SYSTEM", 80);
            Heading(g, "VERBINDUNG", 132);
            Heading(g, "OPTIONEN", 222);
            Heading(g, "STATUS", 456);
        }
    }

    void Heading(Graphics aGraphics, string aText, int aY)
    {
        using (var green = new SolidBrush(Pip.Green))
        using (var pen = new Pen(Pip.Dim))
        using (var font = Pip.Text(12f, FontStyle.Bold))
        {
            aGraphics.DrawString(aText, font, green, 16, aY - 1);
            int x = 24 + (int)aGraphics.MeasureString(aText, font).Width;
            aGraphics.DrawLine(pen, x, aY + 7, ClientSize.Width - 17, aY + 7);
        }
    }

    void UpdateOptions()
    {
        m_address.Enabled = !m_host.Checked;
        m_share.Enabled = m_host.Checked;
        m_steam.Enabled = m_host.Checked;
        m_pickSave.Enabled = m_host.Checked || !m_takeSave.Checked || m_keepFigure.Checked;
        m_takeSave.Enabled = !m_host.Checked;
        m_keepFigure.Enabled = !m_host.Checked && m_takeSave.Checked;
        ShowSave();
    }

    void ShowSave()
    {
        var save = m_sharedSave ?? SaveShare.Newest();
        var prefix = !m_host.Checked && m_takeSave.Checked ? "EIGENE FIGUR: " : "SPIELSTAND: ";
        m_saveLabel.Text = !m_pickSave.Enabled ? "SPIELSTAND: VOM HOST"
            : prefix + (save == null ? "keiner gefunden" : (m_sharedSave == null ? "neuester, " : "") + Path.GetFileName(save));
    }

    void PickSave()
    {
        using (var dialog = new OpenFileDialog { Filter = "Fallout-4-Spielstand (*.fos)|*.fos", InitialDirectory = SaveShare.SavesDir() })
            if (dialog.ShowDialog(this) == DialogResult.OK)
            {
                m_sharedSave = dialog.FileName;
                ShowSave();
            }
    }

    void LoadSettings()
    {
        if (!File.Exists(m_settingsPath))
            return;
        foreach (var line in File.ReadAllLines(m_settingsPath))
        {
            var parts = line.Split(new[] { '=' }, 2);
            if (parts.Length != 2) continue;
            if (parts[0] == "address") m_address.Text = parts[1];
            if (parts[0] == "game") m_game = parts[1];
        }
    }

    void Save()
    {
        File.WriteAllLines(m_settingsPath, new[] { "address=" + m_address.Text.Trim(), "game=" + (m_game ?? "") });
    }

    void PickGame()
    {
        using (var dialog = new FolderBrowserDialog { Description = "Ordner mit Fallout4.exe auswählen" })
        {
            if (dialog.ShowDialog(this) != DialogResult.OK)
                return;
            if (!Game.IsGameDir(dialog.SelectedPath))
            {
                MessageBox.Show(this, "In diesem Ordner liegt keine Fallout4.exe.", Program.Title);
                return;
            }
            m_game = dialog.SelectedPath;
            ShowGame();
        }
    }

    void ShowGame()
    {
        m_gameLabel.Text = Game.IsGameDir(m_game)
            ? "FALLOUT 4 " + Game.Version(m_game).Replace('-', '.') + " GEFUNDEN"
            : "FALLOUT 4 NICHT GEFUNDEN, BITTE ORDNER WÄHLEN";
    }

    void Start()
    {
        if (!Game.IsGameDir(m_game))
        {
            PickGame();
            if (!Game.IsGameDir(m_game))
                return;
        }

        var address = m_address.Text.Trim();
        if (!m_host.Checked && address.Length == 0)
        {
            MessageBox.Show(this, "Bitte die Adresse oder Steam-ID des Hosts eintragen.", Program.Title);
            return;
        }

        if (Process.GetProcessesByName("steam").Length == 0)
        {
            MessageBox.Show(this, "Bitte zuerst Steam starten und anmelden.", Program.Title);
            return;
        }

        m_play.Enabled = false;
        Save();
        var game = m_game;
        var password = m_password.Text;
        var host = m_host.Checked;
        var useSteam = host ? m_steam.Checked : Regex.IsMatch(address, "^[0-9]{17}$");
        var picked = m_sharedSave;
        var chosen = picked ?? SaveShare.Newest();
        var shared = host && m_share.Checked ? chosen : null;
        if (host && m_share.Checked && shared == null)
        {
            MessageBox.Show(this, "Es wurde kein Spielstand zum Freigeben gefunden.", Program.Title);
            m_play.Enabled = true;
            return;
        }
        var takeSave = !host && m_takeSave.Checked;
        var keepFigure = takeSave && m_keepFigure.Checked;
        var skipMenu = !m_mainMenu.Checked;
        var autoLoad = skipMenu ? chosen : null;

        var steps = new List<string> { "setup|Dateien einrichten" };
        if (host)
            steps.Add("server|Server starten");
        if (useSteam)
            steps.Add("tunnel|Steam-Verbindung aufbauen");
        if (!host)
            steps.Add("check|Host und Version prüfen");
        if (shared != null)
            steps.Add("share|Spielstand freigeben");
        if (takeSave)
            steps.Add("fetch|Spielstand vom Host holen");
        if (keepFigure)
            steps.Add("figure|Eigene Figur lesen, Spiel startet zweimal");
        steps.Add("game|Fallout 4 starten");
        steps.Add("load|Spielstand laden");
        steps.Add("connect|Mit dem Server verbinden");
        if (keepFigure)
            steps.Add("apply|Eigene Figur einsetzen");
        steps.Add("play|Im Spiel");
        m_view.SetSteps(steps.ToArray());

        new Thread(() => Run(game, address, password, host, shared, takeSave, keepFigure, autoLoad, skipMenu, picked, useSteam)) { IsBackground = true }.Start();
    }

    void Run(string aGame, string aAddress, string aPassword, bool aHost, string aShared, bool aTakeSave, bool aKeepFigure, string aAutoLoad, bool aSkipMenu, string aPicked, bool aSteam)
    {
        string received = null;
        try
        {
            Step("setup", "Prüfe die Dateien von Fallout Together Reborn.");
            var dir = Payload.Prepare(aPercent => Detail("Dateien werden entpackt.", aPercent));
            // Läuft noch etwas aus einem alten Ordner, wartet das Aufräumen bis zum nächsten Start
            if (Process.GetProcessesByName("FalloutTogether").Length == 0 && Process.GetProcessesByName("FalloutTogetherServer").Length == 0
                && Process.GetProcessesByName("TPProcess").Length == 0)
            {
                Detail("Alte Versionen werden aufgeräumt.");
                Payload.RemoveOldBuilds(Path.GetDirectoryName(dir), dir);
            }

            var version = Game.Version(aGame);
            if (!File.Exists(Path.Combine(dir, "offsets-" + version + ".bin")))
            {
                Fail("Fallout 4 " + version.Replace('-', '.') + " wird von dieser Version noch nicht unterstützt.\n\n"
                     + "Du brauchst eine neue Fallout-Together-Reborn-exe, die zu deinem Spiel passt.");
                return;
            }

            if (aHost)
            {
                Step("server", "Der Server startet, Mitspieler können sich gleich verbinden.");
                StartServer(dir, aPassword);
                aAddress = "127.0.0.1";
                m_shareListener = SaveShare.Serve(aShared, aPassword, Payload.Build(), version);
                if (aSteam)
                {
                    Step("tunnel", "Verbinde mit dem Steam-Netz.");
                    var ownId = StartTunnel(aGame, "0")[1];
                    Ui(delegate { Clipboard.SetText(ownId); });
                    Detail("Deine Steam-ID steht in der Zwischenablage, gib sie deinem Mitspieler: " + ownId);
                }
                if (aShared != null)
                    Step("share", "Mitspieler können " + Path.GetFileName(aShared) + " abholen.");
            }
            else
            {
                if (aSteam)
                {
                    Step("tunnel", "Verbinde mit dem Steam-Netz.");
                    var ready = StartTunnel(aGame, aAddress);
                    aAddress = "127.0.0.1:" + ready[2];
                    SaveShare.Port = int.Parse(ready[3]);
                }
                Step("check", "Frage den Host nach seiner Version.");
                var host = aAddress.Split(':')[0];
                SaveShare.CheckHost(host, Payload.Build(), version);
                if (aTakeSave)
                {
                    Step("fetch", "Der Spielstand wird übertragen.");
                    received = SaveShare.Fetch(host, aPassword, SaveShare.SavesDir(),
                        (aDone, aSize) => Detail(string.Format("{0} von {1} MB übertragen.", aDone >> 20, aSize >> 20), (int)(100 * aDone / aSize)));
                    SaveShare.RememberHost(received);
                }
            }
            if (aAddress.IndexOf(':') < 0)
                aAddress += ":" + Program.DefaultPort;

            var figure = Path.Combine(Path.GetDirectoryName(dir), "figure.bin");
            if (aKeepFigure)
                CaptureFigure(dir, aGame, aAddress, aPassword, figure, aPicked ?? SaveShare.Newest());

            // Sonst wird die Figur beim Spielen mit dem eigenen Spielstand laufend mitgeschrieben
            var mode = !aTakeSave ? "save" : aKeepFigure && File.Exists(figure) ? "apply" : null;
            var autoLoad = aTakeSave ? (aSkipMenu ? received : null) : aAutoLoad;

            Step("game", "Fallout 4 startet, das kann eine Minute dauern.");
            StartGame(dir, aGame, aAddress, aPassword, figure, mode, autoLoad, delegate(string aLine)
            {
                if (aLine.Contains("attached"))
                    Step("load", autoLoad != null ? Path.GetFileName(autoLoad) + " wird geladen."
                        : aTakeSave ? "Klicke im Menü auf Weiter, dann bist du in der Welt des Hosts."
                        : aShared != null ? "Lade jetzt " + Path.GetFileName(aShared) + "."
                        : "Lade einen Spielstand, die Verbindung startet dann automatisch.");
                else if (aLine.Contains("Finished loading"))
                    Step("connect", "Spielstand geladen, verbinde mit " + aAddress + ".");
                else if (aLine.Contains("Connected to server as player"))
                {
                    if (mode == "apply")
                        Step("apply", "Verbunden. Deine Figur wird in 10 Sekunden eingesetzt, dabei verbindet das Spiel kurz neu.");
                    else
                        Step("play", "Verbunden, viel Spaß.");
                }
                else if (aLine.Contains("Applied figure"))
                    Step("play", "Deine Figur ist da, viel Spaß.");
                else if (aLine.Contains("Server refused"))
                    Failed("Der Server hat die Verbindung abgelehnt, die Meldung steht im Spiel.");
                else if (aLine.Contains("Disconnected from server"))
                    Detail("Verbindung zum Server getrennt.");
            });

            StopServer();
            Ui(delegate { m_view.StopActive(); m_view.Detail("SPIEL BEENDET."); });
        }
        catch (Exception e)
        {
            Fail("Start fehlgeschlagen:\n\n" + e.Message);
        }
        finally
        {
            StopServer();
            StopShare();
            StopTunnel();
            SaveShare.Port = Program.DefaultPort;
            try
            {
                if (received != null)
                    File.Delete(received);
            }
            catch (IOException) { }
            Ui(delegate { m_play.Enabled = true; });
        }
    }

    void StartGame(string aDir, string aGame, string aAddress, string aPassword, string aFigure, string aMode, string aAutoLoad, Action<string> aOnLog)
    {
        var info = new ProcessStartInfo(Path.Combine(aDir, "FalloutTogether.exe"), "--exePath \"" + Path.Combine(aGame, "Fallout4.exe") + "\"");
        info.UseShellExecute = false;
        info.WorkingDirectory = aDir;
        info.EnvironmentVariables["TP_AUTOCONNECT"] = aAddress;
        info.EnvironmentVariables["TP_PASSWORD"] = aPassword;
        info.EnvironmentVariables["TP_BUILD"] = Payload.Build();
        info.EnvironmentVariables["TP_FIGURE_FILE"] = aFigure;
        if (aMode != null)
            info.EnvironmentVariables["TP_FIGURE_MODE"] = aMode;
        // Der Name kommt ohne Endung, das Spiel sucht ihn im Spielstandsordner
        if (aAutoLoad != null)
            info.EnvironmentVariables["TP_AUTOLOAD"] = Path.GetFileNameWithoutExtension(aAutoLoad);

        // Das Log ist die einzige Verbindung ins laufende Spiel
        var log = Path.Combine(aDir, @"logs\tp_client.log");
        long start = File.Exists(log) ? new FileInfo(log).Length : 0;
        using (var game = Process.Start(info))
        {
            while (!game.WaitForExit(400))
                start = ReadLog(log, start, aOnLog);
            ReadLog(log, start, aOnLog);
        }
    }

    // Liest nur ganze Zeilen, der Rest kommt beim nächsten Mal. Ein Fehler hier darf nie den Start abbrechen.
    public static long ReadLog(string aPath, long aPosition, Action<string> aOnLog)
    {
        try
        {
            using (var file = new FileStream(aPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
            {
                if (file.Length < aPosition)
                    aPosition = 0;
                file.Position = aPosition;
                var bytes = new byte[file.Length - aPosition];
                int size = file.Read(bytes, 0, bytes.Length);
                int end = Array.LastIndexOf(bytes, (byte)10, size - 1 < 0 ? 0 : size - 1);
                if (size == 0 || end < 0)
                    return aPosition;
                foreach (var line in Encoding.UTF8.GetString(bytes, 0, end + 1).Split('\n'))
                    aOnLog(line);
                return aPosition + end + 1;
            }
        }
        catch (IOException)
        {
            return aPosition;
        }
        catch (UnauthorizedAccessException)
        {
            return aPosition;
        }
    }

    // Erster Start mit dem eigenen Spielstand: Der Client verbindet sich, speichert die Figur und schließt das Spiel.
    // Ein neuer Start ist nur nötig, wenn dieser Spielstand noch nicht oder in einem älteren Stand gespeichert ist.
    void CaptureFigure(string aDir, string aGame, string aAddress, string aPassword, string aFigure, string aOwnSave)
    {
        if (aOwnSave == null)
        {
            Step("figure", "Kein eigener Spielstand gefunden, du spielst die Figur des Hosts.");
            return;
        }

        var source = aFigure + ".source";
        var name = Path.GetFileName(aOwnSave);
        if (File.Exists(aFigure) && File.Exists(source) && File.ReadAllText(source) == name
            && File.GetLastWriteTimeUtc(aFigure) > File.GetLastWriteTimeUtc(aOwnSave))
        {
            Step("figure", "Deine Figur aus " + name + " ist schon gespeichert.");
            return;
        }

        var before = File.Exists(aFigure) ? File.GetLastWriteTimeUtc(aFigure) : DateTime.MinValue;
        Step("figure", "Das Spiel startet jetzt mit " + name + ", liest deine Figur und schließt sich von selbst.");
        StartGame(aDir, aGame, aAddress, aPassword, aFigure, "capture", aOwnSave, delegate(string aLine)
        {
            if (aLine.Contains("Finished loading"))
                Detail("Dein Spielstand ist geladen, verbinde mit dem Server.");
            else if (aLine.Contains("Connected to server as player"))
                Detail("Verbunden. Deine Figur wird gelesen, das Spiel schließt sich gleich.");
            else if (aLine.Contains("Server refused"))
                Detail("Der Server hat die Verbindung abgelehnt, die Figur konnte nicht gelesen werden.");
        });

        if (File.Exists(aFigure) && File.GetLastWriteTimeUtc(aFigure) > before)
            File.WriteAllText(source, name);

        Detail("Figur gelesen. Das Spiel startet gleich mit dem Spielstand des Hosts.");
        // Steam muss das Ende des ersten Starts erst verarbeiten
        Thread.Sleep(3000);
    }

    void StartServer(string aDir, string aPassword)
    {
        var run = Path.Combine(aDir, "server-run");
        Directory.CreateDirectory(run);
        var info = new ProcessStartInfo(Path.Combine(aDir, "FalloutTogetherServer.exe"),
            "--GameServer:uPort=" + Program.DefaultPort + (aPassword.Length > 0 ? " \"--GameServer:sPassword=" + aPassword + "\"" : "") + " --bConsole=false");
        info.UseShellExecute = false;
        info.CreateNoWindow = true;
        info.WorkingDirectory = run;
        info.EnvironmentVariables["TP_BUILD"] = Payload.Build();
        m_server = Process.Start(info);
    }

    // Startet den Tunnel und wartet auf seine Bereitmeldung
    string[] StartTunnel(string aGame, string aRemote)
    {
        var info = new ProcessStartInfo(Application.ExecutablePath, string.Format("--tunnel \"{0}\" {1} {2} {3} {3}",
            aGame.TrimEnd('\\'), Process.GetCurrentProcess().Id, aRemote, Program.DefaultPort));
        info.UseShellExecute = false;
        info.CreateNoWindow = true;
        info.RedirectStandardOutput = true;
        m_tunnel = Process.Start(info);

        // Steam schreibt selbst auf die Ausgabe, darum bis zur eigenen Meldung lesen
        string line = null;
        var tunnel = m_tunnel;
        var reader = new Thread(() =>
        {
            do
            {
                line = tunnel.StandardOutput.ReadLine();
            } while (line != null && !line.StartsWith("READY ") && !line.StartsWith("ERROR "));
        }) { IsBackground = true };
        reader.Start();
        if (!reader.Join(45000))
            throw new IOException("Das Steam-Netz antwortet nicht.");
        if (line == null || !line.StartsWith("READY "))
            throw new IOException(line != null && line.StartsWith("ERROR ") ? line.Substring(6) : "Der Steam-Tunnel ist nicht gestartet, siehe tunnel.log.");
        return line.Split(' ');
    }

    void StopTunnel()
    {
        if (m_tunnel == null)
            return;
        try
        {
            if (!m_tunnel.HasExited)
                m_tunnel.Kill();
        }
        catch (Exception) { }
        m_tunnel = null;
    }

    void StopShare()
    {
        if (m_shareListener != null)
            m_shareListener.Stop();
        m_shareListener = null;
    }

    void StopServer()
    {
        if (m_server == null)
            return;
        try
        {
            if (!m_server.HasExited)
                m_server.Kill();
        }
        catch (Exception) { }
        m_server = null;
    }

    void Ui(Action aAction)
    {
        if (IsHandleCreated && !IsDisposed)
            BeginInvoke(aAction);
    }

    void Step(string aKey, string aDetail)
    {
        Ui(delegate { m_view.Mark(aKey, StepState.Active); m_view.Detail(aDetail); });
    }

    void Detail(string aText, int aPercent = -1)
    {
        Ui(delegate { m_view.Detail(aText, aPercent); });
    }

    void Failed(string aText)
    {
        Ui(delegate { m_view.FailActive(); m_view.Detail(aText); });
    }

    void Fail(string aText)
    {
        Failed(aText.Replace("\n\n", " "));
        Ui(delegate { MessageBox.Show(this, aText, Program.Title); });
    }
}
