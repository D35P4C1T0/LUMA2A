#!/usr/bin/env python3
"""Package existing validated builds; never install them into system directories."""
import hashlib,json,pathlib,subprocess,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
DIST=ROOT/'dist';DIST.mkdir(exist_ok=True)
artifacts=[
    ('Luma2A-macOS-universal-au.zip',ROOT/'build-macos-universal/Luma2A_artefacts/Release/AU/Luma2A.component'),
    ('Luma2A-macOS-universal-vst3.zip',ROOT/'build-macos-universal/Luma2A_artefacts/Release/VST3/Luma2A.vst3'),
    ('Luma2A-macOS-universal-standalone.zip',ROOT/'build-macos-universal/Luma2A_artefacts/Release/Standalone/Luma2A.app'),
    ('Luma2A-Windows-x64-vst3.zip',ROOT/'build-win/Luma2A_artefacts/Release/VST3/Luma2A.vst3'),
]
manifest={'version':'0.1.2','archives':[],'source_sha256':{}}
for name,source in artifacts:
    if not source.exists():raise SystemExit(f'Missing build: {source}')
    target=DIST/name
    if target.exists():target.unlink()
    subprocess.run(['ditto','-c','-k','--sequesterRsrc','--keepParent',str(source),str(target)],check=True)
    manifest['archives'].append({'file':name,'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
files=[ROOT/'CMakeLists.txt',ROOT/'README.md',ROOT/'.gitignore']
for directory in ('Source','docs'):
    files.extend(p for p in (ROOT/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix.lower() not in ('.exe','.obj','.d','.o','.pyc','.manifest'))
files.extend(ROOT/'tools'/name for name in ('UiPreview.cpp','model_checks.cpp','model_render.cpp','package.py','build-windows.py','windows-clang-msvc.cmake'))
files.append(ROOT/'.clang-format')
with zipfile.ZipFile(DIST/'Luma2A-source.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(set(files)):
        archive.write(path,'Luma2A/'+str(path.relative_to(ROOT)))
        manifest['source_sha256'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
sourceArchive=DIST/'Luma2A-source.zip'
manifest['archives'].append({'file':sourceArchive.name,'bytes':sourceArchive.stat().st_size,'sha256':hashlib.sha256(sourceArchive.read_bytes()).hexdigest()})
(DIST/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for artifact in manifest['archives']:print(artifact['file'],artifact['bytes'],artifact['sha256'])
