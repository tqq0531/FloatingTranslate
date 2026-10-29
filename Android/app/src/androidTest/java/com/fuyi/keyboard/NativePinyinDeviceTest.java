package com.fuyi.keyboard;
import android.test.InstrumentationTestCase;
public final class NativePinyinDeviceTest extends InstrumentationTestCase {
 private void delete(java.io.File file){java.io.File[] children=file.listFiles();if(children!=null)for(var child:children)delete(child);file.delete();}
 public void testRealRimeEngine()throws Exception {
  delete(new java.io.File(getInstrumentation().getTargetContext().getNoBackupFilesDir(),"rime-shared-v1"));
  assertTrue("Rime must initialize, not silently fall back",NativePinyin.prepare(getInstrumentation().getTargetContext()));
  assertFalse(NativePinyin.engineVersion().equals("unavailable"));
  for(String raw:new String[]{"nihao","nh","d","wojintianqubeijing","nihaojintianzhogn"}) {
   var rows=NativePinyin.candidates(raw);assertFalse(raw,rows.isEmpty());
   var status=new android.os.Bundle();status.putString("rime",raw+" -> "+rows.get(0).text()+" ("+NativePinyin.engineVersion()+")");getInstrumentation().sendStatus(0,status);
  }
  assertTrue(NativePinyin.candidates("nihao").stream().anyMatch(c->c.text().equals("你好")&&c.consumed()==5));
 }
}
