package com.fuyi.keyboard;

import android.test.InstrumentationTestCase;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Explicit opt-in network check: run separately with a Wi-Fi-connected test device. */
public final class TranslationModelDeviceTest extends InstrumentationTestCase {
    public void testExplicitModelDownloadAndLocalTranslation() throws Exception {
        var arguments=((android.test.InstrumentationTestRunner)getInstrumentation()).getArguments();
        boolean prepare="true".equals(arguments.getString("downloadModels"));
        if (!prepare&&!"true".equals(arguments.getString("preparedModel"))) return;
        LocalTranslator[] translator={null};String[] status={""},translated={""};
        CountDownLatch downloaded=new CountDownLatch(1),done=new CountDownLatch(1);
        getInstrumentation().runOnMainSync(()->{translator[0]=new LocalTranslator();if(prepare)translator[0].download((text,message)->{status[0]=message;downloaded.countDown();});});
        try {
            if(prepare){assertTrue("Model download timed out",downloaded.await(600,TimeUnit.SECONDS));assertTrue(status[0],status[0].contains("已就绪"));}
            getInstrumentation().runOnMainSync(()->translator[0].translate("你好，今天我去北京。",(text,message)->{translated[0]=text;status[0]=message;done.countDown();}));
            assertTrue("Translation timed out",done.await(30,TimeUnit.SECONDS));
            assertFalse("Translation missing: "+status[0],translated[0].isEmpty());
            assertTrue("Expected English sentence: "+translated[0],translated[0].matches("(?s).*[A-Za-z].*"));
            android.os.Bundle result=new android.os.Bundle();result.putString("translation",translated[0]);getInstrumentation().sendStatus(0,result);
        } finally {getInstrumentation().runOnMainSync(()->translator[0].close());}
    }
}
