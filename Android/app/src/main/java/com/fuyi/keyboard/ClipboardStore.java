package com.fuyi.keyboard;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.ArrayList;
import java.util.List;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** No plaintext clipboard file, Android backup, external storage or cloud sync. */
public final class ClipboardStore {
    private static final Object LOCK = new Object();
    private static long scheduledAt = -1;
    private static final String KEY = "fuyi.clipboard.v1";
    private final Context context;
    private final AtomicFile file;
    public ClipboardStore(Context c) {
        context = c.getApplicationContext();
        file = new AtomicFile(new File(context.getNoBackupFilesDir(), "clipboard.enc"));
    }
    private SecretKey key() throws Exception {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore"); ks.load(null);
        if (ks.containsAlias(KEY)) return (SecretKey) ks.getKey(KEY, null);
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(KEY, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build());
        return generator.generateKey();
    }
    private List<HistoryPolicy.Entry> read() throws Exception {
        List<HistoryPolicy.Entry> entries = new ArrayList<>();
        if (!file.getBaseFile().exists()) return entries;
        byte[] bytes = file.readFully();
        if (bytes.length < 30 || bytes.length > 8_000_000 || bytes[0] != 1) throw new IllegalStateException("Invalid history format");
        byte[] iv = java.util.Arrays.copyOfRange(bytes, 1, 13);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, iv));
        JSONArray json = new JSONArray(new String(cipher.doFinal(bytes, 13, bytes.length - 13), StandardCharsets.UTF_8));
        for (int i = 0; i < Math.min(json.length(), HistoryPolicy.MAX_ITEMS); i++) {
            JSONObject row = json.getJSONObject(i);
            entries.add(new HistoryPolicy.Entry(row.getString("id"), row.getString("text"), row.getLong("time")));
        }
        return entries;
    }
    private void write(List<HistoryPolicy.Entry> entries) throws Exception {
        if (entries.isEmpty()) { file.delete(); schedule(entries); return; }
        JSONArray json = new JSONArray();
        for (HistoryPolicy.Entry e : entries) json.put(new JSONObject().put("id", e.id()).put("text", e.text()).put("time", e.copiedAt()));
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding"); cipher.init(Cipher.ENCRYPT_MODE, key());
        byte[] encrypted = cipher.doFinal(json.toString().getBytes(StandardCharsets.UTF_8));
        byte[] bytes = ByteBuffer.allocate(1 + cipher.getIV().length + encrypted.length).put((byte)1).put(cipher.getIV()).put(encrypted).array();
        FileOutputStream out = null;
        try { out = file.startWrite(); out.write(bytes); file.finishWrite(out); }
        catch (Exception e) { if (out != null) file.failWrite(out); throw e; }
        schedule(entries);
    }
    public List<HistoryPolicy.Entry> list() {
        synchronized (LOCK) {
            try {
                long now = System.currentTimeMillis();
                long previous = Options.prefs(context).getLong("historyCheckedAt", 0);
                Options.prefs(context).edit().putLong("historyCheckedAt", now).apply();
                if (now + 1000 < previous) { file.delete(); schedule(List.of()); return List.of(); }
                List<HistoryPolicy.Entry> old = read(), live = HistoryPolicy.prune(old, now);
                if (old.size() != live.size()) write(live); else schedule(live);
                return live;
            } catch (Exception e) { file.delete(); schedule(List.of()); return List.of(); }
        }
    }
    public boolean add(String text, long copiedAt) {
        synchronized (LOCK) {
            try { write(HistoryPolicy.add(list(), text, copiedAt, System.currentTimeMillis())); return true; }
            catch (Exception e) { return false; }
        }
    }
    public void remove(String id) {
        synchronized (LOCK) {
            try { List<HistoryPolicy.Entry> rows = new ArrayList<>(list()); rows.removeIf(e -> e.id().equals(id)); write(rows); }
            catch (Exception e) { file.delete(); }
        }
    }
    public void clear() { synchronized (LOCK) { file.delete(); schedule(List.of()); } }
    public String resolve(String id) { for (HistoryPolicy.Entry e : list()) if (e.id().equals(id)) return e.text(); return null; }
    private void schedule(List<HistoryPolicy.Entry> rows) {
        AlarmManager alarms = context.getSystemService(AlarmManager.class);
        PendingIntent pending = PendingIntent.getBroadcast(context, 1, new Intent(context, ExpiryReceiver.class).setAction(ExpiryReceiver.ACTION_EXPIRE), PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        if (alarms == null) return;
        long next = Long.MAX_VALUE;
        for (HistoryPolicy.Entry e : rows) next = Math.min(next, e.copiedAt() + HistoryPolicy.TTL);
        if (next == scheduledAt) return;
        alarms.cancel(pending); scheduledAt = next;
        if (next != Long.MAX_VALUE) alarms.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, next, pending);
    }
}
