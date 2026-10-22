#!/usr/bin/env python3
"""Static verification only. This does not replace running the Windows IME."""
import hashlib,json,pathlib,struct,sys,zipfile

def pe(data):
 offset=struct.unpack_from('<I',data,0x3c)[0]
 assert data[offset:offset+4]==b'PE\0\0','Not a PE image'
 machine,sections=struct.unpack_from('<HH',data,offset+4)
 optional_size=struct.unpack_from('<H',data,offset+20)[0];optional=offset+24
 is64=struct.unpack_from('<H',data,optional)[0]==0x20b
 directory=optional+(112 if is64 else 96)
 section_table=optional+optional_size
 ranges=[struct.unpack_from('<IIII',data,section_table+i*40+8) for i in range(sections)]
 def raw(rva):
  for virtual_size,start,size,file in ranges:
   if start<=rva<start+max(size,virtual_size):return file+rva-start
  raise AssertionError('Invalid PE RVA')
 def string(rva):
  start=raw(rva);return data[start:data.index(b'\0',start)].decode('ascii')
 imports=[];import_rva=struct.unpack_from('<I',data,directory+8)[0]
 if import_rva:
  cursor=raw(import_rva)
  while any(data[cursor:cursor+20]):
   imports.append(string(struct.unpack_from('<I',data,cursor+12)[0]));cursor+=20
 exports=[];export_rva=struct.unpack_from('<I',data,directory)[0]
 if export_rva:
  base=raw(export_rva);count=struct.unpack_from('<I',data,base+24)[0];names=raw(struct.unpack_from('<I',data,base+32)[0])
  exports=[string(struct.unpack_from('<I',data,names+i*4)[0]) for i in range(count)]
 return machine,imports,exports

def audit(path):
 with zipfile.ZipFile(path) as z:
  assert z.testzip() is None,'ZIP CRC mismatch'
  names=set(z.namelist());root='FloatingTranslate/'
  required=['FuyiIME.dll','FuyiSettings.exe','FuyiCheck.exe','FuyiCoreTest.exe','librime.dll','onnxruntime.dll','ortextensions.dll','vc_redist.x64.exe','启用浮译.cmd','停用浮译.cmd','使用说明.txt','RimeData/build/fuyi.schema.yaml','RimeData/build/pinyin_simp.table.bin','licenses/CC-BY-4.0.txt','licenses/TRANSLATION-NOTICE.txt']
  for name in required:assert root+name in names,'Missing '+name
  sums=z.read(root+'SHA256SUMS.txt').decode().splitlines()
  for line in sums:
   sha,name=line.split('  ',1);assert hashlib.sha256(z.read(root+name)).hexdigest()==sha,'Corrupt '+name
  assert len(sums)==len(names)-1,'Unlisted files in package'
  models=json.loads(z.read(root+'models/manifest.json'))
  for item in models['files']:
   data=z.read(root+'models/'+item['file']);assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256']
  system={'kernel32.dll','user32.dll','advapi32.dll','ole32.dll','oleaut32.dll','gdi32.dll','shell32.dll','bcrypt.dll','comctl32.dll','dbghelp.dll','setupapi.dll','dxgi.dll','windowscodecs.dll','ntdll.dll','ucrtbase.dll','msvcrt.dll','version.dll','shlwapi.dll','crypt32.dll'}
  included={pathlib.PurePosixPath(name).name.lower() for name in names}
  for name in sorted(names):
   if not name.lower().endswith(('.exe','.dll')) or name.endswith('vc_redist.x64.exe'):continue
   machine,imports,exports=pe(z.read(name));assert machine==0x8664,'Wrong architecture: '+name
   for dependency in imports:
    d=dependency.lower();assert d not in {'winhttp.dll','wininet.dll','ws2_32.dll','urlmon.dll'},'Network dependency: '+name
    assert d in system or d in included or d.startswith(('api-ms-win-','ext-ms-win-','msvcp140','vcruntime140')),'Missing native dependency: '+dependency
   if name.endswith('FuyiIME.dll'):assert set(exports)=={'DllGetClassObject','DllCanUnloadNow','DllRegisterServer','DllUnregisterServer'}
  print('ZIP CRC,',len(sums),'file checksums,',len(models['files']),'models, x64 PE/TSF exports and native dependencies passed.')

if __name__=='__main__':audit(pathlib.Path(sys.argv[1]))
