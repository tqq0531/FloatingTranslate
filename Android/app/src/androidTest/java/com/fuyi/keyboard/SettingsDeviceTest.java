package com.fuyi.keyboard;

import android.test.InstrumentationTestCase;
import android.content.Intent;
import android.widget.Switch;
import android.widget.ScrollView;
import android.view.View;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityNodeInfo;

public final class SettingsDeviceTest extends InstrumentationTestCase {
    private MainActivity activity;
    @Override protected void setUp()throws Exception{
        super.setUp();var service=getInstrumentation().getUiAutomation().getServiceInfo();service.flags|=android.accessibilityservice.AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;getInstrumentation().getUiAutomation().setServiceInfo(service);activity=(MainActivity)getInstrumentation().startActivitySync(new Intent(getInstrumentation().getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));getInstrumentation().runOnMainSync(()->activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE));getInstrumentation().waitForIdleSync();Thread.sleep(600);
    }
    public void testPreferenceActionsAndPrivacyDisclosure()throws Exception{
        var context=getInstrumentation().getTargetContext();boolean english=Options.prefs(context).getBoolean("englishDefault",false);Options.history(context,false);
        try{
            getInstrumentation().runOnMainSync(()->{Switch history=activity.findViewById(R.id.history_switch);history.performClick();assertTrue(Options.history(context));});
            var store=new ClipboardStore(context);store.add("设置测试记录",System.currentTimeMillis());assertEquals(1,store.list().size());
            getInstrumentation().runOnMainSync(()->{((Switch)activity.findViewById(R.id.history_switch)).performClick();assertFalse(Options.history(context));assertTrue(store.list().isEmpty());Switch output=activity.findViewById(R.id.english_switch);output.setChecked(!english);assertEquals(!english,Options.prefs(context).getBoolean("englishDefault",english));
                View detail=activity.findViewById(R.id.privacy_details);assertEquals(View.GONE,detail.getVisibility());activity.findViewById(R.id.privacy_expand).performClick();assertEquals(View.VISIBLE,detail.getVisibility());assertTrue(((android.widget.TextView)detail).getText().toString().contains("不能承诺完全不联网"));activity.findViewById(R.id.privacy_expand).performClick();assertEquals(View.GONE,detail.getVisibility());});
            Options.history(context,true);store.add("待清空的测试记录",System.currentTimeMillis());
            getInstrumentation().runOnMainSync(()->activity.findViewById(R.id.clear_history).performClick());Thread.sleep(200);assertEquals("Confirmation must not clear immediately",1,store.list().size());
            AccessibilityNodeInfo root=null;for(int i=0;i<20&&root==null;i++){root=getInstrumentation().getUiAutomation().getRootInActiveWindow();if(root==null)Thread.sleep(100);}assertNotNull(root);boolean clicked=false;
            for(var node:root.findAccessibilityNodeInfosByText("清空")){if("清空".contentEquals(node.getText()==null?"":node.getText())){clicked=node.performAction(AccessibilityNodeInfo.ACTION_CLICK);for(var parent=node.getParent();!clicked&&parent!=null;parent=parent.getParent())clicked=parent.performAction(AccessibilityNodeInfo.ACTION_CLICK);if(clicked)break;}}
            assertTrue("Clear confirmation action",clicked);Thread.sleep(300);assertTrue(store.list().isEmpty());
        }finally{Options.history(context,false);Options.prefs(context).edit().putBoolean("englishDefault",english).commit();}
    }
    private void capture(String name)throws Exception{
        getInstrumentation().waitForIdleSync();Thread.sleep(400);var bitmap=getInstrumentation().getUiAutomation().takeScreenshot();assertNotNull(bitmap);try(var output=new java.io.FileOutputStream(new java.io.File(getInstrumentation().getTargetContext().getExternalFilesDir(null),name))){bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,output);}bitmap.recycle();
    }
    public void testSettingsVisualCapture()throws Exception{
        getInstrumentation().runOnMainSync(()->{activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);activity.getSystemService(android.view.inputmethod.InputMethodManager.class).hideSoftInputFromWindow(activity.getWindow().getDecorView().getWindowToken(),0);((ScrollView)activity.findViewById(R.id.settings_scroll)).scrollTo(0,0);});capture("settings-home.png");
        getInstrumentation().runOnMainSync(()->{var scroll=(ScrollView)activity.findViewById(R.id.settings_scroll);var bar=activity.findViewById(R.id.download_button);var rect=new android.graphics.Rect();bar.getDrawingRect(rect);scroll.offsetDescendantRectToMyCoords(bar,rect);scroll.scrollTo(0,Math.max(0,rect.top-180));});capture("settings-details.png");
    }
    @Override protected void tearDown()throws Exception{if(activity!=null)getInstrumentation().runOnMainSync(activity::finish);super.tearDown();}
}
