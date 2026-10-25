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
    private final boolean toChinese;
    private int targetLanguage=-1;
    static synchronized OfflineTranslationEngine get(Context context,Consumer<DownloadProgress> progress)throws Exception {
        return get(context,false,progress);
    }
    static synchronized OfflineTranslationEngine get(Context context,boolean toChinese,Consumer<DownloadProgress> progress)throws Exception {
        if(instance!=null&&instance.toChinese!=toChinese){instance.release();instance=null;}
        if(instance==null){
            var candidate=new OfflineTranslationEngine(toChinese);
            try{candidate.load(context,progress);instance=candidate;}
            catch(Exception|LinkageError error){candidate.release();throw error;}
        }
        progress.accept(new DownloadProgress(DownloadProgress.Stage.READY,0,-1));return instance;
    }
    /** Validate and open both directions serially; only one native model is resident. */
    static synchronized void prepare(Context context,Consumer<DownloadProgress> progress)throws Exception {
        long[] sizes=new long[2];
        for(int direction=0;direction<2;direction++){
            JSONArray files=manifest(context,direction==1).getJSONArray("files");
            for(int i=0;i<files.length();i++)sizes[direction]+=files.getJSONObject(i).getLong("bytes");
        }
        long total=sizes[0]+sizes[1];
        for(int direction=0;direction<2;direction++){
            final long offset=direction==0?0:sizes[0];
            get(context,direction==1,value->{
                if(value.stage()!=DownloadProgress.Stage.READY)
                    progress.accept(new DownloadProgress(value.stage(),offset+value.downloaded(),total));
            });
        }
        progress.accept(new DownloadProgress(DownloadProgress.Stage.READY,total,total));
    }
    private OfflineTranslationEngine(boolean toChinese)throws OrtException {
        this.toChinese=toChinese;
        env=OrtEnvironment.getEnvironment(OrtLoggingLevel.ORT_LOGGING_LEVEL_ERROR,"FuyiOffline");env.setTelemetry(false);
    }
    private static String assetsPath(boolean toChinese){return toChinese?"translation-en-zh":"translation";}
    private static JSONObject manifest(Context context,boolean toChinese)throws Exception {
        try(var in=context.getAssets().open(assetsPath(toChinese)+"/manifest.json")){
            return new JSONObject(new String(bytes(in),java.nio.charset.StandardCharsets.UTF_8));
        }
    }
    private void load(Context context,Consumer<DownloadProgress> progress)throws Exception {
        var assets=context.getAssets();JSONObject manifest=manifest(context,toChinese);
        JSONArray files=manifest.getJSONArray("files");long total=0,completed=0;
        for(int i=0;i<files.length();i++)total+=files.getJSONObject(i).getLong("bytes");
        File dir=new File(context.getNoBackupFilesDir(),toChinese?"translation-en-zh-v1":"translation-v1");
        if(!dir.isDirectory()&&!dir.mkdirs())throw new IOException("Storage unavailable");
        byte[] buffer=new byte[65536];
        for(int i=0;i<files.length();i++){
            var item=files.getJSONObject(i);String name=item.getString("file"),sha=item.getString("sha256");long size=item.getLong("bytes");
            if(name.contains("/")||name.contains("\\"))throw new IOException("Invalid model path");
            File target=new File(dir,name);
            if(!valid(target,size,sha,buffer)){
                File partial=new File(dir,name+".part");
                try(var in=assets.open(assetsPath(toChinese)+"/"+name);var out=new FileOutputStream(partial)){
                    MessageDigest digest=MessageDigest.getInstance("SHA-256");int n;long count=0,last=0;
                    while((n=in.read(buffer))!=-1){out.write(buffer,0,n);digest.update(buffer,0,n);count+=n;if(count-last>=1024*1024||count==size){progress.accept(new DownloadProgress(DownloadProgress.Stage.EXTRACTING,completed+count,total));last=count;}}
                    out.getFD().sync();if(count!=size||!hex(digest.digest()).equals(sha))throw new IOException("Model integrity check failed");
                } catch(Exception error){partial.delete();throw error;}
                if(target.exists()&&!target.delete())throw new IOException("Cannot replace model");
                if(!partial.renameTo(target))throw new IOException("Cannot install model");
            }
            completed+=size;progress.accept(new DownloadProgress(DownloadProgress.Stage.EXTRACTING,completed,total));
        }
        progress.accept(new DownloadProgress(DownloadProgress.Stage.VERIFYING,total,total));
        JSONArray mapping=new JSONArray(read(new File(dir,"sp-map.json")));spMap=new int[mapping.length()];for(int i=0;i<spMap.length;i++)spMap[i]=mapping.getInt(i);
        JSONObject vocab=new JSONObject(read(new File(dir,"vocab.json")));vocabulary=new String[65001];
        for(var it=vocab.keys();it.hasNext();){String key=it.next();int id=vocab.getInt(key);if(id<0||id>=vocabulary.length)throw new IOException("Invalid vocabulary");vocabulary[id]=key;}
        if(toChinese){targetLanguage=vocab.optInt(">>cmn_Hans<<",-1);if(targetLanguage<0)throw new IOException("Simplified Chinese target missing");}
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
        if(cancelled.getAsBoolean()||source.trim().isEmpty())return "";
        String inputText=source.trim();
        // Whole-input greetings are ambiguous to the subtitle model. Sentence
        // translations below always use the complete neural translation model.
        if(toChinese&&inputText.matches("(?i)(hi|hey|hello)[.!?]*"))return "你好";
        if(toChinese&&Character.isLetterOrDigit(inputText.charAt(inputText.length()-1)))inputText+=".";
        long[] ids;
        try(var input=OnnxTensor.createTensor(env,new String[]{inputText});var result=tokenizer.run(Map.of("text",input))){
            long[] pieces=(long[])result.get(0).getValue();if(pieces.length>256)throw new IllegalArgumentException("句子较长，请分句输入");
            int offset=toChinese?1:0;ids=new long[pieces.length+1+offset];if(toChinese)ids[0]=targetLanguage;
            for(int i=0;i<pieces.length;i++){
                if(pieces[i]<0||pieces[i]>=spMap.length)throw new IOException("Invalid source token");
                ids[i+offset]=spMap[(int)pieces[i]];
            }
            ids[ids.length-1]=0;
        }
        if(cancelled.getAsBoolean())return "";
        long[] mask=new long[ids.length];Arrays.fill(mask,1);
        try(var input=OnnxTensor.createTensor(env,new long[][]{ids});var attention=OnnxTensor.createTensor(env,new long[][]{mask});var encoded=encoder.run(Map.of("input_ids",input,"attention_mask",attention))){
            OnnxTensor hidden=(OnnxTensor)encoded.get(0);
            List<Beam> beams=new ArrayList<>();beams.add(new Beam(new long[]{65000},0,false));
            String lower=source.toLowerCase(Locale.ROOT);
            boolean shortUtterance=toChinese&&source.trim().split("\\s+").length<=8&&
                !source.matches("(?s).*[.,;:!?\\n].*")&&!lower.contains(" and ")&&!lower.contains(" but ")&&!lower.contains(" because ");
            // Beam decoding and four-token loop suppression prevent the greedy
            // decoder from extending short utterances into repeated clauses.
            int beamWidth=toChinese?4:2;
            for(int step=0;step<128;step++){
                List<Beam> next=new ArrayList<>();
                for(Beam beam:beams){
                    if(cancelled.getAsBoolean())return "";
                    if(beam.done){next.add(beam);continue;}
                    long[] prefix=beam.tokens;
                    try(var tokens=OnnxTensor.createTensor(env,new long[][]{prefix});var decoded=decoder.run(Map.of("input_ids",tokens,"encoder_hidden_states",hidden,"encoder_attention_mask",attention),Set.of("logits"))){
                        FloatBuffer scores=((OnnxTensor)decoded.get(0)).getFloatBuffer();int offset=(prefix.length-1)*vocabulary.length;
                        float[] adjusted=new float[vocabulary.length-1];float maximum=-Float.MAX_VALUE;
                        for(int v=0;v<adjusted.length;v++){adjusted[v]=scores.get(offset+v);maximum=Math.max(maximum,adjusted[v]);}
                        double sum=0;for(float score:adjusted)sum+=Math.exp(score-maximum);
                        double normalizer=maximum+Math.log(sum);
                        if(prefix.length>=4)for(int i=1;i+3<prefix.length;i++)
                            if(prefix[i]==prefix[prefix.length-3]&&prefix[i+1]==prefix[prefix.length-2]&&prefix[i+2]==prefix[prefix.length-1])adjusted[(int)prefix[i+3]]=Float.NEGATIVE_INFINITY;
                        for(int k=0;k<beamWidth;k++){
                            int best=0;for(int v=1;v<adjusted.length;v++)if(adjusted[v]>adjusted[best])best=v;
                            long[] output=Arrays.copyOf(prefix,prefix.length+1);output[prefix.length]=best;
                            String piece=vocabulary[best];if(piece==null)throw new IOException("Invalid model token");
                            boolean done=best==0||(shortUtterance&&prefix.length>2&&piece.contains("。"));
                            next.add(new Beam(output,beam.score+adjusted[best]-normalizer,done));adjusted[best]=Float.NEGATIVE_INFINITY;
                        }
                    }
                }
                next.sort((a,b)->Double.compare(b.score,a.score));
                beams=new ArrayList<>(next.subList(0,Math.min(beamWidth,next.size())));
                if(beams.get(0).done){
                    StringBuilder text=new StringBuilder();long[] result=beams.get(0).tokens;
                    for(int i=1;i<result.length&&result[i]!=0;i++){String piece=vocabulary[(int)result[i]];text.append(piece.equals("<unk>")?"�":piece);}
                    return text.toString().replace('▁',' ').trim();
                }
            }
            throw new IllegalArgumentException("句子较长，请分句翻译");
        }
    }
    private static final class Beam {
        final long[] tokens;final double score;final boolean done;
        Beam(long[] tokens,double score,boolean done){this.tokens=tokens;this.score=score;this.done=done;}
    }
    private void release(){for(var session:new OrtSession[]{tokenizer,encoder,decoder})if(session!=null)try{session.close();}catch(OrtException ignored){}}
}
