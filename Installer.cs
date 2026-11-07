// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

[assembly: AssemblyTitle("暴力摩托：30周年纪念版 高清与分辨率补丁")]
[assembly: AssemblyDescription("Native HD, aspect ratio support and rendering fixes; no localization")]
[assembly: AssemblyVersion("5.0.0.0")]
[assembly: AssemblyFileVersion("5.0.0.0")]

internal static class Installer
{
    static readonly string Root = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
    static readonly string Backup = Path.Combine(Root, "分辨率补丁_安装前备份");
    static readonly string StatePath = Path.Combine(Backup, "state.txt");
    const string ExeName = "RoadRash95.exe", DllName = "RoadRashHD.dll", IniName = "RoadRashHD.ini";
    sealed class State
    {
        public string ExeBefore, DllBefore, ExeInstalled, DllInstalled;
        public byte[] Bytes()
        {
            return Encoding.UTF8.GetBytes("RRHD_BACKUP_V1\n" + ExeBefore + "\n" + DllBefore + "\n" + ExeInstalled + "\n" + DllInstalled + "\n");
        }
    }
    sealed class Operation
    {
        public string Path, Temporary;
        public byte[] Before, After;
    }
    static byte[] Read(string path) { return File.Exists(path) ? File.ReadAllBytes(path) : null; }
    static string Hash(byte[] data) { return data == null ? "-" : RendererPatch.Hash(data); }
    static bool Same(byte[] a, byte[] b) { return Hash(a) == Hash(b); }
    static byte[] Resource(string name)
    {
        using (var source = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
        using (var destination = new MemoryStream())
        {
            if (source == null) throw new InvalidDataException("安装包缺少内嵌文件：" + name);
            source.CopyTo(destination); return destination.ToArray();
        }
    }
    static void Cleanup(string path)
    {
        if (path == null) return;
        try { File.Delete(path); } catch (IOException) { } catch (UnauthorizedAccessException) { }
    }
    static string Stage(string path, byte[] bytes)
    {
        string temporary = Path.Combine(Path.GetDirectoryName(path), ".rrhd-" + Guid.NewGuid().ToString("N") + ".tmp");
        try
        {
            using (var file = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                file.Write(bytes, 0, bytes.Length); file.Flush(true);
            }
            return temporary;
        }
        catch { Cleanup(temporary); throw; }
    }
    static void Preserve(string path, byte[] bytes)
    {
        if (bytes == null) return;
        if (File.Exists(path))
        {
            if (!Same(File.ReadAllBytes(path), bytes)) throw new IOException("已有安装前备份与当前文件不一致，未覆盖备份：" + path);
            return;
        }
        string temporary = Stage(path, bytes);
        try { File.Move(temporary, path); } finally { Cleanup(temporary); }
    }
    static State ReadState()
    {
        if (!File.Exists(StatePath)) return null;
        string[] lines = File.ReadAllLines(StatePath, Encoding.UTF8);
        if (lines.Length != 5 || lines[0] != "RRHD_BACKUP_V1") throw new InvalidDataException("安装前备份记录损坏。");
        var state = new State { ExeBefore = lines[1], DllBefore = lines[2], ExeInstalled = lines[3], DllInstalled = lines[4] };
        if (Hash(Read(Path.Combine(Backup, ExeName))) != state.ExeBefore ||
            Hash(Read(Path.Combine(Backup, DllName))) != state.DllBefore ||
            RendererPatch.NativeHash(state.ExeBefore) != state.ExeInstalled)
            throw new InvalidDataException("安装前备份校验失败，未修改游戏文件。");
        return state;
    }
    static void CheckCurrent(State state, byte[] exe, byte[] dll)
    {
        if (Hash(exe) != state.ExeBefore && Hash(exe) != state.ExeInstalled)
            throw new InvalidOperationException("游戏程序与已有备份记录不一致。未覆盖当前程序。");
        string hash = Hash(dll);
        if (hash != state.DllBefore && hash != state.DllInstalled)
            throw new InvalidOperationException("高清模块已被其他文件替换，未覆盖当前模块。");
    }
    static void Add(List<Operation> operations, string path, byte[] after)
    {
        byte[] before = Read(path);
        if (!Same(before, after)) operations.Add(new Operation { Path = path, Before = before, After = after });
    }
    static List<Operation> Prepare(bool restore)
    {
        string exePath = Path.Combine(Root, ExeName), dllPath = Path.Combine(Root, DllName);
        byte[] exe = Read(exePath), dll = Read(dllPath);
        if (exe == null) throw new InvalidOperationException("找不到 RoadRash95.exe。\n请将本 EXE 放到游戏根目录后运行。");
        string nativeHash = RendererPatch.NativeHash(Hash(exe));
        State state = ReadState();
        var operations = new List<Operation>();
        if (restore)
        {
            if (state == null) throw new InvalidOperationException("没有本安装器的安装前备份，未修改游戏文件。");
            CheckCurrent(state, exe, dll);
            // Remove the game's import before removing a newly installed DLL.
            Add(operations, exePath, Read(Path.Combine(Backup, ExeName)));
            Add(operations, dllPath, Read(Path.Combine(Backup, DllName)));
            return operations;
        }
        byte[] renderer = Resource(DllName), patched = exe;
        if (Hash(exe) != nativeHash)
        {
            string temporary = Stage(exePath, exe);
            try { patched = RendererPatch.Create(temporary); } finally { Cleanup(temporary); }
        }
        if (state == null)
        {
            Directory.CreateDirectory(Backup);
            // Reuse only identical leftovers from a previously interrupted
            // attempt; never overwrite someone else's earlier backup.
            if (dll == null && File.Exists(Path.Combine(Backup, DllName)))
                throw new IOException("安装前备份内存在其他高清模块，未覆盖备份。");
            Preserve(Path.Combine(Backup, ExeName), exe); Preserve(Path.Combine(Backup, DllName), dll);
            state = new State { ExeBefore = Hash(exe), DllBefore = Hash(dll) };
        }
        else CheckCurrent(state, exe, dll);
        state.ExeInstalled = Hash(patched); state.DllInstalled = Hash(renderer);
        // The dependency is present before the game imports it. Existing user
        // resolution choices are preserved, including during reinstallation.
        Add(operations, dllPath, renderer);
        Add(operations, exePath, patched);
        string ini = Path.Combine(Root, IniName);
        if (!File.Exists(ini)) Add(operations, ini, Resource(IniName));
        Add(operations, StatePath, state.Bytes());
        return operations;
    }
    static void SetFile(string path, string temporary, byte[] after)
    {
        if (after == null) File.Delete(path);
        else if (File.Exists(path)) File.Replace(temporary, path, null);
        else File.Move(temporary, path);
    }
    static void Apply(List<Operation> operations)
    {
        var applied = new List<Operation>();
        try
        {
            foreach (var op in operations) if (op.After != null) op.Temporary = Stage(op.Path, op.After);
            foreach (var op in operations)
            {
                if (!Same(Read(op.Path), op.Before)) throw new IOException("安装期间文件发生变化，请退出游戏后重试。");
                SetFile(op.Path, op.Temporary, op.After); applied.Add(op);
            }
            foreach (var op in operations) if (!Same(Read(op.Path), op.After)) throw new IOException("写入后校验失败。");
        }
        catch (Exception error)
        {
            var failed = new List<string>();
            for (int i = applied.Count - 1; i >= 0; --i)
            {
                var op = applied[i]; string temporary = null;
                try
                {
                    if (op.Before != null) temporary = Stage(op.Path, op.Before);
                    SetFile(op.Path, temporary, op.Before);
                }
                catch { failed.Add(op.Path); }
                finally { Cleanup(temporary); }
            }
            if (failed.Count > 0) throw new IOException("操作失败，以下文件需从安装前备份手动恢复：\n" + string.Join("\n", failed.ToArray()) + "\n" + error.Message);
            throw new IOException("操作未完成，已还原本次替换的文件。\n" + error.Message, error);
        }
        finally { foreach (var op in operations) Cleanup(op.Temporary); }
    }
    static PatchResult Finish(string message, bool error)
    {
        string log = message + "\n\n许可：PolyForm Noncommercial License 1.0.0\nhttps://polyformproject.org/licenses/noncommercial/1.0.0";
        try { File.WriteAllText(Path.Combine(Root, "RoadRash_HD_Setup.log"), log + Environment.NewLine, Encoding.UTF8); }
        catch (IOException) { } catch (UnauthorizedAccessException) { }
        return new PatchResult(!error, message);
    }
    static PatchResult Execute(bool restore)
    {
        try
        {
            foreach (var process in Process.GetProcessesByName("RoadRash95"))
            {
                process.Dispose(); throw new InvalidOperationException("请先退出暴力摩托，再运行本补丁。");
            }
            var operations = Prepare(restore);
            if (operations.Count == 0)
            {
                return Finish(restore ? "已经恢复安装前状态。" : "已经安装本版本高清支持，现有设置已保留。", false);
            }
            Apply(operations);
            return Finish(restore ? "已恢复安装前程序和模块。分辨率配置文件已保留。" :
                "高清、分辨率与渲染补丁安装完成。\n请从 Steam 启动游戏。\n在“画面与声音 → 比赛分辨率”选择原生或其他分辨率。\n英文菜单对应 Graphics and Sound → Race Resolution。\n\n已有汉化、分辨率设置和存档保持不变。", false);
        }
        catch (Exception error)
        {
            string message = error.Message;
            if (error is UnauthorizedAccessException || error.InnerException is UnauthorizedAccessException)
                message += "\n当前目录没有写入权限，请以管理员身份运行本补丁。";
            return Finish(message, true);
        }
    }
    static Form CreateWindow()
    {
        return new PatchWindow("暴力摩托：30周年纪念版 高清与分辨率补丁", Execute);
    }
    [STAThread]
    static int Main(string[] args)
    {
        bool quiet = Array.Exists(args, a => string.Equals(a, "/quiet", StringComparison.OrdinalIgnoreCase)), restore = false;
        foreach (string arg in args)
        {
            if (string.Equals(arg, "/restore", StringComparison.OrdinalIgnoreCase)) restore = true;
            else if (!string.Equals(arg, "/quiet", StringComparison.OrdinalIgnoreCase))
            {
                var invalid = Finish("用法：双击打开安装与还原窗口；/restore 直接还原；/quiet 静默执行。", true);
                if (!quiet) MessageBox.Show(invalid.Message, "暴力摩托：30周年纪念版 高清与分辨率补丁", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }
        if (quiet || restore)
        {
            var result = Execute(restore);
            if (!quiet) MessageBox.Show(result.Message, "暴力摩托：30周年纪念版 高清与分辨率补丁", MessageBoxButtons.OK,
                                        result.Success ? MessageBoxIcon.Information : MessageBoxIcon.Error);
            return result.Success ? 0 : 1;
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        using (var window = CreateWindow()) Application.Run(window);
        return 0;
    }
}
