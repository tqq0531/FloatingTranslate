package com.fuyi.keyboard;

import android.test.InstrumentationTestCase;
import android.content.*;
import android.os.PersistableBundle;
import android.view.inputmethod.EditorInfo;
import android.text.InputType;
import java.io.File;
import java.nio.file.Files;
import java.util.List;

public final class ClipboardDeviceTest extends InstrumentationTestCase {
    private Context context(){return getInstrumentation().getTargetContext();}
    public void testEncryptedPersistenceAndDelete() throws Exception {
        ClipboardStore store=new ClipboardStore(context());store.clear();
        String text="device-test-你好-"+System.nanoTime();
        assertTrue(store.add(text,System.currentTimeMillis()));
        var rows=new ClipboardStore(context()).list();assertEquals(1,rows.size());assertEquals(text,rows.get(0).text());
        byte[] bytes=Files.readAllBytes(new File(context().getNoBackupFilesDir(),"clipboard.enc").toPath());
        assertFalse(new String(bytes,java.nio.charset.StandardCharsets.UTF_8).contains(text));
        store.remove(rows.get(0).id());assertTrue(new ClipboardStore(context()).list().isEmpty());
    }
    public void testExpiredRecordsCannotBeResolved() throws Exception {
        ClipboardStore store=new ClipboardStore(context());store.clear();
        var method=ClipboardStore.class.getDeclaredMethod("write",List.class);method.setAccessible(true);
        method.invoke(store,List.of(new HistoryPolicy.Entry("expired","old",System.currentTimeMillis()-HistoryPolicy.TTL)));
        assertNull(store.resolve("expired"));assertFalse(new File(context().getNoBackupFilesDir(),"clipboard.enc").exists());
    }
    public void testTamperedCiphertextFailsClosed() throws Exception {
        ClipboardStore store=new ClipboardStore(context());store.clear();store.add("tamper-test",System.currentTimeMillis());
        File file=new File(context().getNoBackupFilesDir(),"clipboard.enc");byte[] data=Files.readAllBytes(file.toPath());data[data.length-1]^=1;Files.write(file.toPath(),data);
        assertTrue(store.list().isEmpty());assertFalse(file.exists());
    }
    public void testPasswordsAndNoLearningArePrivate(){
        EditorInfo info=new EditorInfo();
        for(int type:new int[]{InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,InputType.TYPE_CLASS_NUMBER|InputType.TYPE_NUMBER_VARIATION_PASSWORD}) {info.inputType=type;assertTrue(InputSafety.privateField(info));}
        info.inputType=InputType.TYPE_CLASS_TEXT;info.imeOptions=EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING;assertTrue(InputSafety.privateField(info));
        info.imeOptions=0;assertFalse(InputSafety.privateField(info));
    }
    public void testClipboardListenerSkipsSensitiveAndPrivateFields() throws Exception {
        ClipboardStore store=new ClipboardStore(context());store.clear();Options.history(context(),true);
        android.app.Activity activity=getInstrumentation().startActivitySync(new Intent(context(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        getInstrumentation().runOnMainSync(()->{var edit=(android.widget.EditText)activity.findViewById(R.id.test_editor);edit.requestFocus();activity.getSystemService(android.view.inputmethod.InputMethodManager.class).showSoftInput(edit,0);});
        Thread.sleep(1000);
        final ClipboardCapture[] capture=new ClipboardCapture[1];final boolean[] secure={false};
        getInstrumentation().runOnMainSync(()->capture[0]=new ClipboardCapture(context(),()->secure[0],()->{}));
        try {
            assertTrue("Fixture must be the default IME",capture[0].defaultIme());
            ClipboardManager manager=context().getSystemService(ClipboardManager.class);
            getInstrumentation().runOnMainSync(()->manager.setPrimaryClip(ClipData.newPlainText("test","clipboard-device-test")));
            Thread.sleep(500);assertEquals(1,store.list().size());store.clear();
            getInstrumentation().runOnMainSync(()->{ClipData clip=ClipData.newPlainText("sensitive","must-not-save");PersistableBundle extras=new PersistableBundle();extras.putBoolean("android.content.extra.IS_SENSITIVE",true);clip.getDescription().setExtras(extras);manager.setPrimaryClip(clip);});
            Thread.sleep(500);assertTrue(store.list().isEmpty());
            secure[0]=true;
            getInstrumentation().runOnMainSync(()->{var edit=(android.widget.EditText)activity.findViewById(R.id.test_editor);edit.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_PASSWORD);activity.getSystemService(android.view.inputmethod.InputMethodManager.class).restartInput(edit);});
            Thread.sleep(400);
            getInstrumentation().runOnMainSync(()->manager.setPrimaryClip(ClipData.newPlainText("private","private-field-copy")));
            Thread.sleep(500);assertTrue(store.list().isEmpty());
            secure[0]=false;getInstrumentation().runOnMainSync(()->capture[0].capture());assertTrue(store.list().isEmpty());
        } finally {getInstrumentation().runOnMainSync(()->capture[0].close());Options.history(context(),false);store.clear();getInstrumentation().runOnMainSync(activity::finish);}
    }
}
