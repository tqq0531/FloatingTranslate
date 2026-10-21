package com.fuyi.keyboard;
import android.test.InstrumentationTestCase;
import android.app.Activity;
import android.content.Intent;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

public final class KeyboardDeviceTest extends InstrumentationTestCase {
    private Activity activity;private EditText editor;
    private AccessibilityNodeInfo find(String label,boolean description) {
        for(var window:getInstrumentation().getUiAutomation().getWindows()) {
            var root=window.getRoot();if(root==null)continue;
            var found=walk(root,label,description);if(found!=null)return found;
        }
        return null;
    }
    private AccessibilityNodeInfo walk(AccessibilityNodeInfo node,String label,boolean description) {
        CharSequence value=description?node.getContentDescription():node.getText();
        if(value!=null&&label.contentEquals(value))return node;
        for(int i=0;i<node.getChildCount();i++){var child=node.getChild(i);if(child!=null){var result=walk(child,label,description);if(result!=null)return result;}}
        return null;
    }
    private void tap(String label) throws Exception {tap(label,false);}
    private void tap(String label,boolean description) throws Exception {
        AccessibilityNodeInfo node=null;
        for(int i=0;i<15&&node==null;i++){node=find(label,description);if(node==null)Thread.sleep(100);}
        assertNotNull("Missing key: "+label,node);assertTrue("Click: "+label,node.performAction(AccessibilityNodeInfo.ACTION_CLICK));Thread.sleep(120);
    }
    private void screenshot(String name)throws Exception {
        var bitmap=getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull(bitmap);try(var out=new java.io.FileOutputStream(new java.io.File(getInstrumentation().getTargetContext().getExternalFilesDir(null),name))) {bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,out);}bitmap.recycle();
    }
    private void type(String s)throws Exception {for(char c:s.toCharArray())tap(String.valueOf(c));}
    private String text(){final String[] value={""};getInstrumentation().runOnMainSync(()->value[0]=editor.getText().toString());return value[0];}
    @Override protected void setUp()throws Exception {
        super.setUp();var info=getInstrumentation().getUiAutomation().getServiceInfo();info.flags|=AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;getInstrumentation().getUiAutomation().setServiceInfo(info);
        var context=getInstrumentation().getTargetContext();Options.prefs(context).edit().putBoolean("englishDefault",false).commit();
        activity=getInstrumentation().startActivitySync(new Intent(context,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        getInstrumentation().runOnMainSync(()->{activity.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_SECURE);editor=activity.findViewById(R.id.test_editor);editor.requestFocus();editor.requestRectangleOnScreen(new android.graphics.Rect(0,0,editor.getWidth(),editor.getHeight()));});
        getInstrumentation().waitForIdleSync();Thread.sleep(500);
        getInstrumentation().runOnMainSync(()->{editor.requestRectangleOnScreen(new android.graphics.Rect(0,0,editor.getWidth(),editor.getHeight()),true);activity.getSystemService(InputMethodManager.class).showSoftInput(editor,InputMethodManager.SHOW_IMPLICIT);});
        Thread.sleep(1200);
        getInstrumentation().runOnMainSync(()->((android.widget.ScrollView)editor.getParent().getParent()).scrollTo(0,Math.max(0,editor.getTop()-32)));
    }
    public void testRealSoftKeyboardChineseEnglishAndHistoryInsertion()throws Exception {
        type("wo");tap("空格");type("jintian");tap("空格");type("qubeijing");tap("空格");
        assertEquals("我今天去北京",text());
        assertNotNull("Committed words must stay in whole-sentence context",find("当前中文句子：我今天去北京",true));
        Thread.sleep(1200);screenshot("keyboard.png");
        tap("中 · 拼音");type("abc");assertEquals("我今天去北京abc",text());
        var context=getInstrumentation().getTargetContext();Options.history(context,true);
        ClipboardStore store=new ClipboardStore(context);store.clear();store.add("先前复制的内容",System.currentTimeMillis()-3600000);store.add("另一条",System.currentTimeMillis());
        tap("剪贴板");tap("插入历史：先前复制的内容",true);
        assertEquals("我今天去北京abc先前复制的内容",text());
        assertEquals(2,store.list().size());screenshot("clipboard.png");
        tap("清空");assertTrue(store.list().isEmpty());
        Options.history(context,false);
    }
    public void testPreparedModelEnglishDefaultAndCopy()throws Exception {
        if(!"true".equals(((android.test.InstrumentationTestRunner)getInstrumentation()).getArguments().getString("preparedModel"))) return;
        var context=getInstrumentation().getTargetContext();Options.prefs(context).edit().putBoolean("englishDefault",true).commit();
        try {
            type("nihao");AccessibilityNodeInfo result=null;
            for(int i=0;i<50&&result==null;i++){result=find("Hello",false);if(result==null)Thread.sleep(100);}
            assertNotNull("Expected real English preview",result);
            assertTrue(result.performAction(AccessibilityNodeInfo.ACTION_LONG_CLICK));Thread.sleep(200);
            final String[] copied={""};getInstrumentation().runOnMainSync(()->{var clip=context.getSystemService(android.content.ClipboardManager.class).getPrimaryClip();if(clip!=null)copied[0]=clip.getItemAt(0).getText().toString();});
            assertEquals("Hello",copied[0]);tap("空格");assertEquals("Hello",text());
        } finally {Options.prefs(context).edit().putBoolean("englishDefault",false).commit();}
    }
    public void testPasswordFieldDisablesHistory()throws Exception {
        getInstrumentation().runOnMainSync(()->{editor.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD);activity.getSystemService(InputMethodManager.class).restartInput(editor);});
        Thread.sleep(500);assertNotNull(find("安全输入",false));
        var clipboard=find("剪贴板",false);assertNotNull(clipboard);assertFalse(clipboard.isEnabled());
        type("abc");assertEquals("abc",text());
    }
    @Override protected void tearDown()throws Exception {
        Options.history(getInstrumentation().getTargetContext(),false);
        if(activity!=null)getInstrumentation().runOnMainSync(activity::finish);
        super.tearDown();
    }
}
