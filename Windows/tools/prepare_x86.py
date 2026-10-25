"""Developer-only, pinned native dependencies for 32-bit desktop hosts."""
import hashlib, json, os, pathlib, shutil, subprocess, urllib.request, zipfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
CACHE = pathlib.Path(os.environ.get('FUYI_WINDOWS_CACHE', ROOT / '.deps'))
BUILD = pathlib.Path(os.environ.get('FUYI_WINDOWS_BUILD_DIR', ROOT / 'build'))
OUT = BUILD / 'FloatingTranslate-Windows-x64'
LOCK = json.loads((ROOT / 'tools/dependencies.lock.json').read_text())
CACHE.mkdir(parents=True, exist_ok=True)
def fetch(key):
    item = LOCK[key]; path = CACHE / item['file']
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
        temporary = path.with_suffix('.part')
        urllib.request.urlretrieve(item['url'], temporary)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != item['sha256']:
            raise RuntimeError('Dependency checksum mismatch: ' + key)
        temporary.replace(path)
    return path
native = OUT / 'x86'; native.mkdir(parents=True, exist_ok=True)
headers = BUILD / 'include-x86'; headers.mkdir(parents=True, exist_ok=True)
licenses = OUT / 'licenses'; licenses.mkdir(parents=True, exist_ok=True)
# ONNX Runtime 1.20.1 is the pinned x86 build; newer x64 releases dropped x86.
with zipfile.ZipFile(fetch('onnxruntime_x86')) as archive:
    for name in archive.namelist():
        if name.startswith('build/native/include/') and not name.endswith('/'):
            (headers / pathlib.Path(name).name).write_bytes(archive.read(name))
    for name in ['onnxruntime.dll', 'onnxruntime_providers_shared.dll']:
        (native / name).write_bytes(archive.read('runtimes/win-x86/native/' + name))
    for name in ['LICENSE', 'ThirdPartyNotices.txt']:
        (licenses / ('ONNX-X86-' + name)).write_bytes(archive.read(name))
with zipfile.ZipFile(fetch('extensions')) as archive:
    (native / 'ortextensions.dll').write_bytes(archive.read('runtimes/win-x86/native/ortextensions.dll'))
sdk = CACHE / 'rime-x86-sdk'; sdk.mkdir(exist_ok=True)
subprocess.run([str(fetch('sevenzip_extractor')), 'x', '-y', '-o' + str(sdk), str(fetch('rime_x86'))], check=True)
shutil.copy2(sdk / 'dist/lib/rime.dll', native / 'librime.dll')
shutil.copy2(fetch('vc_redist_x86'), OUT / 'vc_redist.x86.exe')
print('Staged x86 Rime, ONNX Runtime, extensions, notices and VC runtime installer.')
