package com.fuyi.keyboard;
import org.junit.Test;
import static org.junit.Assert.*;
public final class DownloadProgressTest {
    @Test public void unknownSizeNeverInventsPercentage(){var p=new DownloadProgress(DownloadProgress.Stage.DOWNLOADING,1024*1024,-1);assertFalse(p.determinate());assertEquals(0,p.percent());assertTrue(p.detail().contains("已下载 1.0 MB"));assertFalse(p.detail().contains("%"));}
    @Test public void actualBytesAndClamping(){var p=new DownloadProgress(DownloadProgress.Stage.DOWNLOADING,15L*1024*1024,30L*1024*1024);assertTrue(p.determinate());assertEquals(50,p.percent());assertEquals("50% · 15.0 MB / 30.0 MB",p.detail());assertEquals(100,new DownloadProgress(DownloadProgress.Stage.DOWNLOADING,Long.MAX_VALUE,1024).percent());assertEquals(0,new DownloadProgress(DownloadProgress.Stage.DOWNLOADING,-1,100).percent());}
    @Test public void pauseKeepsBytesAndDownloadCompletionIsNotModelReadiness(){var p=new DownloadProgress(DownloadProgress.Stage.WAITING_WIFI,5,10);assertEquals(50,p.percent());assertTrue(p.message().contains("Wi-Fi"));var verifying=new DownloadProgress(DownloadProgress.Stage.VERIFYING,10,10);assertEquals(100,verifying.percent());assertTrue(verifying.message().contains("校验"));assertFalse(verifying.message().contains("已就绪"));assertEquals(100,new DownloadProgress(DownloadProgress.Stage.READY,0,-1).percent());}
}
