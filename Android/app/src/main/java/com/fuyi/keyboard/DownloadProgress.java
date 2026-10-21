package com.fuyi.keyboard;
import java.util.Locale;
/** Local APK extraction bytes. No downloader or network request exists in this build. */
record DownloadProgress(Stage stage,long downloaded,long total) {
    enum Stage { PREPARING, EXTRACTING, VERIFYING, READY, FAILED }
    DownloadProgress { downloaded=Math.max(0,downloaded);if(total>0)downloaded=Math.min(downloaded,total); }
    boolean determinate(){return total>0||stage==Stage.READY;}
    int percent(){return stage==Stage.READY?100:total>0?(int)Math.min(100,100.0*downloaded/total):0;}
    String message(){return switch(stage){
        case PREPARING->"中英模型已内置，正在准备本机翻译…";
        case EXTRACTING->"正在从安装包解压离线模型";
        case VERIFYING->"解压完成，正在加载本机翻译引擎…";
        case READY->"离线翻译已就绪，无需联网或下载";
        case FAILED->"模型初始化未完成，请检查空间与设备兼容性";
    };}
    String detail(){
        if(stage==Stage.READY)return "100% · 模型随应用内置";
        if(stage==Stage.FAILED)return "请保留至少 500 MB 空间；仍失败请检查设备兼容性";
        if(stage==Stage.VERIFYING)return "正在加载引擎，请稍候…";
        if(total>0)return percent()+"% · "+size(downloaded)+" / "+size(total);
        return "读取安装包内的模型，不需要网络";
    }
    private static String size(long bytes){return String.format(Locale.ROOT,"%.1f MB",bytes/(1024.0*1024));}
}
