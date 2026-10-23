#!/usr/bin/env python3
"""Stage an offline Windows bundle. Only the developer's computer downloads files."""
import hashlib,json,os,pathlib,shutil,struct,subprocess,sys,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1];REPO=ROOT.parent
CACHE=pathlib.Path(os.environ.get('FUYI_WINDOWS_CACHE',ROOT/'.deps'))
BUILD=pathlib.Path(os.environ.get('FUYI_WINDOWS_BUILD_DIR',ROOT/'build'))
OUT=BUILD/'FloatingTranslate-Windows-x64';HEADERS=BUILD/'include'
for p in [CACHE,OUT,HEADERS]:p.mkdir(parents=True,exist_ok=True)
LOCK=json.loads((ROOT/'tools/dependencies.lock.json').read_text())
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
 return h.hexdigest()
def fetch(key):
 item=LOCK[key];p=CACHE/item['file']
 if not p.exists() or digest(p)!=item['sha256']:
  tmp=p.with_suffix('.part');print('Developer download:',key,flush=True)
  subprocess.run(['curl','--fail','--location','--retry','5','--retry-all-errors','--output',str(tmp),item['url']],check=True)
  if digest(tmp)!=item['sha256']:raise RuntimeError('Checksum mismatch: '+key)
  tmp.replace(p)
 return p
ort=fetch('onnxruntime');ext=fetch('extensions');rime=fetch('rime');redist=fetch('vc_redist')
licenses=OUT/'licenses';licenses.mkdir(exist_ok=True)
license_source=pathlib.Path(os.environ.get('FUYI_WINDOWS_LICENSES',REPO/'Android/app/src/main/assets/licenses'))
shutil.copytree(license_source,licenses,dirs_exist_ok=True)
with zipfile.ZipFile(ort) as z:
 for name in z.namelist():
  if name.startswith('build/native/include/') and not name.endswith('/'):(HEADERS/pathlib.Path(name).name).write_bytes(z.read(name))
 for name in ['onnxruntime.dll','onnxruntime_providers_shared.dll']:(OUT/name).write_bytes(z.read('runtimes/win-x64/native/'+name))
 (licenses/'ONNX-RUNTIME-LICENSE.txt').write_bytes(z.read('LICENSE'));(licenses/'ONNX-RUNTIME-ThirdPartyNotices.txt').write_bytes(z.read('ThirdPartyNotices.txt'))
with zipfile.ZipFile(ext) as z:
 (OUT/'ortextensions.dll').write_bytes(z.read('runtimes/win-x64/native/ortextensions.dll'))
 (licenses/'ONNX-EXTENSIONS-LICENSE.txt').write_bytes(z.read('LICENSE.txt'))
 for name in z.namelist():
  if 'ThirdPartyNotices' in name:(licenses/'ONNX-EXTENSIONS-ThirdPartyNotices.txt').write_bytes(z.read(name));break
# Official clang Rime is self contained. The MinGW Rime archive has external MSYS2
# DLL dependencies and must not be substituted without bundling/auditing them.
rime_sdk=CACHE/'rime-clang-sdk';rime_sdk.mkdir(exist_ok=True)
subprocess.run(['tar','-xf',str(rime),'-C',str(rime_sdk)],check=True)
shutil.copyfile(rime_sdk/'dist/lib/rime.dll',OUT/'librime.dll')
shutil.copyfile(rime_sdk/'dist/include/rime_api.h',HEADERS/'rime_api.h')
shutil.copyfile(redist,OUT/'vc_redist.x64.exe')
# Shared, pinned model export. No SDK or downloaded app cache is used.
source=pathlib.Path(os.environ.get('FUYI_TRANSLATION_ASSETS',CACHE/'translation'))
if not (source/'manifest.json').exists():
 env=dict(os.environ,FUYI_TRANSLATION_ASSETS=str(source))
 subprocess.run([sys.executable,str(REPO/'Android/tools/prepare_translation.py')],env=env,check=True)
manifest=json.loads((source/'manifest.json').read_text())
for entry in manifest['files']:
 p=source/entry['file']
 if not p.exists() or digest(p)!=entry['sha256']:raise RuntimeError('Invalid source model: '+entry['file'])
models=OUT/'models';models.mkdir(exist_ok=True)
for name in ['tokenizer.onnx','encoder_model_quantized.onnx','decoder_model_quantized.onnx']:shutil.copyfile(source/name,models/name)
vocab=json.loads((source/'vocab.json').read_text());words=['']*65001
for word,index in vocab.items():words[index]=word
with (models/'vocab.bin').open('wb') as f:
 f.write(struct.pack('<I',len(words)))
 for word in words:
  text=word.encode('utf-8');f.write(struct.pack('<I',len(text)));f.write(text)
