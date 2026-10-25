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
    private android.app.Application app;
    private final android.app.Application.ActivityLifecycleCallbacks captureLifecycle=new android.app.Application.ActivityLifecycleCallbacks(){
        public void onActivityCreated(Activity a,android.os.Bundle state){if(a instanceof MainActivity)a.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_SECURE);}
        public void onActivityResumed(Activity a){if(a instanceof MainActivity){activity=a;editor=a.findViewById(R.id.test_editor);a.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_SECURE);}}
        public void onActivityStarted(Activity a){}public void onActivityPaused(Activity a){}public void onActivityStopped(Activity a){}public void onActivitySaveInstanceState(Activity a,android.os.Bundle state){}public void onActivityDestroyed(Activity a){}
    };
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
        assertNotNull("Missing key: "+label,node);boolean clicked=node.performAction(AccessibilityNodeInfo.ACTION_CLICK);
        for(var parent=node.getParent();!clicked&&parent!=null;parent=parent.getParent())clicked=parent.performAction(AccessibilityNodeInfo.ACTION_CLICK);
        assertTrue("Click: "+label,clicked);Thread.sleep(200);
    }
    private void screenshot(String name)throws Exception {
        var bitmap=getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull(bitmap);try(var out=new java.io.FileOutputStream(new java.io.File(getInstrumentation().getTargetContext().getExternalFilesDir(null),name))) {bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,out);}bitmap.recycle();
    }
    private void type(String s)throws Exception {for(char c:s.toCharArray())tap("按键 "+c,true);}
    private String text(){final String[] value={""};getInstrumentation().runOnMainSync(()->value[0]=editor.getText().toString());return value[0];}
    @Override protected void setUp()throws Exception {
        super.setUp();var info=getInstrumentation().getUiAutomation().getServiceInfo();info.flags|=AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;getInstrumentation().getUiAutomation().setServiceInfo(info);
        var context=getInstrumentation().getTargetContext();Options.prefs(context).edit().putBoolean("englishDefault",false).putBoolean("chineseDefault",false).putString("keyboardTheme","system").putInt("keyHeight",54).commit();
        app=(android.app.Application)context.getApplicationContext();app.registerActivityLifecycleCallbacks(captureLifecycle);
        activity=getInstrumentation().startActivitySync(new Intent(context,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        getInstrumentation().runOnMainSync(()->{activity.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_SECURE);editor=activity.findViewById(R.id.test_editor);editor.requestFocus();editor.requestRectangleOnScreen(new android.graphics.Rect(0,0,editor.getWidth(),editor.getHeight()));});
        getInstrumentation().waitForIdleSync();Thread.sleep(500);
        getInstrumentation().runOnMainSync(()->{editor.requestRectangleOnScreen(new android.graphics.Rect(0,0,editor.getWidth(),editor.getHeight()),true);activity.getSystemService(InputMethodManager.class).showSoftInput(editor,InputMethodManager.SHOW_IMPLICIT);});
        Thread.sleep(1200);
        getInstrumentation().runOnMainSync(()->{var scroll=(android.widget.ScrollView)activity.findViewById(R.id.settings_scroll);var rect=new android.graphics.Rect();editor.getDrawingRect(rect);scroll.offsetDescendantRectToMyCoords(editor,rect);scroll.scrollTo(0,Math.max(0,rect.top-24));});
    }
    public void testRealSoftKeyboardChineseEnglishAndHistoryInsertion()throws Exception {
        type("wo");tap("空格");type("jintian");tap("空格");type("qubeijing");tap("空格");
        assertEquals("我今天去北京",text());
        assertNotNull("Committed words must stay in whole-sentence context",find("当前中文句子：我今天去北京",true));
        Thread.sleep(1200);screenshot("keyboard.png");
        tap("切换中英文：中文拼音",true);type("abc");assertEquals("我今天去北京abc",text());
        var context=getInstrumentation().getTargetContext();Options.history(context,true);
        ClipboardStore store=new ClipboardStore(context);store.clear();store.add("先前复制的内容",System.currentTimeMillis()-3600000);store.add("另一条",System.currentTimeMillis());
        tap("剪贴板",true);tap("插入历史：先前复制的内容",true);
        assertEquals("我今天去北京abc先前复制的内容",text());
        assertEquals(2,store.list().size());screenshot("clipboard.png");
        tap("记录操作：另一条",true);tap("删除");assertEquals(1,store.list().size());
        tap("剪贴板操作",true);tap("清空");assertTrue(store.list().isEmpty());
        Options.history(context,false);
    }
    private void touch(android.graphics.Rect rect,boolean hold,boolean edge)throws Exception {
        long time=android.os.SystemClock.uptimeMillis();float x=edge?rect.left+1:rect.exactCenterX(),y=rect.exactCenterY();
        var down=android.view.MotionEvent.obtain(time,time,android.view.MotionEvent.ACTION_DOWN,x,y,0);down.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
        assertTrue(getInstrumentation().getUiAutomation().injectInputEvent(down,true));down.recycle();if(hold)Thread.sleep(850);
        var up=android.view.MotionEvent.obtain(time,android.os.SystemClock.uptimeMillis(),android.view.MotionEvent.ACTION_UP,x,y,0);up.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
        assertTrue(getInstrumentation().getUiAutomation().injectInputEvent(up,true));up.recycle();Thread.sleep(200);
    }
    private android.graphics.Rect bounds(String description){var node=find(description,true);assertNotNull(description,node);var rect=new android.graphics.Rect();node.getBoundsInScreen(rect);return rect;}
    public void testStaggeredKeysTouchGuttersRepeatAndPanels()throws Exception {
        screenshot("layout.png");
        var q=bounds("按键 q");var a=bounds("按键 a");var z=bounds("按键 z");
        assertTrue("Second row must be indented",a.centerX()>q.centerX());
        assertTrue("Third row must leave room for Shift",z.centerX()>a.centerX());
        assertTrue(bounds("大写").centerX()<z.centerX());assertTrue(bounds("退格").centerX()>bounds("按键 m").centerX());
        tap("切换中英文：中文拼音",true);type("abcde");touch(bounds("退格"),true,false);assertEquals("",text());
        touch(bounds("按键 a"),false,true);assertEquals("a",text());
        tap("大写",true);tap("按键 A",true);assertEquals("aA",text());assertNotNull(find("按键 a",true));
        tap("数字与符号",true);tap("按键 1",true);tap("数字与符号",true);assertNotNull(find("按键 a",true));
        assertNull("Simplified keyboard does not expose emoji tools",find("表情键盘",true));assertEquals("aA1",text());
    }
    public void testSendKeyUsesEditorAction()throws Exception {
        final int[] received={0};
        getInstrumentation().runOnMainSync(()->{editor.setSingleLine(true);editor.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_SEND);editor.setOnEditorActionListener((view,action,event)->{received[0]=action;return true;});activity.getSystemService(InputMethodManager.class).restartInput(editor);});
        Thread.sleep(500);tap("执行：发送",true);assertEquals(android.view.inputmethod.EditorInfo.IME_ACTION_SEND,received[0]);
    }
    public void testPreparedModelEnglishDefaultAndCopy()throws Exception {
        if(!"true".equals(((android.test.InstrumentationTestRunner)getInstrumentation()).getArguments().getString("preparedModel"))) return;
        var context=getInstrumentation().getTargetContext();Options.prefs(context).edit().putBoolean("englishDefault",true).commit();
        try {
            type("nihao");AccessibilityNodeInfo result=null;
            for(int i=0;i<50&&result==null;i++){result=find("Hello.",false);if(result==null)Thread.sleep(100);}
            assertNotNull("Expected real English preview",result);
            assertTrue(result.performAction(AccessibilityNodeInfo.ACTION_LONG_CLICK));Thread.sleep(200);
            final String[] copied={""};getInstrumentation().runOnMainSync(()->{var clip=context.getSystemService(android.content.ClipboardManager.class).getPrimaryClip();if(clip!=null)copied[0]=clip.getItemAt(0).getText().toString();});
            assertEquals("Hello.",copied[0]);tap("空格");assertEquals("Hello.",text());
        } finally {Options.prefs(context).edit().putBoolean("englishDefault",false).commit();}
    }
    public void testPasswordFieldDisablesHistory()throws Exception {
        getInstrumentation().runOnMainSync(()->{editor.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD);activity.getSystemService(InputMethodManager.class).restartInput(editor);});
        Thread.sleep(500);assertNotNull(find("安全输入",true));
        var clipboard=find("剪贴板",true);assertNotNull(clipboard);assertFalse(clipboard.isEnabled());
        type("abc");assertEquals("abc",text());
    }
    private String awaitTranslation(String source)throws Exception {
        for(int i=0;i<300;i++){
            var node=find(source,true);
            if(node!=null&&node.getText()!=null){String value=node.getText().toString();if(!value.contains("正在")&&!value.contains("输入英文")&&!value.contains("输入中文")&&!value.isEmpty())return value;}
            Thread.sleep(200);
        }
        fail("Translation did not finish: "+source);return "";
    }
    public void testEnglishSentenceAppendAndChineseOutput()throws Exception {
        tap("切换中英文：中文拼音",true);
        for(String word:new String[]{"i","want","to","go","to"}){type(word);tap("空格");}
        type("gym");tap("空格");type("now");
        String english="i want to go to gym now";assertEquals(english,text());
        String translated=awaitTranslation("当前英文句子："+english);
        assertTrue("Full sentence must include the gym: "+translated,translated.contains("健身")||translated.contains("体育"));
        assertTrue("Full sentence must retain the subject: "+translated,translated.contains("我"));
        screenshot("keyboard-english-chinese.png");tap("使用中文译文",true);assertEquals(translated,text());
        getInstrumentation().runOnMainSync(()->{editor.setText("");activity.getSystemService(InputMethodManager.class).restartInput(editor);});Thread.sleep(350);
        tap("切换中英文：中文拼音",true);type("hi");assertEquals("你好",awaitTranslation("当前英文句子：hi"));
        Options.prefs(getInstrumentation().getTargetContext()).edit().putBoolean("chineseDefault",true).commit();tap("执行：换行",true);assertEquals("你好",text());
        Options.prefs(getInstrumentation().getTargetContext()).edit().putBoolean("chineseDefault",false).commit();
    }
    public void testThemeAndHeightPreserveEnglishDraft()throws Exception {
        var context=getInstrumentation().getTargetContext();
        tap("切换中英文：中文拼音",true);type("hello");tap("空格");type("world");
        assertEquals("hello world",text());
        int normal=bounds("按键 q").height();
        tap("键盘高度与主题",true);tap("主题：深色",true);Thread.sleep(450);
        assertEquals("dark",Options.theme(context));assertEquals("hello world",text());
        screenshot("keyboard-adjust-dark.png");tap("返回键盘",true);screenshot("keyboard-dark.png");
        Options.setKeyHeight(context,68);Thread.sleep(400);int tall=bounds("按键 q").height();assertTrue("Custom height expands keys",tall>normal);
        assertEquals("hello world",text());Options.setKeyHeight(context,44);Thread.sleep(400);
        assertTrue("Custom height shrinks keys",bounds("按键 q").height()<normal);
        Options.setKeyHeight(context,54);Options.setTheme(context,"light");Thread.sleep(500);screenshot("keyboard-light.png");
        assertEquals("hello world",text());
    }
    public void testFollowSystemPaletteAndFixedOverrides()throws Exception {
        var context=getInstrumentation().getTargetContext();Options.setTheme(context,"system");
        var config=new android.content.res.Configuration(context.getResources().getConfiguration());
        config.uiMode=(config.uiMode&~android.content.res.Configuration.UI_MODE_NIGHT_MASK)|android.content.res.Configuration.UI_MODE_NIGHT_YES;
        var night=context.createConfigurationContext(config);assertTrue(KeyboardAppearance.from(night).dark);
        config.uiMode=(config.uiMode&~android.content.res.Configuration.UI_MODE_NIGHT_MASK)|android.content.res.Configuration.UI_MODE_NIGHT_NO;
        var day=context.createConfigurationContext(config);assertFalse(KeyboardAppearance.from(day).dark);
        Options.setTheme(context,"light");assertFalse(KeyboardAppearance.from(night).dark);
        Options.setTheme(context,"dark");assertTrue(KeyboardAppearance.from(day).dark);
    }
    private void shell(String command)throws Exception {
        try(var descriptor=getInstrumentation().getUiAutomation().executeShellCommand(command);var stream=new java.io.FileInputStream(descriptor.getFileDescriptor())){byte[] data=new byte[1024];while(stream.read(data)>0){}}
    }
    private float keyBrightness()throws Exception {
        android.view.accessibility.AccessibilityNodeInfo node=null;
        for(int i=0;i<50&&node==null;i++){node=find("按键 q",true);if(node==null)Thread.sleep(100);}
        assertNotNull("Keyboard must remain available after system appearance change",node);
        var rect=new android.graphics.Rect();node.getBoundsInScreen(rect);
        var image=getInstrumentation().getUiAutomation().takeScreenshot();assertNotNull(image);
        int color=image.getPixel(rect.centerX(),rect.top+rect.height()/5);image.recycle();return android.graphics.Color.luminance(color);
    }
    public void testLiveSystemNightAppearance()throws Exception {
        var context=getInstrumentation().getTargetContext();Options.setTheme(context,"system");
        try {
            shell("cmd uimode night no");Thread.sleep(1500);float light=keyBrightness();
            shell("cmd uimode night yes");Thread.sleep(1800);float dark=keyBrightness();
            assertTrue("Visible key must turn dark with the system: "+light+" / "+dark,light>0.7f&&dark<0.2f);
            screenshot("keyboard-system-dark.png");
            shell("cmd uimode night no");Thread.sleep(1800);assertTrue("Visible key must return to light",keyBrightness()>0.7f);
            screenshot("keyboard-system-light.png");
        } finally {shell("cmd uimode night no");}
    }
    @Override protected void tearDown()throws Exception {
        Options.history(getInstrumentation().getTargetContext(),false);
        Options.setTheme(getInstrumentation().getTargetContext(),"system");Options.setKeyHeight(getInstrumentation().getTargetContext(),54);
        if(activity!=null)getInstrumentation().runOnMainSync(activity::finish);
        if(app!=null)app.unregisterActivityLifecycleCallbacks(captureLifecycle);
        super.tearDown();
    }
}
