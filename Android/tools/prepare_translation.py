#!/usr/bin/env python3
"""Build-time only: verify both bundled directions; the APK never downloads models.

FUYI_REVERSE_CACHE may point at an existing verified en-zh source/model directory.
It is a development convenience; every reused upstream file is checksum checked.
"""
import base64, hashlib, json, os, pathlib, shutil, urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = pathlib.Path(os.environ.get('FUYI_TRANSLATION_ASSETS', ROOT / 'app/src/main/assets/translation'))
MODELS = [
    ('zh-en', OUT, '39d480d52a9ea3065a1f117adfe4dbc55de10e6f', {
        'source.spm': 'e27a3a1b539f4959ec72ea60e453f49156289f95d4e6000b29332efc45616203',
        'vocab.json': '08a119a1defd522fa047cb5e3bfe3e89633e96caa38ced0dc9cee7ef1021a011',
        'onnx/encoder_model_quantized.onnx': '84d5e171b626bc8b6b220d022ac58696e9528c25deeacca62b5cbf4364547a99',
        'onnx/decoder_model_quantized.onnx': 'debcc3054b9ff9aaed972bd6e251a459abe1e2f1402da1efa63163dc2f46ef73',
    }),
    ('en-zh', OUT.parent / 'translation-en-zh', '046f55aec303cdee3e0318604406d4df20f1e8ea', {
        'source.spm': '5775ddc9e3ff2fae91554da56468ad35ff56edaba870fea74447bc7234bfdaa8',
        'vocab.json': '22c957348eed495ee925afc40a36da3e387c8a34a734c8486967c2dca271613e',
        'onnx/encoder_model_quantized.onnx': 'd3b7912bf6a9bd27e4c074c2df91d4ff3d5b4bc5f7f6c8d7cc9c805c98fbafee',
        'onnx/decoder_model_quantized.onnx': '2c66a3981099b40edbb3a0d65e015d05964d42fecb9780f8422776eff5939112',
    }),
]


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def prepare(direction, out, revision, files):
    out.mkdir(parents=True, exist_ok=True)
    cache = os.environ.get('FUYI_REVERSE_CACHE') if direction == 'en-zh' else None
    for name, sha in files.items():
        path = out / pathlib.Path(name).name
        if path.exists() and digest(path) == sha:
            continue
        cached = pathlib.Path(cache) / path.name if cache else None
        temp = path.with_suffix('.part')
        if cached and cached.exists() and digest(cached) == sha:
            print('Reusing verified', direction, name, flush=True)
            shutil.copyfile(cached, temp)
        else:
            print('Fetching', direction, name, flush=True)
            urllib.request.urlretrieve(
                f'https://huggingface.co/Xenova/opus-mt-{direction}/resolve/{revision}/{name}?download=true', temp)
        if digest(temp) != sha:
            raise RuntimeError('Checksum mismatch: ' + name)
        temp.replace(path)
    # Only preparation requires these Python dependencies, never the installed app.
    import onnx
    from onnx import helper, TensorProto
    import sentencepiece as spm
    sp = spm.SentencePieceProcessor(model_file=str(out / 'source.spm'))
    vocab = json.loads((out / 'vocab.json').read_text())
    if len(vocab) != 65001 or vocab['</s>'] != 0 or vocab['<pad>'] != 65000:
        raise RuntimeError('Unexpected Marian vocabulary')
    if direction == 'en-zh' and '>>cmn_Hans<<' not in vocab:
        raise RuntimeError('Missing Simplified Chinese target token')
    (out / 'sp-map.json').write_text(json.dumps(
        [vocab.get(sp.id_to_piece(i), vocab['<unk>']) for i in range(sp.get_piece_size())], separators=(',', ':')))
    nodes = [helper.make_node('SentencepieceTokenizer', ['text', 'nbest', 'alpha', 'bos', 'eos', 'reverse'],
                             ['tokens', 'indices'], domain='ai.onnx.contrib',
                             model=base64.b64encode((out / 'source.spm').read_bytes())),
             helper.make_node('Cast', ['tokens'], ['ids'], to=TensorProto.INT64)]
    inits = [helper.make_tensor('nbest', TensorProto.INT64, [1], [0]),
             helper.make_tensor('alpha', TensorProto.FLOAT, [1], [0.0])]
    inits += [helper.make_tensor(x, TensorProto.BOOL, [1], [False]) for x in ['bos', 'eos', 'reverse']]
    graph = helper.make_graph(nodes, 'FuyiSentencePiece',
                              [helper.make_tensor_value_info('text', TensorProto.STRING, [1])],
                              [helper.make_tensor_value_info('ids', TensorProto.INT64, [None])], inits)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 17), helper.make_opsetid('ai.onnx.contrib', 1)])
    model.ir_version = 9
    onnx.checker.check_model(model)
    onnx.save(model, out / 'tokenizer.onnx')
    entries = []
    for name in ['tokenizer.onnx', 'sp-map.json', 'vocab.json', 'encoder_model_quantized.onnx', 'decoder_model_quantized.onnx']:
        path = out / name
        entries.append(dict(file=name, bytes=path.stat().st_size, sha256=digest(path)))
    (out / 'manifest.json').write_text(json.dumps(dict(repository='Xenova/opus-mt-' + direction,
                                                     revision=revision, files=entries), indent=2) + '\n')
    print('Verified bundled', direction, sum(x['bytes'] for x in entries), 'bytes', flush=True)


if __name__ == '__main__':
    for model in MODELS:
        prepare(*model)