mapping=json.loads((source/'sp-map.json').read_text());(models/'sp-map.bin').write_bytes(struct.pack('<I',len(mapping))+struct.pack('<'+'i'*len(mapping),*mapping))
entries=[dict(file=p.name,bytes=p.stat().st_size,sha256=digest(p)) for p in sorted(models.iterdir()) if p.is_file() and p.suffix!='.json']
(models/'manifest.json').write_text(json.dumps(dict(repository=manifest['repository'],revision=manifest['revision'],files=entries),indent=2)+'\n')
checks='#pragma once\nnamespace fuyi { struct ModelCheck {const wchar_t* file;const char* sha;};inline const ModelCheck modelChecks[]={\n'+''.join(' {L"'+e['file']+'","'+e['sha256']+'"},\n' for e in entries)+'};}\n';(HEADERS/'ModelChecks.h').write_text(checks)
# Build dictionaries on the development machine. Source clones have no ignored .bin files.
rime_source=pathlib.Path(os.environ.get('FUYI_RIME_DATA',REPO/'Resources/RimeData'))
rime_data=OUT/'RimeData';rime_data.mkdir(exist_ok=True)
for name in ['default.yaml','fuyi.schema.yaml','pinyin_simp.dict.yaml']:shutil.copyfile(rime_source/name,rime_data/name)
deployer=os.environ.get('RIME_DEPLOYER') or shutil.which('rime_deployer')
if not deployer and sys.platform=='win32':
 bin_dir=rime_sdk/'dist/bin';shutil.copyfile(rime_sdk/'dist/lib/rime.dll',bin_dir/'rime.dll');deployer=str(bin_dir/'rime_deployer.exe')
if not deployer:raise RuntimeError('Development only: supply RIME_DEPLOYER 1.17.0 to compile the dictionary')
subprocess.run([deployer,'--build',str(rime_data),str(rime_data),str(rime_data/'build')],check=True)
if not (rime_data/'build/pinyin_simp.table.bin').exists():raise RuntimeError('Dictionary compilation failed')
# Remove deployer metadata/log files; retain only fixed source and compiled dictionaries.
for p in rime_data.iterdir():
 if p.is_file() and p.name not in ['default.yaml','fuyi.schema.yaml','pinyin_simp.dict.yaml']:p.unlink()
notice=(licenses/'TRANSLATION-NOTICE.txt').read_text().replace('Android bundled','Windows bundled').replace('Extensions 0.13.0','Extensions 0.14.0');(licenses/'TRANSLATION-NOTICE.txt').write_text(notice)
dict_license=license_source/'RIME-DICTIONARY-APACHE-2.0.txt'
shutil.copyfile(dict_license if dict_license.exists() else REPO/'Resources/RIME-LICENSE.txt',licenses/'RIME-DICTIONARY-APACHE-2.0.txt')
for key,name in [('rime_license','RIME-BSD-LICENSE.txt'),('mingw_licenses','MINGW-NOTICES.txt')]:shutil.copyfile(fetch(key),licenses/name)
shutil.copyfile(ROOT/'README.md',OUT/'开发与隐私说明.md')
shutil.copyfile(ROOT/'使用说明.txt',OUT/'使用说明.txt')
shutil.copyfile(ROOT/'TESTING.md',OUT/'测试记录与清单.md')
shutil.copytree(ROOT/'assets/licenses',licenses,dirs_exist_ok=True)
for name,arg in [('启用浮译.cmd','--enable'),('停用浮译.cmd','--disable'),('浮译设置.cmd','')]:
 (OUT/name).write_bytes(('@echo off\r\n"%~dp0FuyiSettings.exe" '+arg+'\r\n').encode())
(OUT/'检查离线组件.cmd').write_bytes(b'@echo off\r\n"%~dp0FuyiCoreTest.exe"\r\nif errorlevel 1 goto failed\r\n"%~dp0FuyiCheck.exe"\r\nif errorlevel 1 goto failed\r\necho PASSED\r\ngoto end\r\n:failed\r\necho FAILED\r\n:end\r\npause\r\n')
subprocess.run([sys.executable,str(ROOT/'tools/prepare_reverse_translation.py')],check=True)
print('Staged offline distribution:',OUT,flush=True)
