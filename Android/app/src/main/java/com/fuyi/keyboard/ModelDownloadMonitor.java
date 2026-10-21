package com.fuyi.keyboard;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;
import android.os.Handler;
import android.os.Looper;
import java.net.URI;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Only reads this app's translation-model downloads, with no storage permission. */
final class ModelDownloadMonitor implements AutoCloseable {
    private final DownloadManager manager;
    private final ConnectivityManager connectivity;
    private final Consumer<DownloadProgress> listener;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final ScheduledExecutorService worker=Executors.newSingleThreadScheduledExecutor();
    private volatile boolean closed;
    private long observedId=-1;
    ModelDownloadMonitor(Context context,Consumer<DownloadProgress> listener){
        manager=context.getApplicationContext().getSystemService(DownloadManager.class);
        connectivity=context.getApplicationContext().getSystemService(ConnectivityManager.class);
        this.listener=listener;
    }
    void start(){worker.scheduleWithFixedDelay(()->{
        DownloadProgress value=read();main.post(()->{if(!closed)listener.accept(value);});
    },0,500,TimeUnit.MILLISECONDS);}
    DownloadProgress read(){
        try {
            if(manager==null)return new DownloadProgress(DownloadProgress.Stage.UNAVAILABLE,0,-1);
            DownloadManager.Query query=new DownloadManager.Query();
            if(observedId>=0)query.setFilterById(observedId);
            else query.setFilterByStatus(DownloadManager.STATUS_PENDING|DownloadManager.STATUS_RUNNING|DownloadManager.STATUS_PAUSED);
            try(Cursor cursor=manager.query(query)){
                long newest=-1;DownloadProgress value=null;
                if(cursor!=null)while(cursor.moveToNext()){
                    if(!isChineseModel(cursor.getString(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_URI))))continue;
                    long id=cursor.getLong(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_ID));
                    if(id<=newest)continue;newest=id;
                    value=fromRow(cursor);
                }
                if(value!=null){observedId=newest;return value;}
            }
            observedId=-1;
            var caps=connectivity==null?null:connectivity.getNetworkCapabilities(connectivity.getActiveNetwork());
            return new DownloadProgress(caps==null||!caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)?DownloadProgress.Stage.WAITING_WIFI:DownloadProgress.Stage.PREPARING,0,-1);
        }catch(RuntimeException e){return new DownloadProgress(DownloadProgress.Stage.UNAVAILABLE,0,-1);}
    }
    static boolean isChineseModel(String address){
        try{URI uri=URI.create(address);String host=uri.getHost(),path=uri.getPath();
            return "https".equals(uri.getScheme())&&host!=null&&(host.equals("redirector.gvt1.com")||host.endsWith(".gvt1.com")||host.endsWith(".googleusercontent.com")||host.equals("www.gstatic.com"))&&path!=null&&path.matches(".*/translate/offline/.*(?:en_zh|zh_en)\\.zip");
        }catch(RuntimeException e){return false;}
    }
    static DownloadProgress fromRow(Cursor cursor){
        int state=cursor.getInt(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_STATUS));
        int reason=cursor.getInt(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_REASON));
        DownloadProgress.Stage stage=switch(state){
            case DownloadManager.STATUS_RUNNING->DownloadProgress.Stage.DOWNLOADING;
            case DownloadManager.STATUS_PAUSED->switch(reason){
                case DownloadManager.PAUSED_QUEUED_FOR_WIFI->DownloadProgress.Stage.WAITING_WIFI;
                case DownloadManager.PAUSED_WAITING_FOR_NETWORK->DownloadProgress.Stage.WAITING_NETWORK;
                default->DownloadProgress.Stage.RETRYING;
            };
            case DownloadManager.STATUS_SUCCESSFUL->DownloadProgress.Stage.VERIFYING;
            case DownloadManager.STATUS_FAILED->DownloadProgress.Stage.FAILED;
            default->DownloadProgress.Stage.PREPARING;
        };
        return new DownloadProgress(stage,cursor.getLong(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR)),cursor.getLong(cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_TOTAL_SIZE_BYTES)));
    }
    @Override public void close(){closed=true;worker.shutdownNow();main.removeCallbacksAndMessages(null);}
}
