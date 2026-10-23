#!/usr/bin/env python3
"""Development only: pin/verify the second offline model; users download nothing."""
import base64, hashlib, json, os, pathlib, struct, subprocess
import onnx
from onnx import helper, TensorProto
import sentencepiece as spm
ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = pathlib.Path(os.environ.get('FUYI_WINDOWS_BUILD_DIR', ROOT / 'build'))
CACHE = pathlib.Path(os.environ.get('FUYI_REVERSE_CACHE', BUILD / 'cache' / 'en-zh'))
OUT = BUILD / 'FloatingTranslate-Windows-x64' / 'models' / 'en-zh'
REV = '046f55aec303cdee3e0318604406d4df20f1e8ea'
FILES = {
 'source.spm': '5775ddc9e3ff2fae91554da56468ad35ff56edaba870fea74447bc7234bfdaa8',
 'vocab.json': '22c957348eed495ee925afc40a36da3e387c8a34a734c8486967c2dca271613e',
 'config.json': '4727d1229a04f95bf6f39abf949d8080615433d99d6ebd85f81c09edd247d5fa',
 'onnx/encoder_model_quantized.onnx': 'd3b7912bf6a9bd27e4c074c2df91d4ff3d5b4bc5f7f6c8d7cc9c805c98fbafee',
 'onnx/decoder_model_quantized.onnx': '2c66a3981099b40edbb3a0d65e015d05964d42fecb9780f8422776eff5939112',
}
CACHE.mkdir(parents=True, exist_ok=True); OUT.mkdir(parents=True, exist_ok=True)
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for name, sha in FILES.items():
 p = CACHE / pathlib.Path(name).name
 if not p.exists() or digest(p) != sha:
  temporary = p.with_suffix('.part')
  subprocess.run(['curl', '--http1.1', '--fail', '-L', '--retry', '3', '--retry-all-errors',
                  '--connect-timeout', '20', '--max-time', '900', '-o', str(temporary),
                  f'https://huggingface.co/Xenova/opus-mt-en-zh/resolve/{REV}/{name}?download=true'], check=True)
  if digest(temporary) != sha: raise RuntimeError('Reverse model checksum mismatch: ' + name)
  temporary.replace(p)
config = json.loads((CACHE / 'config.json').read_text())
assert config['vocab_size'] == 65001 and config['decoder_start_token_id'] == 65000 and config['eos_token_id'] == 0
sp = spm.SentencePieceProcessor(model_file=str(CACHE / 'source.spm'))
vocab = json.loads((CACHE / 'vocab.json').read_text()); words = [''] * 65001
for word, index in vocab.items(): words[index] = word
with (OUT / 'vocab.bin').open('wb') as f:
 f.write(struct.pack('<I', len(words)))
 for word in words:
  token = word.encode('utf-8'); f.write(struct.pack('<I', len(token))); f.write(token)
mapping = [vocab.get(sp.id_to_piece(i), vocab['<unk>']) for i in range(sp.get_piece_size())]
(OUT / 'sp-map.bin').write_bytes(struct.pack('<I', len(mapping)) + struct.pack('<' + 'i' * len(mapping), *mapping))
nodes = [helper.make_node('SentencepieceTokenizer', ['text','nbest','alpha','bos','eos','reverse'],
 ['tokens','indices'], domain='ai.onnx.contrib', model=base64.b64encode((CACHE / 'source.spm').read_bytes())),
 helper.make_node('Cast', ['tokens'], ['ids'], to=TensorProto.INT64)]
inits = [helper.make_tensor('nbest', TensorProto.INT64, [1], [0]), helper.make_tensor('alpha', TensorProto.FLOAT, [1], [0.0])] + [helper.make_tensor(x, TensorProto.BOOL, [1], [False]) for x in ['bos','eos','reverse']]
graph = helper.make_graph(nodes, 'FuyiSentencePiece', [helper.make_tensor_value_info('text',TensorProto.STRING,[1])], [helper.make_tensor_value_info('ids',TensorProto.INT64,[None])], inits)
model = helper.make_model(graph, opset_imports=[helper.make_opsetid('',17), helper.make_opsetid('ai.onnx.contrib',1)]); model.ir_version = 9
onnx.checker.check_model(model); onnx.save(model, OUT / 'tokenizer.onnx')
import shutil
for name in ['encoder_model_quantized.onnx','decoder_model_quantized.onnx']: shutil.copyfile(CACHE / name, OUT / name)
entries = [dict(file=p.name,bytes=p.stat().st_size,sha256=digest(p)) for p in sorted(OUT.iterdir()) if p.is_file() and p.suffix != '.json']
(OUT / 'manifest.json').write_text(json.dumps(dict(repository='Xenova/opus-mt-en-zh',revision=REV,files=entries), indent=2) + '\n')
print('Verified reverse model:', sum(e['bytes'] for e in entries), 'bytes')
