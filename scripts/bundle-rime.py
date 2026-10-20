#!/usr/bin/env python3
"""Bundle the exact dylib dependency closure. End users do not need Homebrew."""
from pathlib import Path
import subprocess, shutil, sys, json
app = Path(sys.argv[1])
prefix = Path(subprocess.check_output(['brew','--prefix'], text=True).strip())
frameworks = app/'Contents/Frameworks'
notices = app/'Contents/Resources/ThirdPartyLicenses'
frameworks.mkdir(parents=True, exist_ok=True); notices.mkdir(parents=True, exist_ok=True)
queue = [prefix/'opt/librime/lib/librime.1.dylib']
seen = {}; packages = set()
while queue:
    source = queue.pop(0)
    if source.name in seen: continue
    target = frameworks/source.name
    shutil.copyfile(source.resolve(), target); target.chmod(0o755)
    seen[source.name] = target
    resolved = source.resolve()
    if 'Cellar' in resolved.parts:
        index = resolved.parts.index('Cellar')
        package_root = Path(*resolved.parts[:index+3])
        packages.add(resolved.parts[index+1])
        for license in package_root.rglob('*'):
            if license.is_file() and any(license.name.upper().startswith(word) for word in ['LICENSE','COPYING','COPYRIGHT','NOTICE']):
                destination = notices/(resolved.parts[index+1]+'-'+license.name)
                if not destination.exists(): shutil.copyfile(license, destination)
    links = subprocess.check_output(['otool','-L',str(source)],text=True).splitlines()[2:]
    for line in links:
        dependency = line.strip().split(' (',1)[0]
        if dependency.startswith(str(prefix)+'/'):
            path = Path(dependency); queue.append(path)
            subprocess.run(['install_name_tool','-change',dependency,'@rpath/'+path.name,str(target)],check=True,capture_output=True)
    subprocess.run(['install_name_tool','-id','@rpath/'+target.name,str(target)],check=True,capture_output=True)
    subprocess.run(['install_name_tool','-add_rpath','@loader_path',str(target)],check=True,capture_output=True)
for target in seen.values():
    subprocess.run(['xattr','-c',str(target)],check=True)
    subprocess.run(['codesign','--force','--sign','-',str(target)],check=True,capture_output=True)
(notices/'DEPENDENCIES.txt').write_text('Bundled local Rime engine and dependencies:\n'+'\n'.join(sorted(packages))+'\n\nThese libraries are used for local input processing, not cloud translation.\n')
print('Bundled '+str(len(seen))+' local engine libraries')
