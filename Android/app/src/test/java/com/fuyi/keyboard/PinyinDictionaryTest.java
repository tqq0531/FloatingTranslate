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
    @Test public void unfinishedSyllablesAndMixedInitials() {
        assertEquals("的",engine.candidates("d").get(0).text());
        assertEquals("我",engine.candidates("w").get(0).text());
        assertTrue(engine.candidates("zh").stream().anyMatch(c->c.text().equals("中")&&c.consumed()==2));
        for(String input : new String[]{"nih","nhao","nh","nihao"})
            assertTrue(input,engine.candidates(input).stream().anyMatch(c->c.text().equals("你好")&&c.consumed()==input.length()));
        assertTrue(engine.candidates("ni'h").stream().anyMatch(c->c.text().equals("你好")&&c.consumed()==4));
        assertTrue(engine.candidates("123").isEmpty());
    }
    @Test public void optionalExtendedDictionaryAddsWords() throws Exception {
        var extended=new PinyinDictionary(new FileReader(System.getProperty("fuyi.dictionary")),new FileReader("src/main/assets/dictionary/ice-common.dict.yaml"));
        assertTrue(extended.candidates("wugeyi").stream().anyMatch(c->c.text().equals("五个亿")));
        assertEquals("的",extended.candidates("d").get(0).text());
    }
}
