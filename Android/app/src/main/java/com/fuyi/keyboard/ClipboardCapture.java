package com.fuyi.keyboard;
import android.app.KeyguardManager;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.provider.Settings;

final class ClipboardCapture implements ClipboardManager.OnPrimaryClipChangedListener {
    private final Context context;
    private final ClipboardManager manager;
    private final ClipboardStore store;
    private final java.util.function.BooleanSupplier secure;
    private final Runnable changed;
    ClipboardCapture(Context c, java.util.function.BooleanSupplier secure, Runnable changed) {
        context = c; manager = c.getSystemService(ClipboardManager.class); store = new ClipboardStore(c);
        this.secure = secure; this.changed = changed;
        manager.addPrimaryClipChangedListener(this);
    }
    boolean defaultIme() {
        String id = Settings.Secure.getString(context.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
        return id != null && id.startsWith(context.getPackageName() + "/");
    }
    @Override public void onPrimaryClipChanged() { capture(); }
    void capture() {
        if (!Options.history(context) || !defaultIme()) return;
        KeyguardManager lock = context.getSystemService(KeyguardManager.class);
        try {
            ClipDescription description = manager.getPrimaryClipDescription();
            if (description == null) return;
            long stamp = description.getTimestamp();
            long seen = Options.prefs(context).getLong("lastSeenClip", 0);
            if (seen > System.currentTimeMillis()) seen = 0;
            if (stamp <= seen) return;
            // Remember skipped clips too, so a password copy is not imported later.
            Options.prefs(context).edit().putLong("lastSeenClip", stamp).apply();
            if (secure.getAsBoolean() || (lock != null && lock.isDeviceLocked()) || stamp < Options.prefs(context).getLong("historySince", Long.MAX_VALUE)) return;
            if (description.getExtras() != null && description.getExtras().getBoolean("android.content.extra.IS_SENSITIVE", false)) return;
            if (!description.hasMimeType("text/plain") && !description.hasMimeType("text/html")) return;
            ClipData data = manager.getPrimaryClip();
            if (data == null || data.getItemCount() == 0 || data.getItemAt(0).getText() == null) return;
            // getText only: never dereference URIs or trigger remote content providers.
            if (store.add(data.getItemAt(0).getText().toString(), stamp)) changed.run();
        } catch (SecurityException ignored) { /* Android may revoke clipboard access when focus changes. */ }
    }
    void close() { manager.removePrimaryClipChangedListener(this); }
}
