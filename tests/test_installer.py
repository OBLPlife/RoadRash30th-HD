# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Test file installation and the loaded DLL without starting the game."""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
NATIVE = {
    '9e40e25737149ab095061b7154797a35495faea97644fafc5a1e616f1400c968': '8a91e80e70b4ec1e764676712e9e78ec7b718562b853232ab60e3e24bf842a1a',
    '5590703f9bec56ed66c63f27d61a910d751cdf5cdaa0ce32e08a669de9c8216f': 'dcefc1f0ca18f62a2fb752b5d98806989229ab2db891607e0a0ec5fba292b97d',
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(original_dir, localized_dir):
    build = (ROOT / 'build').resolve()
    folder = Path(tempfile.mkdtemp(prefix='installer-test-', dir=build)).resolve()
    if folder.parent != build:
        raise RuntimeError('测试目录必须位于 build 内')
    unrelated_cwd = folder / '不相关的工作目录'; unrelated_cwd.mkdir()
    installer = ROOT / 'dist/RoadRash_HD_Setup.exe'
    renderer = (build / 'RoadRashHD.dll').read_bytes()
    license_text = (ROOT / 'LICENSE').read_bytes()
    assert license_text in installer.read_bytes(), 'license not embedded in installer'
    checks = []

    def fixture(name, source):
        game = folder / name; (game / 'TEXT').mkdir(parents=True)
        shutil.copyfile(installer, game / '高清分辨率补丁.exe')
        if source is not None:
            shutil.copyfile(source / 'RoadRash95.exe', game / 'RoadRash95.exe')
            shutil.copyfile(source / 'TEXT/ENU.DLL', game / 'TEXT/ENU.DLL')
        (game / '1.RRS').write_bytes(b'save fixture - never modify')
        (game / 'original-art.rri').write_bytes(b'art fixture - never modify')
        return game

    def execute(game, *args, success=True):
        language = (game / 'TEXT/ENU.DLL').read_bytes() if (game / 'TEXT/ENU.DLL').exists() else None
        result = subprocess.run([str(game / '高清分辨率补丁.exe'), '/quiet', *args], cwd=unrelated_cwd, timeout=30)
        log = (game / 'RoadRash_HD_Setup.log').read_text(encoding='utf-8-sig')
        assert (result.returncode == 0) == success, (result.returncode, log)
        assert 'PolyForm Noncommercial License 1.0.0' in log
        if language is not None:
            assert (game / 'TEXT/ENU.DLL').read_bytes() == language, 'language DLL modified'
        assert (game / '1.RRS').read_bytes() == b'save fixture - never modify'
        assert (game / 'original-art.rri').read_bytes() == b'art fixture - never modify'
        assert not list(game.rglob('.rrhd-*.tmp'))
        return log

    try:
        compiler = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
        window_test = build / 'window_test.exe'
        subprocess.run([str(compiler), '/nologo', '/utf8output', '/out:' + str(window_test),
                        '/win32manifest:' + str(ROOT / 'app.manifest'),
                        '/reference:System.Windows.Forms.dll', '/reference:System.Drawing.dll',
                        str(ROOT / 'tests/test_window.cs')], check=True)
        missing = fixture('缺少游戏', None)
        assert '根目录' in execute(missing, success=False)
        checks.append('missing root rejected')
        english_native = None
        for label, source in [('english', original_dir), ('chinese', localized_dir)]:
            if source is None:
                continue
            game = fixture(label + ' 中文路径 与 空格', source)
            before = (game / 'RoadRash95.exe').read_bytes()
            assert digest(before) in NATIVE, 'unsupported fixture input'
            subprocess.run([str(window_test), str(game / '高清分辨率补丁.exe'), str(build / ('window-' + label + '.png'))],
                           cwd=unrelated_cwd, check=True, timeout=30)
            checks.append(label + ': GUI opens without writes and contains only Install and Restore buttons')
            assert '安装完成' in execute(game)
            actual = (game / 'RoadRash95.exe').read_bytes()
            assert digest(actual) == NATIVE[digest(before)], 'native PE differs from verified reference'
            assert (game / 'RoadRashHD.dll').read_bytes() == renderer
            assert (game / '分辨率补丁_安装前备份/RoadRash95.exe').read_bytes() == before
            subprocess.run([str(build / 'hd_loader_test.exe'), str(game / 'RoadRash95.exe')], check=True, timeout=30)
            subprocess.run([str(build / 'hd_animation_test.exe'), str(game / 'RoadRash95.exe'),
                            str(game / 'RoadRashHD.dll')], check=True, timeout=30)
            if label == 'english':
                english_native = actual
            custom = b'[Graphics]\r\nEnabled=1\r\nRenderHeight=900\r\n; preserve my settings\r\n'
            (game / 'RoadRashHD.ini').write_bytes(custom)
            assert '已经安装' in execute(game)
            assert (game / 'RoadRashHD.ini').read_bytes() == custom
            assert '已恢复' in execute(game, '/restore')
            assert (game / 'RoadRash95.exe').read_bytes() == before
            assert not (game / 'RoadRashHD.dll').exists()
            assert (game / 'RoadRashHD.ini').read_bytes() == custom
            assert '已经恢复' in execute(game, '/restore')
            assert '安装完成' in execute(game)
            assert (game / 'RoadRashHD.ini').read_bytes() == custom
            checks.append(label + ': native PE + real DLL load, no translation/save changes, repeat, restore and preserved config')

        unknown = fixture('未知版本', original_dir)
        bad = bytearray((unknown / 'RoadRash95.exe').read_bytes()); bad[256] ^= 1
        (unknown / 'RoadRash95.exe').write_bytes(bad)
        assert '版本不匹配' in execute(unknown, success=False)
        assert (unknown / 'RoadRash95.exe').read_bytes() == bad
        assert not (unknown / 'RoadRashHD.dll').exists()
        assert not (unknown / '分辨率补丁_安装前备份').exists()
        checks.append('unsupported executable rejected without mutation')

        prior = fixture('已有高清版本', original_dir)
        (prior / 'RoadRash95.exe').write_bytes(english_native)
        previous_dll = bytearray(renderer)
        pe_header = int.from_bytes(previous_dll[60:64], 'little')
        previous_dll[pe_header + 8] ^= 1  # Different PE timestamp, same valid DLL.
        (prior / 'RoadRashHD.dll').write_bytes(previous_dll)
        config = b'[Graphics]\r\nEnabled=1\r\nRenderHeight=1080\r\n'
        (prior / 'RoadRashHD.ini').write_bytes(config)
        execute(prior)
        assert (prior / 'RoadRashHD.dll').read_bytes() == renderer
        assert (prior / 'RoadRashHD.ini').read_bytes() == config
        execute(prior, '/restore')
        assert (prior / 'RoadRash95.exe').read_bytes() == english_native
        assert (prior / 'RoadRashHD.dll').read_bytes() == previous_dll
        assert (prior / 'RoadRashHD.ini').read_bytes() == config
        checks.append('existing HD DLL and custom configuration restored exactly')

        conflict = fixture('已有不同备份', original_dir)
        old_backup = conflict / '分辨率补丁_安装前备份/RoadRash95.exe'
        old_backup.parent.mkdir(); old_backup.write_bytes(b'keep existing backup')
        assert '备份' in execute(conflict, success=False)
        assert old_backup.read_bytes() == b'keep existing backup'
        assert not (conflict / 'RoadRashHD.dll').exists()
        checks.append('conflicting backup retained')

        locked = fixture('程序被其他进程占用', original_dir)
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                      wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.CreateFileW(str(locked / 'RoadRash95.exe'), 0x80000000, 1, None, 3, 0, None)
        assert handle != ctypes.c_void_p(-1).value, ctypes.get_last_error()
        try:
            assert '操作未完成' in execute(locked, success=False)
        finally:
            kernel.CloseHandle(handle)
        assert (locked / 'RoadRash95.exe').read_bytes() == (original_dir / 'RoadRash95.exe').read_bytes()
        assert not (locked / 'RoadRashHD.dll').exists(), 'newly committed dependency not rolled back'
        assert not (locked / 'RoadRashHD.ini').exists()
        checks.append('locked EXE replacement rolls back newly installed DLL')

        report = {'passed': checks, 'license': 'PolyForm-Noncommercial-1.0.0',
                  'game_started': False, 'real_install_modified': False, 'installer_sha256': digest(installer.read_bytes())}
        (build / 'test-results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(report))
    finally:
        if folder.resolve().parent != build:
            raise RuntimeError('拒绝清理 build 外的目录')
        shutil.rmtree(folder)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original-dir', type=Path, required=True)
    parser.add_argument('--localized-dir', type=Path)
    args = parser.parse_args()
    run(args.original_dir.resolve(), args.localized_dir.resolve() if args.localized_dir else None)
