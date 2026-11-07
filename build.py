# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Build the complete HD installer without distributing or requiring game files."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parent


def compiler_environment(explicit):
    if explicit:
        path = explicit.resolve()
    else:
        vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        installation = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                                               'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                                               '-property', 'installationPath'], encoding='utf-8').strip()
        path = Path(installation) / 'VC/Auxiliary/Build/vcvarsall.bat'
    if not path.is_file():
        raise FileNotFoundError('需要 Visual Studio C++ 桌面开发工具和 Windows SDK，可用 --vcvarsall 指定 vcvarsall.bat')
    return path


def build(vcvarsall, tests):
    work = ROOT / 'build'
    work.mkdir(exist_ok=True)
    (ROOT / 'dist').mkdir(exist_ok=True)
    commands = ['@echo off', 'chcp 65001 >nul', f'call "{compiler_environment(vcvarsall)}" x86 >nul', 'if errorlevel 1 exit /b 1',
                'cd /d "%~dp0.."',
                'rc /nologo /fo build\\hd_menu.res src\\hd_menu.rc', 'if errorlevel 1 exit /b 1',
                'cl /nologo /std:c++17 /O2 /MT /EHsc /W4 /utf-8 /LD src\\hd_renderer.cpp '
                '/Fo:build\\hd_renderer.obj /Fe:build\\RoadRashHD.dll /link /Brepro '
                '/DEF:src\\hd_renderer.def build\\hd_menu.res user32.lib gdi32.lib', 'if errorlevel 1 exit /b 1']
    if tests:
        for name in ['hd_test', 'hd_loader_test', 'hd_perf_test']:
            commands.extend([f'cl /nologo /std:c++17 /O2 /MT /EHsc /W4 /utf-8 tests\\{name}.cpp '
                             f'/Fo:build\\{name}.obj /Fe:build\\{name}.exe /link user32.lib gdi32.lib',
                             'if errorlevel 1 exit /b 1'])
    commands.append('exit /b 0')
    batch = work / 'compile.cmd'
    batch.write_text('\n'.join(commands) + '\n', encoding='utf-8', newline='\r\n')
    subprocess.run(['cmd.exe', '/d', '/c', str(batch)], check=True)
    csc = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
    exe = ROOT / 'dist/RoadRash_HD_Setup.exe'
    subprocess.run([str(csc), '/nologo', '/target:winexe', '/platform:anycpu', '/optimize+', '/utf8output',
                    '/out:' + str(exe), '/win32manifest:' + str(ROOT / 'app.manifest'),
                    '/resource:' + str(work / 'RoadRashHD.dll') + ',RoadRashHD.dll',
                    '/resource:' + str(ROOT / 'RoadRashHD.ini') + ',RoadRashHD.ini',
                    '/resource:' + str(ROOT / 'LICENSE') + ',LICENSE', '/reference:System.Windows.Forms.dll',
                    str(ROOT / 'Installer.cs'), str(ROOT / 'RendererPatch.cs')], check=True)
    manifest = {'renderer_version': 4, 'license': 'PolyForm-Noncommercial-1.0.0',
                'game_files_embedded': False, 'localization_embedded': False,
                'installer_bytes': exe.stat().st_size,
                'installer_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
                'renderer_sha256': hashlib.sha256((work / 'RoadRashHD.dll').read_bytes()).hexdigest()}
    (work / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vcvarsall', type=Path, help='optional explicit path to vcvarsall.bat')
    parser.add_argument('--tests', action='store_true', help='also compile offscreen renderer tests')
    args = parser.parse_args()
    build(args.vcvarsall, args.tests)
