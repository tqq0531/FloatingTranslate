package com.fuyi.keyboard;
import android.content.Context;
import java.io.*;
import java.util.*;
/** Serialized Rime bridge; native user dictionary is disabled, encrypted learning stays in Java. */
final class NativePinyin {
    private static boolean ready;
    static synchronized boolean prepare(Context context) {
        if(ready)return true;
        try {
            System.loadLibrary("fuyi_rime");
            File shared=new File(context.getNoBackupFilesDir(),"rime-shared-v1"),user=new File(context.getCacheDir(),"rime-runtime");
            copy(context,"rime",shared);if(!user.isDirectory()&&!user.mkdirs())throw new IOException("Rime runtime directory");
            String error=initialize(new File(context.getApplicationInfo().nativeLibraryDir,"librime_jni.so").getPath(),shared.getPath(),user.getPath());
            if(!error.isEmpty())throw new IOException(error);
            ready=true;return true;
        }catch(IOException|UnsatisfiedLinkError failure){android.util.Log.e("FuyiEngine","Native engine startup failed",failure);return false;}
    }
    private static void copy(Context c,String path,File dest)throws IOException {
        String[] children=c.getAssets().list(path);
        if(children!=null&&children.length>0){if(!dest.isDirectory()&&!dest.mkdirs())throw new IOException("Rime directory");for(String child:children)copy(c,path+"/"+child,new File(dest,child));}
        else if(!dest.isFile()){
            File pending=new File(dest.getPath()+".tmp");
            try(InputStream in=c.getAssets().open(path);OutputStream out=new FileOutputStream(pending)){byte[] bytes=new byte[65536];for(int n;(n=in.read(bytes))!=-1;)out.write(bytes,0,n);}
            if(!pending.renameTo(dest))throw new IOException("Cannot install Rime data");
        }
    }
    static synchronized List<PinyinDictionary.Candidate> candidates(String raw){
        if(!ready)return List.of();String[] values=query(raw);var result=new ArrayList<PinyinDictionary.Candidate>();
        for(int i=0;i+1<values.length;i+=2){int n=Integer.parseInt(values[i+1]);if(n>0&&n<=raw.length()&&!values[i].matches(".*[A-Za-z].*"))result.add(new PinyinDictionary.Candidate(values[i],n));}
        return result;
    }
    static synchronized String engineVersion(){return ready?version():"unavailable";}
    private static native String initialize(String library,String shared,String user);
    private static native String[] query(String raw);
    private static native String version();
}
