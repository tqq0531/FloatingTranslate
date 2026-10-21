package com.fuyi.keyboard;
import android.test.InstrumentationTestCase;
import android.content.Intent;
import android.widget.ProgressBar;
import android.widget.TextView;

public final class DownloadProgressDeviceTest extends InstrumentationTestCase {
    public void testLocalExtractionProgressAndReady()throws Exception {
        var activity=(MainActivity)getInstrumentation().startActivitySync(new Intent(getInstrumentation().getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        try{
            boolean ready=false;for(int i=0;i<600&&!ready;i++){final boolean[] current={false};getInstrumentation().runOnMainSync(()->current[0]=((TextView)activity.findViewById(R.id.download_status)).getText().toString().contains("已就绪"));ready=current[0];if(!ready)Thread.sleep(100);}
            assertTrue("Bundled model should initialize without a network",ready);
            getInstrumentation().runOnMainSync(()->{
                ProgressBar bar=activity.findViewById(R.id.download_progress);TextView detail=activity.findViewById(R.id.download_detail);
                assertEquals(100,bar.getProgress());assertTrue(detail.getText().toString().contains("内置"));
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.EXTRACTING,15L*1024*1024,30L*1024*1024));assertFalse(bar.isIndeterminate());assertEquals(50,bar.getProgress());assertEquals("50% · 15.0 MB / 30.0 MB",detail.getText().toString());
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.VERIFYING,30,30));assertEquals(100,bar.getProgress());
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.READY,30,30));assertEquals(100,bar.getProgress());
            });
        }finally{getInstrumentation().runOnMainSync(activity::finish);}
    }
}
