#!/usr/bin/env python3
import os,pathlib,subprocess,hashlib,zipfile,json,sys
ROOT=pathlib.Path(__file__).resolve().parents[1];BUILD=pathlib.Path(os.environ.get('FUYI_WINDOWS_BUILD_DIR',ROOT/'build'));OUT=BUILD/'FloatingTranslate-Windows-x64'
TOOL=pathlib.Path(os.environ['LLVM_MINGW'])/'bin';CXX=TOOL/'x86_64-w64-mingw32-clang++';RC=TOOL/'x86_64-w64-mingw32-windres'
def run(args,**kwargs):print(' '.join(str(x) for x in args),flush=True);subprocess.run([str(x) for x in args],check=True,**kwargs)
# Embed hashes for both complete model bundles in every native build.
checks=[]
for folder in [OUT/'models',OUT/'models/en-zh']:
 manifest=json.loads((folder/'manifest.json').read_text())
 for entry in manifest['files']:
  p=folder/entry['file']
  if p.stat().st_size!=entry['bytes'] or hashlib.sha256(p.read_bytes()).hexdigest()!=entry['sha256']:
   raise RuntimeError('Invalid bundled model: '+str(p))
  checks.append((p.relative_to(OUT/'models').as_posix(),entry['sha256']))
(BUILD/'include/ModelChecks.h').write_text('#pragma once\nnamespace fuyi { struct ModelCheck {const wchar_t* file;const char* sha;};inline const ModelCheck modelChecks[]={\n'+''.join(' {L"'+name+'","'+sha+'"},\n' for name,sha in checks)+'};}\n')
if hashlib.sha256((OUT/'Proofreading/en_50k.txt').read_bytes()).hexdigest()!='5351ff405b1126ef555791dd4d9798a48e3e9a501a9fc481a9da957752cfb458':raise RuntimeError('Invalid proofreading dictionary')
run([RC,'-i','app.rc','-o',BUILD/'app.res'],cwd=ROOT/'assets')
common=[CXX,'-std=c++17','-ffile-prefix-map='+str(ROOT)+'=Windows','-O2','-g0','-static','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-I',BUILD/'include','-I',ROOT/'src','-Wl,--no-insert-timestamp','-Wl,--dynamicbase','-Wl,--nxcompat']
if os.environ.get('FUYI_DISPLAY_DIAGNOSTICS')=='1':common+=['-DFUYI_DISPLAY_DIAGNOSTICS']
libs=['-lole32','-loleaut32','-luuid','-luser32','-lgdi32','-lshell32','-ladvapi32','-lbcrypt','-lcomctl32','-lcrypt32','-lgdiplus']
run(common+[ROOT/'src/TextService.cpp',ROOT/'src/Registration.cpp',ROOT/'src/InputScopeGuids.cpp',ROOT/'assets/FuyiIME.def',BUILD/'app.res','-shared','-o',OUT/'FuyiIME.dll']+libs)
run(common+[ROOT/'src/Settings.cpp',BUILD/'app.res','-municode','-mwindows','-o',OUT/'FuyiSettings.exe']+libs)
run(common+[ROOT/'src/Check.cpp',BUILD/'app.res','-municode','-o',OUT/'FuyiCheck.exe']+libs)
run(common+[ROOT/'tests/core_test.cpp','-o',OUT/'FuyiCoreTest.exe'])
if '--test' in sys.argv:
 if sys.platform!='win32':raise RuntimeError('--test requires a Windows host')
 run(common+[ROOT/'tests/runtime_regression.cpp','-municode','-o',BUILD/'runtime_regression.exe']+libs)
 run(common+[ROOT/'tests/maintenance_test.cpp','-o',BUILD/'maintenance_test.exe']+libs)
 run(common+[ROOT/'tests/candidate_ui_test.cpp','-o',BUILD/'candidate_ui_test.exe']+libs)
 run(common+[ROOT/'tests/panel_preview.cpp','-municode','-o',BUILD/'panel_preview.exe']+libs)
 for dpi in [96,120,144,192]:
  run([BUILD/'panel_preview.exe',BUILD/('panel-'+str(dpi)+'.png'),str(dpi)])
  run([BUILD/'panel_preview.exe',BUILD/('panel-glass-'+str(dpi)+'.png'),str(dpi),'glass'])
 run([BUILD/'candidate_ui_test.exe'])
 run([BUILD/'maintenance_test.exe'])
 run([OUT/'FuyiCoreTest.exe'])
 run([OUT/'FuyiCheck.exe'])
 run([BUILD/'runtime_regression.exe',OUT/'FuyiIME.dll'])
# Keep user-facing docs in step with the build, including pre-staged bundles.
import shutil
for source,name in [('README.md','开发与隐私说明.md'),('TESTING.md','测试记录与清单.md'),('使用说明.txt','使用说明.txt')]:
 shutil.copy2(ROOT/source,OUT/name)
# Checksums of every delivered file; no last-minute downloads on the user's PC.
files=sorted(p for p in OUT.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt')
(OUT/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(OUT).as_posix()+'\n' for p in files),encoding='utf-8')
zip_path=BUILD/'FloatingTranslate-Windows-0.1.18-x64-offline.zip'
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(OUT.rglob('*')):
  if p.is_file():z.write(p,'FloatingTranslate/'+p.relative_to(OUT).as_posix())
print('Packaged:',zip_path,zip_path.stat().st_size,'bytes')
