package com.fuyi.keyboard;
import org.junit.Test;
import static org.junit.Assert.*;
public final class DownloadProgressTest {
    @Test public void unknownSizeNeverInventsPercentage(){var p=new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1);assertFalse(p.determinate());assertEquals(0,p.percent());assertFalse(p.detail().contains("%"));assertTrue(p.detail().contains("不需要网络"));}
    @Test public void actualExtractionBytesAndClamping(){var p=new DownloadProgress(DownloadProgress.Stage.EXTRACTING,15L*1024*1024,30L*1024*1024);assertTrue(p.determinate());assertEquals(50,p.percent());assertEquals("50% · 15.0 MB / 30.0 MB",p.detail());assertEquals(100,new DownloadProgress(DownloadProgress.Stage.EXTRACTING,Long.MAX_VALUE,1024).percent());assertEquals(0,new DownloadProgress(DownloadProgress.Stage.EXTRACTING,-1,100).percent());}
    @Test public void extractedBytesDoNotImplyEngineReadiness(){var p=new DownloadProgress(DownloadProgress.Stage.VERIFYING,10,10);assertEquals(100,p.percent());assertTrue(p.message().contains("加载"));assertFalse(p.message().contains("已就绪"));assertEquals(100,new DownloadProgress(DownloadProgress.Stage.READY,0,-1).percent());}
}
