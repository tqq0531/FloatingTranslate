package com.fuyi.keyboard;

import android.content.Context;
import android.security.keystore.*;
import android.util.AtomicFile;
import javax.crypto.*;
import javax.crypto.spec.GCMParameterSpec;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.*;
import java.util.concurrent.Executors;
import org.json.*;

final class PersonalDictionary {
    private static PersonalDictionary instance;
    static synchronized PersonalDictionary get(Context context){if(instance==null)instance=new PersonalDictionary(context.getApplicationContext());return instance;}
    private final LearnedWords words=new LearnedWords();
    private final AtomicFile file;
    private final java.util.concurrent.ExecutorService io=Executors.newSingleThreadExecutor();
    private static final String KEY="fuyi.personal.words.v1";
    private PersonalDictionary(Context context){
        file=new AtomicFile(new File(context.getNoBackupFilesDir(),"personal-words.enc"));
        try{
            if(!file.getBaseFile().exists())return;
            byte[] data=file.readFully();if(data.length<28||data.length>2000000)throw new IOException();
            Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.DECRYPT_MODE,key(),new GCMParameterSpec(128,Arrays.copyOf(data,12)));
            JSONArray json=new JSONArray(new String(cipher.doFinal(data,12,data.length-12),StandardCharsets.UTF_8));
            var rows=new ArrayList<LearnedWords.Entry>();for(int i=0;i<Math.min(2000,json.length());i++){var r=json.getJSONObject(i);rows.add(new LearnedWords.Entry(r.getString("k"),r.getString("w"),r.getLong("o")));}words.load(rows);
        }catch(Exception ignored){file.delete();}
    }
    private SecretKey key()throws Exception{
        KeyStore store=KeyStore.getInstance("AndroidKeyStore");store.load(null);
        if(store.containsAlias(KEY))return (SecretKey)store.getKey(KEY,null);
        KeyGenerator generator=KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(KEY,KeyProperties.PURPOSE_ENCRYPT|KeyProperties.PURPOSE_DECRYPT).setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build());return generator.generateKey();
    }
    synchronized List<PinyinDictionary.Candidate> rank(String raw,List<PinyinDictionary.Candidate> base){return words.rank(raw,base);}
    synchronized List<String> next(String previous){return words.next(previous);}
    synchronized void remember(String raw,String word,String previous){words.remember(raw,word,previous);save();}
    synchronized void clear(){words.clear();io.execute(file::delete);}
    private void save(){var snapshot=words.snapshot();io.execute(()->{
        FileOutputStream output=null;
        try{
            JSONArray json=new JSONArray();for(var row:snapshot)json.put(new JSONObject().put("k",row.key()).put("w",row.word()).put("o",row.order()));
            Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.ENCRYPT_MODE,key());
            byte[] encrypted=cipher.doFinal(json.toString().getBytes(StandardCharsets.UTF_8));
            output=file.startWrite();output.write(cipher.getIV());output.write(encrypted);file.finishWrite(output);
        }catch(Exception ignored){if(output!=null)file.failWrite(output);}
    });}
}
