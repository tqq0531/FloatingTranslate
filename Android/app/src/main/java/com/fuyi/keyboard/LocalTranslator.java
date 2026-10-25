package com.fuyi.keyboard;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

final class LocalTranslator implements AutoCloseable {
    // A single worker bounds native memory and allows one shared set of model sessions.
    private static final ExecutorService WORK=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"Fuyi-local-translation");t.setDaemon(true);return t;});
    private final Context context;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final AtomicInteger generation=new AtomicInteger();
    private volatile boolean closed;
    interface Result { void complete(String text,String status); }
    LocalTranslator(Context context){this.context=context.getApplicationContext();}
    void prepare(Consumer<DownloadProgress> progress,Result result){
        WORK.execute(()->{
            if(closed)return;
            try{OfflineTranslationEngine.prepare(context,value->main.post(()->{if(!closed)progress.accept(value);}));complete(result,"","离线翻译已就绪，无需联网或下载");}
            catch(Exception|LinkageError error){complete(result,"","模型初始化未完成，请检查空间与设备兼容性");}
        });
    }
    void translate(String text,Result result){translate(text,false,result);}
    void translate(String text,boolean toChinese,Result result){
        int ticket=generation.incrementAndGet();
        WORK.execute(()->{
            if(closed||ticket!=generation.get())return;
            try{var engine=OfflineTranslationEngine.get(context,toChinese,p->{});String value=engine.translate(text,()->closed||ticket!=generation.get());if(!closed&&ticket==generation.get())complete(ticket,result,value,"");}
            catch(IllegalArgumentException error){if(ticket==generation.get())complete(ticket,result,"",error.getMessage());}
            catch(Exception|LinkageError error){if(ticket==generation.get())complete(ticket,result,"","本机翻译暂不可用，请在设置检查内置模型");}
        });
    }
    private void complete(int ticket,Result result,String text,String status){main.post(()->{if(!closed&&ticket==generation.get())result.complete(text,status);});}
    private void complete(Result result,String text,String status){main.post(()->{if(!closed)result.complete(text,status);});}
    void cancel(){generation.incrementAndGet();}
    @Override public void close(){closed=true;generation.incrementAndGet();}
}
