package com.fuyi.keyboard;
import org.junit.Test;
import java.util.List;
import static org.junit.Assert.*;
public class LearnedWordsTest {
    @Test public void chosenWordLeadsOnNextUseAndSurvivesReload(){
        var words=new LearnedWords();var base=List.of(new PinyinDictionary.Candidate("是",3),new PinyinDictionary.Candidate("时",3));
        words.remember("shi","时","");assertEquals("时",words.rank("shi",base).get(0).text());
        var restored=new LearnedWords();restored.load(words.snapshot());assertEquals("时",restored.rank("shi",base).get(0).text());
        assertEquals("是",restored.rank("s",base).get(0).text());
        restored.remember("shi","是","");assertEquals("是",restored.rank("shi",base).get(0).text());
        restored.clear();assertTrue(restored.snapshot().isEmpty());
    }
    @Test public void associatesSelectedWordsWithoutKeepingWholeDraft(){
        var words=new LearnedWords();words.remember("hao","好","你");assertEquals(List.of("好"),words.next("你"));
        words.remember("","们","你");assertEquals("们",words.next("你").get(0));
        words.remember("secret","abc123","");assertEquals(3,words.snapshot().size());
    }
}
