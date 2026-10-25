package com.fuyi.keyboard;
import android.test.InstrumentationTestCase;
import android.content.pm.PackageManager;
import java.util.*;

/** Run on a fresh installation with Wi-Fi/data disabled, before any model initialization. */
public final class TranslationModelDeviceTest extends InstrumentationTestCase {
    public void testBundledModelOfflineAndRealTranslation()throws Exception {
        var context=getInstrumentation().getTargetContext();
        var permissions=context.getPackageManager().getPackageInfo(context.getPackageName(),PackageManager.GET_PERMISSIONS).requestedPermissions;
        assertFalse("APK must not request INTERNET",permissions!=null&&Arrays.asList(permissions).contains("android.permission.INTERNET"));
        List<DownloadProgress> progress=new ArrayList<>();long start=System.currentTimeMillis();
        var engine=OfflineTranslationEngine.get(context,progress::add);
        assertEquals(DownloadProgress.Stage.READY,progress.get(progress.size()-1).stage());
        for(String input:new String[]{"你好","我今天去北京。","为什么不一起翻译了？"}){
            long tick=System.currentTimeMillis();String result=engine.translate(input,()->false);
            assertFalse(result.isEmpty());assertTrue("English missing: "+result,result.matches("(?s).*[A-Za-z].*"));
            if(input.contains("北京"))assertTrue(result,result.toLowerCase(Locale.ROOT).contains("beijing")||result.toLowerCase(Locale.ROOT).contains("peking"));
            var report=new android.os.Bundle();report.putString("translation",input+" → "+result);report.putLong("translation_ms",System.currentTimeMillis()-tick);getInstrumentation().sendStatus(0,report);
        }
        assertEquals("",engine.translate("这句已被用户撤销",()->true));
        engine=OfflineTranslationEngine.get(context,true,progress::add);
        assertEquals("你好",engine.translate("hi",()->false));
        for(String input:new String[]{"I want to go to the gym now", "The cat is sleeping on the sofa.", "Hi my name is Jack"}){
            long tick=System.currentTimeMillis();String result=engine.translate(input,()->false);
            assertTrue("Chinese missing: "+result,result.matches("(?s).*[\\u4e00-\\u9fff].*"));
            if(input.contains("gym"))assertTrue(result,result.contains("健身"));
            if(input.contains("cat"))assertTrue(result,result.contains("猫"));
            if(input.contains("Jack")){
                assertTrue(result,result.contains("杰克"));
                assertEquals("Name repeated: "+result,result.indexOf("杰克"),result.lastIndexOf("杰克"));
            }
            var report=new android.os.Bundle();report.putString("translation",input+" → "+result);report.putLong("translation_ms",System.currentTimeMillis()-tick);getInstrumentation().sendStatus(0,report);
        }
        assertEquals("",engine.translate("Do not translate this cancelled request",()->true));
        // Switching direction closes the earlier sessions; switching back must
        // recreate them and translate rather than reuse the other vocabulary.
        engine=OfflineTranslationEngine.get(context,false,progress::add);
        assertTrue(engine.translate("这是一只猫。",()->false).toLowerCase(Locale.ROOT).contains("cat"));
        OfflineTranslationEngine.prepare(context,progress::add);
        assertEquals(DownloadProgress.Stage.READY,progress.get(progress.size()-1).stage());
        var report=new android.os.Bundle();report.putLong("total_model_test_ms",System.currentTimeMillis()-start);getInstrumentation().sendStatus(0,report);
    }
}
