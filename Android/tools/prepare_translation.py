#!/usr/bin/env python3
"""Build-time only. Fetch pinned public weights, verify SHA-256, export local tokenizer."""
import base64, hashlib, json, pathlib, urllib.request, os
import onnx
from onnx import helper, TensorProto
import sentencepiece as spm
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=pathlib.Path(os.environ.get('FUYI_TRANSLATION_ASSETS',ROOT/'app/src/main/assets/translation'))
REV='39d480d52a9ea3065a1f117adfe4dbc55de10e6f'
FILES={
 'source.spm':'e27a3a1b539f4959ec72ea60e453f49156289f95d4e6000b29332efc45616203',
 'vocab.json':'08a119a1defd522fa047cb5e3bfe3e89633e96caa38ced0dc9cee7ef1021a011',
 'onnx/encoder_model_quantized.onnx':'84d5e171b626bc8b6b220d022ac58696e9528c25deeacca62b5cbf4364547a99',
 'onnx/decoder_model_quantized.onnx':'debcc3054b9ff9aaed972bd6e251a459abe1e2f1402da1efa63163dc2f46ef73',
}
OUT.mkdir(parents=True,exist_ok=True)
for name,sha in FILES.items():
    path=OUT/pathlib.Path(name).name
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest()!=sha:
        temp=path.with_suffix('.part')
        print('Fetching',name,flush=True)
        urllib.request.urlretrieve(f'https://huggingface.co/Xenova/opus-mt-zh-en/resolve/{REV}/{name}?download=true',temp)
        if hashlib.sha256(temp.read_bytes()).hexdigest()!=sha:raise RuntimeError('Checksum mismatch: '+name)
        temp.replace(path)
sp=spm.SentencePieceProcessor(model_file=str(OUT/'source.spm'))
vocab=json.loads((OUT/'vocab.json').read_text())
(OUT/'sp-map.json').write_text(json.dumps([vocab.get(sp.id_to_piece(i),vocab['<unk>']) for i in range(sp.get_piece_size())],separators=(',',':')))
# v0.13 SentencepieceTokenizer requires scalar controls as graph inputs/initializers.
nodes=[helper.make_node('SentencepieceTokenizer',['text','nbest','alpha','bos','eos','reverse'],['tokens','indices'],domain='ai.onnx.contrib',model=base64.b64encode((OUT/'source.spm').read_bytes())),helper.make_node('Cast',['tokens'],['ids'],to=TensorProto.INT64)]
inits=[helper.make_tensor('nbest',TensorProto.INT64,[1],[0]),helper.make_tensor('alpha',TensorProto.FLOAT,[1],[0.0])]+[helper.make_tensor(x,TensorProto.BOOL,[1],[False]) for x in ['bos','eos','reverse']]
graph=helper.make_graph(nodes,'FuyiSentencePiece',[helper.make_tensor_value_info('text',TensorProto.STRING,[1])],[helper.make_tensor_value_info('ids',TensorProto.INT64,[None])],inits)
model=helper.make_model(graph,opset_imports=[helper.make_opsetid('',17),helper.make_opsetid('ai.onnx.contrib',1)]);model.ir_version=9
onnx.checker.check_model(model);onnx.save(model,OUT/'tokenizer.onnx')
# source.spm is encoded in tokenizer.onnx and need not be duplicated in APK.
entries=[]
for name in ['tokenizer.onnx','sp-map.json','vocab.json','encoder_model_quantized.onnx','decoder_model_quantized.onnx']:
    p=OUT/name;entries.append(dict(file=name,bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
(OUT/'manifest.json').write_text(json.dumps(dict(repository='Xenova/opus-mt-zh-en',revision=REV,files=entries),indent=2)+'\n')
print('Verified bundled translation files:',sum(x['bytes'] for x in entries),'bytes')
