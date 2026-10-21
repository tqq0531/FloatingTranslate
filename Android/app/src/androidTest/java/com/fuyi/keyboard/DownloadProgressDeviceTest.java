package com.fuyi.keyboard;
import android.test.InstrumentationTestCase;
import android.app.DownloadManager;
import android.content.Intent;
import android.database.MatrixCursor;
import android.widget.ProgressBar;
import android.widget.TextView;

public final class DownloadProgressDeviceTest extends InstrumentationTestCase {
    /** Opt-in: remove only the disposable QA device's model to observe a real SDK transfer. */
    public void testLiveDownloadProgressAndReopen()throws Exception {
        if(!"true".equals(((android.test.InstrumentationTestRunner)getInstrumentation()).getArguments().getString("downloadModels")))return;
        var model=new com.google.mlkit.nl.translate.TranslateRemoteModel.Builder(com.google.mlkit.nl.translate.TranslateLanguage.CHINESE).build();
        com.google.android.gms.tasks.Tasks.await(com.google.mlkit.common.model.RemoteModelManager.getInstance().deleteDownloadedModel(model),30,java.util.concurrent.TimeUnit.SECONDS);
        final MainActivity[] activity={(MainActivity)getInstrumentation().startActivitySync(new Intent(getInstrumentation().getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))};
        try{
            getInstrumentation().runOnMainSync(activity[0]::beginLanguageDownload);
            int first=-1;String detail="";
            for(int i=0;i<1200&&first<1;i++){
                final int[] progress={-1};final String[] current={""};
                getInstrumentation().runOnMainSync(()->{ProgressBar bar=activity[0].findViewById(R.id.download_progress);if(!bar.isIndeterminate())progress[0]=bar.getProgress();current[0]=((TextView)activity[0].findViewById(R.id.download_detail)).getText().toString();});
                first=progress[0];detail=current[0];if(first<1)Thread.sleep(100);
            }
            assertTrue("No real percentage observed: "+detail,first>=1&&first<100);
            android.os.Bundle report=new android.os.Bundle();report.putString("live_download",detail);getInstrumentation().sendStatus(0,report);
            getInstrumentation().runOnMainSync(activity[0]::finish);getInstrumentation().waitForIdleSync();
            activity[0]=(MainActivity)getInstrumentation().startActivitySync(new Intent(getInstrumentation().getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            final int previous=first;boolean resumed=false;
            for(int i=0;i<100&&!resumed;i++){final boolean[] ok={false};getInstrumentation().runOnMainSync(()->{ProgressBar bar=activity[0].findViewById(R.id.download_progress);ok[0]=!bar.isIndeterminate()&&bar.getProgress()>=previous;});resumed=ok[0];if(!resumed)Thread.sleep(100);}
            assertTrue("Reopened page must recover existing transfer",resumed);
            getInstrumentation().runOnMainSync(()->{activity[0].getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_SECURE);var bar=(ProgressBar)activity[0].findViewById(R.id.download_progress);((android.widget.ScrollView)bar.getParent().getParent()).scrollTo(0,Math.max(0,bar.getTop()-250));});Thread.sleep(400);
            var bitmap=getInstrumentation().getUiAutomation().takeScreenshot();assertNotNull(bitmap);
            try(var out=new java.io.FileOutputStream(new java.io.File(getInstrumentation().getTargetContext().getExternalFilesDir(null),"download.png"))){bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,out);}bitmap.recycle();
        }finally{getInstrumentation().runOnMainSync(activity[0]::finish);}
    }
    public void testSystemCursorMappingAndModelFiltering(){
        try(var cursor=new MatrixCursor(new String[]{DownloadManager.COLUMN_STATUS,DownloadManager.COLUMN_REASON,DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR,DownloadManager.COLUMN_TOTAL_SIZE_BYTES})){
            cursor.addRow(new Object[]{DownloadManager.STATUS_PAUSED,DownloadManager.PAUSED_QUEUED_FOR_WIFI,5L,10L});cursor.moveToFirst();var p=ModelDownloadMonitor.fromRow(cursor);assertEquals(DownloadProgress.Stage.WAITING_WIFI,p.stage());assertEquals(50,p.percent());
        }
        assertTrue(ModelDownloadMonitor.isChineseModel("https://redirector.gvt1.com/edgedl/translate/offline/v5/high/r29/en_zh.zip"));
        for(String uri:new String[]{"https://example.com/translate/offline/en_zh.zip","https://redirector.gvt1.com/other/en_zh.zip","https://redirector.gvt1.com/translate/offline/en_fr.zip","content://downloads/1","bad-url"})assertFalse(uri,ModelDownloadMonitor.isChineseModel(uri));
    }
    public void testProgressBarWaitingPercentageAndInstallationStates()throws Exception {
        var activity=(MainActivity)getInstrumentation().startActivitySync(new Intent(getInstrumentation().getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        try{
            getInstrumentation().runOnMainSync(()->{
                ProgressBar bar=activity.findViewById(R.id.download_progress);TextView status=activity.findViewById(R.id.download_status),detail=activity.findViewById(R.id.download_detail);
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1));assertTrue(bar.isIndeterminate());assertFalse(detail.getText().toString().contains("%"));
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.DOWNLOADING,15L*1024*1024,30L*1024*1024));assertFalse(bar.isIndeterminate());assertEquals(50,bar.getProgress());assertEquals("50% · 15.0 MB / 30.0 MB",detail.getText().toString());
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.WAITING_WIFI,15,30));assertEquals(50,bar.getProgress());assertTrue(status.getText().toString().contains("Wi-Fi"));
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.VERIFYING,30,30));assertEquals(100,bar.getProgress());assertTrue(status.getText().toString().contains("安装"));
                activity.renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.READY,30,30));assertEquals(100,bar.getProgress());assertTrue(status.getText().toString().contains("已就绪"));
            });
        }finally{getInstrumentation().runOnMainSync(activity::finish);}
    }
    public void testInstalledModelCompletesVisibleProgress()throws Exception {
        if(!"true".equals(((android.test.InstrumentationTestRunner)getInstrumentation()).getArguments().getString("preparedModel")))return;
        var activity=(MainActivity)getInstrumentation().startActivitySync(new Intent(getInstrumentation().getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        try{
            getInstrumentation().runOnMainSync(activity::beginLanguageDownload);
            boolean ready=false;for(int i=0;i<100&&!ready;i++){final boolean[] current={false};getInstrumentation().runOnMainSync(()->current[0]=((TextView)activity.findViewById(R.id.download_status)).getText().toString().contains("已就绪"));ready=current[0];if(!ready)Thread.sleep(100);}
            assertTrue("Already-installed model should reach 100%",ready);
            getInstrumentation().runOnMainSync(()->assertEquals(100,((ProgressBar)activity.findViewById(R.id.download_progress)).getProgress()));
        }finally{getInstrumentation().runOnMainSync(activity::finish);}
    }
}
