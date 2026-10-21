package com.fuyi.keyboard;
import android.content.Context;
import android.content.SharedPreferences;
final class Options {
    static SharedPreferences prefs(Context c) { return c.getSharedPreferences("options", Context.MODE_PRIVATE); }
    static boolean history(Context c) { return prefs(c).getBoolean("history", false); }
    static void history(Context c, boolean enabled) {
        prefs(c).edit().putBoolean("history", enabled).putLong("historySince", System.currentTimeMillis()).apply();
        if (!enabled) new ClipboardStore(c).clear();
    }
}
