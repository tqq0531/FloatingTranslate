#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
python3 - <<'PY'
import tempfile, pathlib, subprocess, shutil, hashlib, plistlib
root=pathlib.Path.cwd()
with tempfile.TemporaryDirectory(prefix='fuyi-package-') as temp:
    package=pathlib.Path(temp)/'浮译-Mac本地试用版'
    package.mkdir()
    subprocess.run(['ditto','--noextattr','--norsrc',str(root/'build/浮译.app'),str(package/'浮译.app')],check=True)
    for name in ['安装浮译.command','README.md']:
        shutil.copyfile(root/name, package/name)
    shutil.copytree(root/'docs',package/'docs')
    shutil.copyfile(root/'Resources/RIME-LICENSE.txt', package/'第三方词库许可.txt')
    (package/'安装浮译.command').chmod(0o755)
    subprocess.run(['xattr','-cr',str(package)],check=True)
    subprocess.run(['codesign','--verify','--deep','--strict',str(package/'浮译.app')],check=True)
    version=plistlib.loads((root/'Info.plist').read_bytes())['CFBundleShortVersionString']
    archive=root/('build/浮译-Mac本地试用版-'+version+'.zip')
    subprocess.run(['ditto','-c','-k','--norsrc','--noextattr','--keepParent',str(package),str(archive)],check=True)
    digest=hashlib.sha256(archive.read_bytes()).hexdigest()
    (root/'build/SHA256SUMS.txt').write_text(digest+'  '+archive.name+'\n')
    print('Packaged local preview: '+archive.name)
    print('SHA256: '+digest)
PY
