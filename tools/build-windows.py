#!/usr/bin/env python3
"""Cross-build Windows x64 using an existing LLVM and Microsoft SDK installation."""
import argparse
import pathlib
import subprocess
root = pathlib.Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk', type=pathlib.Path, required=True, help='Root containing crt/ and sdk/')
p.add_argument('--juce', type=pathlib.Path, required=True, help='JUCE 8.0.10 source checkout')
p.add_argument('--build', type=pathlib.Path, default=root / 'build-win')
p.add_argument('--llvm', type=pathlib.Path, default=pathlib.Path('/opt/homebrew/opt/llvm'))
p.add_argument('--jobs', type=int, default=6)
a = p.parse_args()
if not (a.sdk / 'crt/include/vector').is_file() or not (a.sdk / 'sdk/include/um/Windows.h').is_file():
    p.error('SDK must contain the Microsoft CRT and Windows SDK headers/libraries')
subprocess.run(['cmake', '-S', str(root), '-B', str(a.build), '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_TOOLCHAIN_FILE=' + str(root / 'tools/windows-clang-msvc.cmake'),
    '-DOPTO_WINSDK_ROOT=' + str(a.sdk.resolve()), '-DOPTO_LLVM_ROOT=' + str(a.llvm.resolve()),
    '-DJUCE_SOURCE_DIR=' + str(a.juce.resolve())], check=True)
subprocess.run(['cmake', '--build', str(a.build), '--target', 'Luma2A_VST3', '--parallel', str(a.jobs)], check=True)
