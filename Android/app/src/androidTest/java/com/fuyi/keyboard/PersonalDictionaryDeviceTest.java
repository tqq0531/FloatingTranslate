package com.fuyi.keyboard;
import android.test.InstrumentationTestCase;
import java.nio.charset.StandardCharsets;
import java.util.List;
public final class PersonalDictionaryDeviceTest extends InstrumentationTestCase {
    public void testEncryptedPersistenceAndClear()throws Exception{
        var context=getInstrumentation().getTargetContext();var store=PersonalDictionary.get(context);store.clear();
        var file=new java.io.File(context.getNoBackupFilesDir(),"personal-words.enc");
        store.remember("shi","时","今");
        byte[] bytes=null;
        for(int i=0;i<50;i++){Thread.sleep(100);if(file.exists()){bytes=java.nio.file.Files.readAllBytes(file.toPath());if(bytes.length>28)break;}}
        assertNotNull(bytes);assertFalse(new String(bytes,StandardCharsets.UTF_8).contains("时"));
        var constructor=PersonalDictionary.class.getDeclaredConstructor(android.content.Context.class);constructor.setAccessible(true);
        var reloaded=constructor.newInstance(context);
        assertEquals("时",reloaded.rank("shi",List.of(new PinyinDictionary.Candidate("是",3))).get(0).text());
        assertEquals("时",reloaded.next("今").get(0));
        store.clear();for(int i=0;i<30&&file.exists();i++)Thread.sleep(100);assertFalse(file.exists());
        assertEquals("是",store.rank("shi",List.of(new PinyinDictionary.Candidate("是",3))).get(0).text());
    }
}
