package com.fuyi.keyboard;
import org.junit.Test;
import static org.junit.Assert.*;
import java.util.List;
public class HistoryPolicyTest {
    private final long now=1_800_000_000_000L;
    @Test public void expiresAtExactly24Hours() {
        var row=new HistoryPolicy.Entry("a","旧记录",now);
        assertTrue(HistoryPolicy.live(row,now+HistoryPolicy.TTL-1));
        assertFalse(HistoryPolicy.live(row,now+HistoryPolicy.TTL));
    }
    @Test public void repeatedCopyDoesNotExtendRetention() {
        var rows=HistoryPolicy.add(List.of(),"你好",now,now);
        var twice=HistoryPolicy.add(rows,"你好",now+1000,now+1000);
        assertEquals(1,twice.size());assertEquals(now,twice.get(0).copiedAt());
        assertTrue(HistoryPolicy.prune(twice,now+HistoryPolicy.TTL).isEmpty());
    }
    @Test public void rejectsExpiredFutureEmptyAndOversizedEntries() {
        assertTrue(HistoryPolicy.add(List.of(),"old",now-HistoryPolicy.TTL,now).isEmpty());
        assertTrue(HistoryPolicy.add(List.of(),"future",now+1,now).isEmpty());
        assertTrue(HistoryPolicy.add(List.of()," ",now,now).isEmpty());
        assertTrue(HistoryPolicy.add(List.of(),"x".repeat(10001),now,now).isEmpty());
        assertFalse(HistoryPolicy.live(new HistoryPolicy.Entry("a","x",now+100),now));
    }
    @Test public void storesNewest100WithoutPinningBeyondExpiry() {
        List<HistoryPolicy.Entry> rows=List.of();
        for(int i=0;i<105;i++)rows=HistoryPolicy.add(rows,"row-"+i,now+i,now+i);
        assertEquals(100,rows.size());assertEquals("row-104",rows.get(0).text());
        assertTrue(HistoryPolicy.prune(rows,now+104+HistoryPolicy.TTL).isEmpty());
    }
}
