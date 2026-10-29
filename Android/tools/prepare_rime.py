#!/usr/bin/env python3
"""Fetch exact official upstream binaries; never bypass TLS or checksum checks."""
import hashlib,json,pathlib,subprocess,tempfile,zipfile
root=pathlib.Path(__file__).resolve().parents[1]
manifest=json.loads((root/'tools/rime-release-sha256.json').read_text())
with tempfile.TemporaryDirectory() as temp:
 for abi,digest in manifest.items():
  archive=pathlib.Path(temp)/(abi+'.apk')
  url='https://github.com/osfans/trime/releases/download/v3.3.12/com.osfans.trime-v3.3.12-0-ge09ac711-'+abi+'-release.apk'
  subprocess.run(['curl','-fL','--retry','3',url,'-o',str(archive)],check=True)
  if hashlib.sha256(archive.read_bytes()).hexdigest()!=digest:raise RuntimeError('Upstream checksum mismatch: '+abi)
  target=root/'app/src/main/jniLibs'/abi;target.mkdir(parents=True,exist_ok=True)
  with zipfile.ZipFile(archive) as z:(target/'librime_jni.so').write_bytes(z.read('lib/'+abi+'/librime_jni.so'))
 print('Verified all four Rime binaries')
