package com.fuyi.keyboard;
import org.junit.Test;
import org.junit.BeforeClass;
import java.io.FileReader;
import static org.junit.Assert.*;
public class PinyinDictionaryTest {
    static PinyinDictionary engine;
    @BeforeClass public static void load() throws Exception {engine=new PinyinDictionary(new FileReader(System.getProperty("fuyi.dictionary")));}
    @Test public void fullPinyinAndInitials() {
        assertEquals("你好",engine.candidates("nihao").get(0).text());
        assertTrue(engine.candidates("nh").stream().anyMatch(c->c.text().equals("你好")));
        assertTrue(engine.candidates("nhm").stream().anyMatch(c->c.text().equals("你好吗")));
    }
    @Test public void completeSentenceAndPartialCandidateConsume() {
        assertEquals("我今天去北京",engine.complete("wojintianqubeijing"));
        assertEquals("为什么不一起翻译",engine.complete("weishenmebuyiqifanyi"));
        assertTrue(engine.candidates("wojintianx").stream().anyMatch(c->c.text().equals("我")&&c.consumed()==2));
        assertEquals("",engine.complete("zzzzzz"));
    }
    @Test public void swappedLettersConsumeOriginalInput() {assertTrue(engine.candidates("zhogn").stream().anyMatch(c->c.text().equals("中")&&c.consumed()==5));}
}
