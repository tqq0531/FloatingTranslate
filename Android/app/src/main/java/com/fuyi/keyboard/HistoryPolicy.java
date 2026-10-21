package com.fuyi.keyboard;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/** Pure retention rules, shared by the encrypted store and JVM tests. */
public final class HistoryPolicy {
    public static final long TTL = 24L * 60 * 60 * 1000;
    public static final int MAX_ITEMS = 100, MAX_CHARS = 10000;
    public record Entry(String id, String text, long copiedAt) { }
    public static boolean live(Entry e, long now) {
        return e.copiedAt() >= 0 && e.copiedAt() <= now && now - e.copiedAt() < TTL;
    }
    public static List<Entry> prune(List<Entry> entries, long now) {
        List<Entry> result = new ArrayList<>();
        for (Entry e : entries) if (live(e, now) && result.size() < MAX_ITEMS) result.add(e);
        return result;
    }
    public static List<Entry> add(List<Entry> entries, String text, long copiedAt, long now) {
        List<Entry> result = prune(entries, now);
        if (text == null || text.isBlank() || text.length() > MAX_CHARS || copiedAt < 0 || copiedAt > now || now - copiedAt >= TTL) return result;
        // A repeated copy does not extend the original record's lifetime.
        for (Entry e : result) if (e.text().equals(text)) return result;
        result.add(0, new Entry(UUID.randomUUID().toString(), text, copiedAt));
        return prune(result, now);
    }
}
