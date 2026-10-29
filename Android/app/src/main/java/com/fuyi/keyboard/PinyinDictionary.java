package com.fuyi.keyboard;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
import java.util.*;

/** Fixed local dictionary; no user learning or input logging. Not the native Rime runtime. */
public final class PinyinDictionary {
    public record Candidate(String text, int consumed) { }
    private record Word(String text, String code, double weight) { }
    private final Map<String, List<Word>> full = new HashMap<>(), initials = new HashMap<>(), prefixes = new HashMap<>(), continuations = new HashMap<>();
    private int maxCode;
    private boolean nativeEnabled;
    void useNative(boolean enabled){nativeEnabled=enabled;cachedRaw=null;}
    private String cachedRaw;
    private List<Candidate> cachedCandidates=List.of();
    private static final int CANDIDATE_LIMIT=100;
    public PinyinDictionary(Reader... inputs) throws IOException {
        for (Reader input : inputs) try (BufferedReader reader = new BufferedReader(input)) {
            boolean body = false;
            for (String line; (line = reader.readLine()) != null;) {
                if (!body) { if (line.equals("...")) body = true; continue; }
                String[] cells = line.split("\t");
                if (cells.length < 2 || cells[0].isEmpty()) continue;
                String[] syllables = cells[1].split(" +");
                String code = cells[1].replace(" ", ""), shortCode = "";
                for (String syllable : syllables) if (!syllable.isEmpty()) shortCode += syllable.charAt(0);
                if (!code.matches("[a-z]+")) continue;
                double weight = cells.length >= 3 ? Double.parseDouble(cells[2]) : 1;
                Word word = new Word(cells[0], code, weight);
                full.computeIfAbsent(code, k -> new ArrayList<>()).add(word);
                for(int n=1;n<Math.min(word.text().length(),5);n++)
                    continuations.computeIfAbsent(word.text().substring(0,n),k->new ArrayList<>()).add(word);
                initials.computeIfAbsent(shortCode, k -> new ArrayList<>()).add(word);
                // Prefixes complete unfinished single syllables: d/de, zh/zhong, sh/shen.
                if (syllables.length == 1) for (int n=1;n<code.length();n++)
                    prefixes.computeIfAbsent(code.substring(0,n), k -> new ArrayList<>()).add(word);
                // Each syllable can be full or initial: nihao, nh, nih, nhao.
                if (syllables.length > 1 && syllables.length <= 4)
                    indexMixed(word, syllables, 0, "", code, shortCode);
                maxCode = Math.max(maxCode, code.length());
            }
        }
        Comparator<Word> order = Comparator.comparingDouble(Word::weight).reversed();
        for (Map<String,List<Word>> index : List.of(full, initials, prefixes, continuations)) for (List<Word> words : index.values()) words.sort(order);
    }
    public List<String> following(String prefix){
        if(prefix.isEmpty())return List.of();
        return continuations.getOrDefault(prefix,List.of()).stream().map(w->w.text().substring(prefix.length())).distinct().limit(12).toList();
    }
    private void indexMixed(Word word, String[] syllables, int index, String code, String fullCode, String initialsCode) {
        if (index == syllables.length) {
            if (!code.equals(fullCode) && !code.equals(initialsCode))
                initials.computeIfAbsent(code, k -> new ArrayList<>()).add(word);
            return;
        }
        String syllable = syllables[index];
        indexMixed(word, syllables, index+1, code+syllable, fullCode, initialsCode);
        if (syllable.length()>1) indexMixed(word, syllables, index+1, code+syllable.charAt(0), fullCode, initialsCode);
    }
    private String clean(String raw) { return raw.toLowerCase(Locale.ROOT).replace("'", ""); }
    private int consumed(String raw, int letters) {
        int i = 0, n = 0;
        while (i < raw.length() && n < letters) if (raw.charAt(i++) != '\'') n++;
        while (i < raw.length() && raw.charAt(i) == '\'') i++;
        return i;
    }
    public String complete(String raw) {
        String code = clean(raw);
        if (code.isEmpty() || code.length() > 80) return "";
        List<Word> exact = full.get(code);
        if (exact != null) return exact.get(0).text();
        List<Word> abbr = initials.get(code);
        if (abbr != null) return abbr.get(0).text();
        double[] scores = new double[code.length()+1]; Arrays.fill(scores, Double.NEGATIVE_INFINITY); scores[0] = 0;
        String[] text = new String[code.length()+1]; text[0] = "";
        for (int end = 1; end <= code.length(); end++) for (int start = Math.max(0, end-maxCode); start < end; start++) {
            if (text[start] == null) continue;
            String syllable = code.substring(start,end);
            if (List.of("m", "n", "ng", "hm", "hng").contains(syllable)) continue;
            List<Word> words = full.get(syllable);
            if (words == null) continue;
            Word word = words.get(0);
            double score = scores[start] + Math.log1p(word.weight()) + 4*word.text().length() - 14;
            if (score > scores[end]) { scores[end] = score; text[end] = text[start] + word.text(); }
        }
        return text[code.length()] == null ? "" : text[code.length()];
    }
    /** Decode every input position, including mixed initials and unfinished syllables.
     * Exact sentence decoding remains preferred. Never silently discard unknown letters. */
    private String tolerantSentence(String code) {
        if (!code.matches("[a-z]+") || code.length()>80) return "";
        int size=code.length();
        double[] score=new double[size+1]; Arrays.fill(score,Double.NEGATIVE_INFINITY); score[0]=0;
        String[] text=new String[size+1]; text[0]="";
        for(int start=0;start<size;start++) {
            // Retain a genuinely unknown letter rather than dropping the rest of a draft.
            improve(score,text,start,start+1,String.valueOf(code.charAt(start)),-45);
            for(int end=start+1;end<=Math.min(size,start+maxCode);end++) {
                String part=code.substring(start,end);
                List<Word> words=full.get(part);
                if(words!=null) extend(score,text,start,end,words.get(0),0);
                words=initials.get(part);
                if(words!=null) extend(score,text,start,end,words.get(0),22);
                // Prefix completion is restricted to the last segment, not interior letters.
                if(end==size) {
                    words=prefixes.get(part);
                    if(words!=null) extend(score,text,start,end,words.get(0),14);
                }
                // Correct adjacent transposition within a syllable/word without changing
                // how many original keystrokes the resulting candidate consumes.
                if(part.length()>=3 && part.length()<=8 && !full.containsKey(part))
                    for(int i=0;i+1<part.length();i++) {
                        char[] swap=part.toCharArray();char c=swap[i];swap[i]=swap[i+1];swap[i+1]=c;
                        words=full.get(new String(swap));
                        if(words!=null) extend(score,text,start,end,words.get(0),6);
                    }
            }
        }
        return text[size];
    }
    private void extend(double[] scores,String[] text,int start,int end,Word word,double penalty) {
        improve(scores,text,start,end,word.text(),Math.log1p(word.weight())+4*word.text().length()-14-penalty);
    }
    private void improve(double[] scores,String[] text,int start,int end,String word,double delta) {
        if(text[start]!=null && scores[start]+delta>scores[end]) {
            scores[end]=scores[start]+delta;text[end]=text[start]+word;
        }
    }
    public List<Candidate> candidates(String raw) {
        if(raw.equals(cachedRaw))return cachedCandidates;
        String code = clean(raw); if (code.isEmpty()) return List.of();
        LinkedHashMap<String,Candidate> result = new LinkedHashMap<>();
        if(nativeEnabled){
            // Keep exact bundled/extension entries reachable before long native lists.
            var exact=full.get(code);if(exact!=null)for(var word:exact.subList(0,Math.min(exact.size(),5)))result.putIfAbsent(word.text(),new Candidate(word.text(),raw.length()));
            for(var item:NativePinyin.candidates(raw))result.putIfAbsent(item.text(),item);
        }
        String sentence = result.isEmpty()?complete(raw):"";
        if(sentence.isEmpty() && result.isEmpty() && code.length()>1) sentence=tolerantSentence(code);
        if (!sentence.isEmpty()) result.putIfAbsent(sentence,new Candidate(sentence,raw.length()));
        add(result,full.get(code),raw.length());
        add(result,prefixes.get(code),raw.length());
        add(result,initials.get(code),raw.length());
        if (!full.containsKey(code)) {
            // A common accidental adjacent-letter swap, preserving original keystroke consumption.
            for (int i=0; i+1<code.length(); i++) {
                char[] swapped = code.toCharArray(); char c=swapped[i]; swapped[i]=swapped[i+1]; swapped[i+1]=c;
                add(result,full.get(new String(swapped)),raw.length());
            }
        }
        for (int len=Math.min(maxCode,code.length()-1); len>0 && result.size()<CANDIDATE_LIMIT; len--) add(result,full.get(code.substring(0,len)),consumed(raw,len));
        cachedRaw=raw;cachedCandidates=List.copyOf(new ArrayList<>(result.values()).subList(0, Math.min(result.size(),CANDIDATE_LIMIT)));return cachedCandidates;
    }
    private void add(Map<String,Candidate> result,List<Word> words,int consumed) {
        if (words == null) return;
        for (Word word:words) { result.putIfAbsent(word.text(),new Candidate(word.text(),consumed)); if (result.size()>=CANDIDATE_LIMIT) break; }
    }
}
