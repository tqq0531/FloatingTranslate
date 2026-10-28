package com.fuyi.keyboard;

import java.util.*;

/** Bounded local choices and next-word pairs, never a document or keystroke log. */
public final class LearnedWords {
    public record Entry(String key,String word,long order) {}
    private final LinkedHashMap<String,Entry> entries=new LinkedHashMap<>();
    private long clock;
    private static String code(String s){return s.toLowerCase(Locale.ROOT).replace("'","");}
    public void load(List<Entry> rows){entries.clear();clock=0;for(var e:rows){entries.put(e.key()+"\t"+e.word(),e);clock=Math.max(clock,e.order());}trim();}
    public List<Entry> snapshot(){return new ArrayList<>(entries.values());}
    public void clear(){entries.clear();}
    public void remember(String spelling,String word,String previous){
        if(!word.matches("[\\p{IsHan}]{1,40}"))return;
        if(spelling.matches("[a-zA-Z']{1,80}"))put("p:"+code(spelling),word);
        if(previous.matches("[\\p{IsHan}]{1,40}"))put("n:"+previous,word);
        trim();
    }
    private void put(String key,String word){String id=key+"\t"+word;entries.remove(id);entries.put(id,new Entry(key,word,++clock));}
    private void trim(){while(entries.size()>2000)entries.remove(entries.keySet().iterator().next());}
    public List<String> next(String previous){return matches("n:"+previous);}
    private List<String> matches(String key){return entries.values().stream().filter(e->e.key().equals(key)).sorted(Comparator.comparingLong(Entry::order).reversed()).limit(12).map(Entry::word).toList();}
    public List<PinyinDictionary.Candidate> rank(String spelling,List<PinyinDictionary.Candidate> base){
        var result=new ArrayList<PinyinDictionary.Candidate>();
        for(String word:matches("p:"+code(spelling)))result.add(new PinyinDictionary.Candidate(word,spelling.length()));
        for(var candidate:base)if(result.stream().noneMatch(c->c.text().equals(candidate.text())&&c.consumed()==candidate.consumed()))result.add(candidate);
        return result;
    }
}
