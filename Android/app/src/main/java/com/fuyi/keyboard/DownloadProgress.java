package com.fuyi.keyboard;

import java.util.Locale;

/** Values come from the system downloader; unknown totals never imply a percentage. */
record DownloadProgress(Stage stage,long downloaded,long total) {
    enum Stage { PREPARING, DOWNLOADING, WAITING_WIFI, WAITING_NETWORK, RETRYING, VERIFYING, READY, FAILED, UNAVAILABLE }
    DownloadProgress {
        downloaded=Math.max(0,downloaded);
        if(total>0)downloaded=Math.min(downloaded,total);
    }
    boolean determinate(){return total>0||stage==Stage.READY;}
    int percent(){return stage==Stage.READY?100:total>0?(int)Math.min(100,100.0*downloaded/total):0;}
    String message(){return switch(stage){
        case PREPARING->"正在连接语言包下载服务…";
        case DOWNLOADING->"正在下载中英离线语言包";
        case WAITING_WIFI->"等待 Wi-Fi，连接后会继续下载";
        case WAITING_NETWORK->"等待网络恢复，进度会保留";
        case RETRYING->"网络暂时中断，系统正在等待重试";
        case VERIFYING->"下载完成，正在校验和安装语言包…";
        case READY->"语言包已就绪，可以断网翻译";
        case FAILED->"下载未完成，请检查 Wi-Fi 后重试";
        case UNAVAILABLE->"正在准备语言包，系统暂未提供进度";
    };}
    String detail(){
        if(stage==Stage.READY)return "100% · 语言包可离线使用";
        if(total>0)return percent()+"% · "+size(downloaded)+" / "+size(total);
        if(stage==Stage.FAILED&&downloaded==0)return "请连接 Wi-Fi 后重试";
        if(stage==Stage.VERIFYING)return "正在校验安装，请稍候…";
        if(stage==Stage.UNAVAILABLE&&downloaded==0)return "下载仍由系统处理，暂时无法读取大小";
        return downloaded>0?"已下载 "+size(downloaded)+" · 总大小待确认":"正在获取下载大小…";
    }
    private static String size(long bytes){return String.format(Locale.ROOT,"%.1f MB",bytes/(1024.0*1024));}
}
