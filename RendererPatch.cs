// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

internal static class RendererPatch
{
    // Steam build 26105967: English base, and the separately released Chinese
    // localization. No translated text or original executable is embedded here.
    static readonly Dictionary<string, string> Versions = new Dictionary<string, string>
    {
        { "9e40e25737149ab095061b7154797a35495faea97644fafc5a1e616f1400c968", "8a91e80e70b4ec1e764676712e9e78ec7b718562b853232ab60e3e24bf842a1a" },
        { "5590703f9bec56ed66c63f27d61a910d751cdf5cdaa0ce32e08a669de9c8216f", "dcefc1f0ca18f62a2fb752b5d98806989229ab2db891607e0a0ec5fba292b97d" }
    };
    public static string Hash(byte[] data)
    {
        using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "").ToLowerInvariant();
    }
    public static string NativeHash(string hash)
    {
        string native;
        if (Versions.TryGetValue(hash, out native)) return native;
        if (Versions.ContainsValue(hash)) return hash;
        throw new InvalidOperationException("游戏程序版本不匹配。\n支持 Steam build 26105967 英文原版、独立纯汉化版，以及对应的 v4 高清版。\n未修改游戏文件。");
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr BeginUpdateResourceW(string file, bool deleteExisting);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool UpdateResourceW(IntPtr handle, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool EndUpdateResourceW(IntPtr handle, bool discard);

    sealed class PE
    {
        public byte[] Data;
        public int Header, Optional, Sections, Count;
        public PE(byte[] data)
        {
            Data = data; Header = I(60); Optional = Header + 24;
            Count = U16(Header + 6); Sections = Optional + U16(Header + 20);
            if (U16(Header + 4) != 0x14c || U16(Optional) != 0x10b || I(Optional + 28) != 0x400000)
                throw new InvalidDataException("不是支持的 x86 游戏程序。");
        }
        public int I(int offset) { return BitConverter.ToInt32(Data, offset); }
        public int U16(int offset) { return BitConverter.ToUInt16(Data, offset); }
        public void Put(int offset, int value) { Buffer.BlockCopy(BitConverter.GetBytes(value), 0, Data, offset, 4); }
        public void Put16(int offset, int value) { Buffer.BlockCopy(BitConverter.GetBytes((ushort)value), 0, Data, offset, 2); }
        public int Offset(int rva)
        {
            for (int i = 0; i < Count; ++i)
            {
                int s = Sections + i * 40, va = I(s + 12), size = I(s + 16);
                if (rva >= va && rva < va + size) return I(s + 20) + rva - va;
            }
            throw new InvalidDataException("无效的 PE 地址。");
        }
        int ResourceChild(int root, int relative, int id, bool directory)
        {
            int entry = root + relative, count = U16(entry + 12) + U16(entry + 14);
            for (int i = 0; i < count; ++i)
            {
                int p = entry + 16 + i * 8;
                if (I(p) != id) continue;
                int child = I(p + 4);
                if ((child < 0) != directory) throw new InvalidDataException("资源层级不匹配。");
                return child & 0x7fffffff;
            }
            throw new InvalidDataException("未找到原版 DPI 清单。");
        }
        public byte[] Manifest()
        {
            int root = Offset(I(Optional + 112));
            int type = ResourceChild(root, 0, 24, true);
            int name = ResourceChild(root, type, 1, true);
            int data = root + ResourceChild(root, name, 1033, false);
            byte[] result = new byte[I(data + 4)];
            Buffer.BlockCopy(Data, Offset(I(data)), result, 0, result.Length);
            return result;
        }
    }
    static void SetDpiAware(string file)
    {
        var image = new PE(File.ReadAllBytes(file));
        string source = Encoding.UTF8.GetString(image.Manifest());
        if (source.Contains("dpiAware") || !source.Contains("</assembly>")) throw new InvalidDataException("DPI 清单不匹配。");
        const string application = "  <application xmlns=\"urn:schemas-microsoft-com:asm.v3\">\n" +
            "    <windowsSettings>\n" +
            "      <dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>\n" +
            "    </windowsSettings>\n" +
            "  </application>\n";
        byte[] contents = Encoding.UTF8.GetBytes(source.Replace("</assembly>", application + "</assembly>"));
        IntPtr handle = BeginUpdateResourceW(file, false);
        if (handle == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        if (!UpdateResourceW(handle, new IntPtr(24), new IntPtr(1), 1033, contents, (uint)contents.Length))
        {
            int error = Marshal.GetLastWin32Error(); EndUpdateResourceW(handle, true); throw new Win32Exception(error);
        }
        if (!EndUpdateResourceW(handle, false)) throw new Win32Exception(Marshal.GetLastWin32Error());
    }
    static int Align(int value, int alignment) { return (value + alignment - 1) / alignment * alignment; }
    static byte[] Integers(params int[] values)
    {
        byte[] result = new byte[values.Length * 4];
        Buffer.BlockCopy(values, 0, result, 0, result.Length); return result;
    }
    sealed class Section
    {
        public int Rva;
        public readonly List<byte> Bytes = new List<byte>();
        public int Add(byte[] data, int alignment)
        {
            while (Bytes.Count % alignment != 0) Bytes.Add(0);
            int address = Rva + Bytes.Count; Bytes.AddRange(data); return address;
        }
    }
    // Only the temporary copy is passed here. The verified output is committed
    // by the installer after the DLL and backup are prepared.
    public static byte[] Create(string temporary)
    {
        byte[] original = File.ReadAllBytes(temporary);
        string hash = Hash(original), expectedHash = NativeHash(hash);
        if (hash == expectedHash) return original;
        SetDpiAware(temporary);
        var image = new PE(File.ReadAllBytes(temporary));
        int opt = image.Optional, sectionHeaders = image.Sections, count = image.Count;
        int fileAlignment = image.I(opt + 36), sectionAlignment = image.I(opt + 32), end = 0;
        for (int i = 0; i < count; ++i)
        {
            int s = sectionHeaders + i * 40;
            if (Encoding.ASCII.GetString(image.Data, s, 8).StartsWith(".rrhd")) throw new InvalidDataException("已存在其他高清模块。");
            end = Math.Max(end, image.I(s + 12) + Math.Max(image.I(s + 8), image.I(s + 16)));
        }
        int[] calls = { 0x40132f, 0x4688e0, 0x404525, 0x42e850, 0x40492b, 0x46e630, 0x401118, 0x46e470 };
        for (int i = 0; i < calls.Length; i += 2)
        {
            int p = image.Offset(calls[i] - 0x400000);
            if (image.Data[p] != 0xe9 || calls[i] + 5 + image.I(p + 1) != calls[i + 1]) throw new InvalidDataException("绘制入口不匹配。");
        }
        if (sectionHeaders + (count + 1) * 40 > image.I(opt + 60)) throw new InvalidDataException("PE 头没有空间。");
        var section = new Section { Rva = Align(end, sectionAlignment) };
        int raw = Align(image.Data.Length, fileAlignment);
        var imports = new List<byte>();
        for (int p = image.Offset(image.I(opt + 104)); ; p += 20)
        {
            bool nonzero = false; for (int i = 0; i < 20; ++i) nonzero |= image.Data[p + i] != 0;
            if (!nonzero) break;
            for (int i = 0; i < 20; ++i) imports.Add(image.Data[p + i]);
        }
        int dllName = section.Add(Encoding.ASCII.GetBytes("RoadRashHD.dll\0"), 1);
        int symbol = section.Add(Encoding.ASCII.GetBytes("\0\0RRHD_Init\0"), 2);
        int lookup = section.Add(Integers(symbol, 0), 4), iat = section.Add(Integers(symbol, 0), 4);
        int importsLength = imports.Count + 40;
        imports.AddRange(Integers(lookup, 0, 0, dllName, iat)); imports.AddRange(new byte[20]);
        int descriptors = section.Add(imports.ToArray(), 4);
        int originalEntry = image.I(opt + 16), entry = Align(section.Rva + section.Bytes.Count, 16);
        var code = new List<byte>(new byte[] { 0x9c, 0x60, 0xe8, 0, 0, 0, 0, 0x58, 0x2d });
        code.AddRange(Integers(entry + 7)); code.AddRange(new byte[] { 0x50, 0xff, 0x90 }); code.AddRange(Integers(iat));
        code.AddRange(new byte[] { 0x83, 0xc4, 0x04, 0x61, 0x9d, 0xe9 });
        code.AddRange(Integers(originalEntry - (entry + code.Count + 4)));
        if (section.Add(code.ToArray(), 16) != entry) throw new InvalidDataException("初始化入口错位。");
        int rawSize = Align(section.Bytes.Count, fileAlignment);
        Array.Resize(ref image.Data, raw + rawSize);
        Buffer.BlockCopy(section.Bytes.ToArray(), 0, image.Data, raw, section.Bytes.Count);
        int header = sectionHeaders + count * 40;
        Buffer.BlockCopy(Encoding.ASCII.GetBytes(".rrhd\0\0\0"), 0, image.Data, header, 8);
        image.Put(header + 8, section.Bytes.Count); image.Put(header + 12, section.Rva);
        image.Put(header + 16, rawSize); image.Put(header + 20, raw);
        image.Put(header + 24, 0); image.Put(header + 28, 0); image.Put(header + 32, 0);
        image.Put(header + 36, unchecked((int)0xe0000060)); image.Put16(image.Header + 6, count + 1);
        image.Put(opt + 16, entry); image.Put(opt + 56, Align(section.Rva + section.Bytes.Count, sectionAlignment));
        image.Put(opt + 64, 0); image.Put(opt + 104, descriptors); image.Put(opt + 108, importsLength);
        image.Put(opt + 128, 0); image.Put(opt + 132, 0);
        if (Hash(image.Data) != expectedHash) throw new InvalidDataException("高清程序重建校验失败，未安装。");
        return image.Data;
    }
}
