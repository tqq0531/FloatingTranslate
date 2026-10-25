#!/usr/bin/env python3
"""Developer-only C++/WinRT header generation; no downloads in the IME."""
import hashlib,json,os,pathlib,shutil,subprocess,urllib.request,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
CACHE=pathlib.Path(os.environ.get('FUYI_WINDOWS_CACHE',ROOT/'.deps'))
CACHE.mkdir(parents=True,exist_ok=True)
item=json.loads((ROOT/'tools/dependencies.lock.json').read_text())['cppwinrt']
package=CACHE/item['file']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if not package.exists() or sha(package)!=item['sha256']:
 data=urllib.request.urlopen(item['url'],timeout=60).read()
 if hashlib.sha256(data).hexdigest()!=item['sha256']:raise RuntimeError('C++/WinRT checksum mismatch')
 package.write_bytes(data)
tool=CACHE/'cppwinrt'
with zipfile.ZipFile(package) as archive:
 for name in ['bin/cppwinrt.exe','LICENSE']:archive.extract(name,tool)
metadata=pathlib.Path(os.environ.get('FUYI_WINDOWS_METADATA',pathlib.Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/WinMetadata'))
if not metadata.is_dir():raise RuntimeError('Set FUYI_WINDOWS_METADATA to Windows SDK WinMetadata')
headers=pathlib.Path(os.environ.get('CPPWINRT_INCLUDE',CACHE/'cppwinrt-include'))
subprocess.run([str(tool/'bin/cppwinrt.exe'),'-input',str(metadata),'-output',str(headers),'-base',
 '-filter','Windows.UI.Composition','-filter','Windows.System.DispatcherQueue','-filter','Windows.Foundation'],check=True)
build=pathlib.Path(os.environ.get('FUYI_WINDOWS_BUILD_DIR',ROOT/'build'))
license_dir=build/'FloatingTranslate-Windows-x64/licenses'
license_dir.mkdir(parents=True,exist_ok=True)
shutil.copy2(tool/'LICENSE',license_dir/'CPPWINRT-MIT-LICENSE.txt')
print('C++/WinRT projection prepared:',headers)
