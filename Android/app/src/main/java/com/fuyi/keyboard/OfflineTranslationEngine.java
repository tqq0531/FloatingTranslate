package com.fuyi.keyboard;

import android.content.Context;
import ai.onnxruntime.*;
import ai.onnxruntime.extensions.OrtxPackage;
import org.json.*;
import java.io.*;
import java.nio.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** Shared CPU engine. All artifacts come from APK assets; input is never written to disk. */
final class OfflineTranslationEngine {
    private static OfflineTranslationEngine instance;
    private final OrtEnvironment env;
    private OrtSession tokenizer,encoder,decoder;
    private int[] spMap;
    private String[] vocabulary;
    static synchronized OfflineTranslationEngine get(Context context,Consumer<DownloadProgress> progress)throws Exception {
        if(instance==null){
            var candidate=new OfflineTranslationEngine();
            try{candidate.load(context,progress);instance=candidate;}
            catch(Exception|LinkageError error){candidate.release();throw error;}
        }
        progress.accept(new DownloadProgress(DownloadProgress.Stage.READY,0,-1));return instance;
    }
    private OfflineTranslationEngine()throws OrtException {env=OrtEnvironment.getEnvironment(OrtLoggingLevel.ORT_LOGGING_LEVEL_ERROR,"FuyiOffline");env.setTelemetry(false);}
    private void load(Context context,Consumer<DownloadProgress> progress)throws Exception {
        var assets=context.getAssets();JSONObject manifest;
        try(var in=assets.open("translation/manifest.json")){manifest=new JSONObject(new String(bytes(in),java.nio.charset.StandardCharsets.UTF_8));}
        JSONArray files=manifest.getJSONArray("files");long total=0,completed=0;
        for(int i=0;i<files.length();i++)total+=files.getJSONObject(i).getLong("bytes");
        File dir=new File(context.getNoBackupFilesDir(),"translation-v1");if(!dir.isDirectory()&&!dir.mkdirs())throw new IOException("Storage unavailable");
        byte[] buffer=new byte[65536];
        for(int i=0;i<files.length();i++){
            var item=files.getJSONObject(i);String name=item.getString("file"),sha=item.getString("sha256");long size=item.getLong("bytes");File target=new File(dir,name);
            if(!valid(target,size,sha,buffer)){
                File partial=new File(dir,name+".part");
                try(var in=assets.open("translation/"+name);var out=new FileOutputStream(partial)){
                    MessageDigest digest=MessageDigest.getInstance("SHA-256");int n;long bytes=0,last=0;
                    while((n=in.read(buffer))!=-1){out.write(buffer,0,n);digest.update(buffer,0,n);bytes+=n;if(bytes-last>=1024*1024||bytes==size){progress.accept(new DownloadProgress(DownloadProgress.Stage.EXTRACTING,completed+bytes,total));last=bytes;}}
                    out.getFD().sync();if(bytes!=size||!hex(digest.digest()).equals(sha))throw new IOException("Model integrity check failed");
                } catch(Exception error){partial.delete();throw error;}
                if(target.exists()&&!target.delete())throw new IOException("Cannot replace model");
                if(!partial.renameTo(target))throw new IOException("Cannot install model");
            }
            completed+=size;progress.accept(new DownloadProgress(DownloadProgress.Stage.EXTRACTING,completed,total));
        }
        progress.accept(new DownloadProgress(DownloadProgress.Stage.VERIFYING,total,total));
        JSONArray mapping=new JSONArray(read(new File(dir,"sp-map.json")));spMap=new int[mapping.length()];for(int i=0;i<spMap.length;i++)spMap[i]=mapping.getInt(i);
        JSONObject vocab=new JSONObject(read(new File(dir,"vocab.json")));vocabulary=new String[65001];for(var it=vocab.keys();it.hasNext();){String key=it.next();vocabulary[vocab.getInt(key)]=key;}
        try(var opts=options()) {opts.registerCustomOpLibrary(OrtxPackage.getLibraryPath());tokenizer=env.createSession(new File(dir,"tokenizer.onnx").getPath(),opts);}
        try(var opts=options()){encoder=env.createSession(new File(dir,"encoder_model_quantized.onnx").getPath(),opts);decoder=env.createSession(new File(dir,"decoder_model_quantized.onnx").getPath(),opts);}
    }
    private static byte[] bytes(InputStream in)throws IOException {var out=new ByteArrayOutputStream();byte[] buffer=new byte[8192];int n;while((n=in.read(buffer))!=-1)out.write(buffer,0,n);return out.toByteArray();}
    private static String read(File path)throws IOException {try(var in=new FileInputStream(path)){return new String(bytes(in),java.nio.charset.StandardCharsets.UTF_8);}}
    private static boolean valid(File path,long size,String sha,byte[] buffer)throws Exception {
        if(!path.isFile()||path.length()!=size)return false;var digest=MessageDigest.getInstance("SHA-256");try(var in=new FileInputStream(path)){int n;while((n=in.read(buffer))!=-1)digest.update(buffer,0,n);}return hex(digest.digest()).equals(sha);
    }
    private static String hex(byte[] bytes){var out=new StringBuilder();for(byte b:bytes)out.append(String.format(java.util.Locale.ROOT,"%02x",b&255));return out.toString();}
    private static OrtSession.SessionOptions options()throws OrtException {
        var opts=new OrtSession.SessionOptions();opts.setIntraOpNumThreads(2);opts.setInterOpNumThreads(1);opts.setMemoryPatternOptimization(false);opts.setCPUArenaAllocator(false);return opts;
    }
    String translate(String source,BooleanSupplier cancelled)throws Exception {
        long[] ids;
        try(var input=OnnxTensor.createTensor(env,new String[]{source});var result=tokenizer.run(Map.of("text",input))){
            long[] pieces=(long[])result.get(0).getValue();if(pieces.length>256)throw new IllegalArgumentException("句子较长，请分句输入");
            ids=new long[pieces.length+1];for(int i=0;i<pieces.length;i++)ids[i]=spMap[(int)pieces[i]];ids[pieces.length]=0;
        }
        if(cancelled.getAsBoolean())return "";
        long[] mask=new long[ids.length];Arrays.fill(mask,1);
        try(var input=OnnxTensor.createTensor(env,new long[][]{ids});var attention=OnnxTensor.createTensor(env,new long[][]{mask});var encoded=encoder.run(Map.of("input_ids",input,"attention_mask",attention))){
            OnnxTensor hidden=(OnnxTensor)encoded.get(0);List<Long> output=new ArrayList<>();output.add(65000L);StringBuilder english=new StringBuilder();
            for(int step=0;step<128;step++){
                if(cancelled.getAsBoolean())return "";
                long[] prefix=new long[output.size()];for(int i=0;i<prefix.length;i++)prefix[i]=output.get(i);
                try(var tokens=OnnxTensor.createTensor(env,new long[][]{prefix});var decoded=decoder.run(Map.of("input_ids",tokens,"encoder_hidden_states",hidden,"encoder_attention_mask",attention),Set.of("logits"))){
                    OnnxTensor logits=(OnnxTensor)decoded.get(0);FloatBuffer scores=logits.getFloatBuffer();int offset=(prefix.length-1)*vocabulary.length,best=0;float max=-Float.MAX_VALUE;
                    for(int v=0;v<vocabulary.length-1;v++){float value=scores.get(offset+v);if(value>max){max=value;best=v;}}
                    if(best==0)return english.toString().replace('▁',' ').trim();
                    String piece=vocabulary[best];if(piece==null)throw new IOException("Invalid model token");
                    if(!piece.equals("<unk>"))english.append(piece);output.add((long)best);
                }
            }
            throw new IllegalArgumentException("句子较长，请分句翻译");
        }
    }
    private void release(){for(var session:new OrtSession[]{tokenizer,encoder,decoder})if(session!=null)try{session.close();}catch(OrtException ignored){}}
}
