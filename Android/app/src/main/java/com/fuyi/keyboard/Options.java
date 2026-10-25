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
    static String theme(Context c) {
        String value = prefs(c).getString("keyboardTheme", "system");
        return "light".equals(value) || "dark".equals(value) ? value : "system";
    }
    static void setTheme(Context c, String value) {
        prefs(c).edit().putString("keyboardTheme", "light".equals(value) || "dark".equals(value) ? value : "system").apply();
    }
    static int keyHeight(Context c) { return Math.max(44, Math.min(68, prefs(c).getInt("keyHeight", 54))); }
    static void setKeyHeight(Context c, int value) { prefs(c).edit().putInt("keyHeight", Math.max(44, Math.min(68, value))).apply(); }
    static boolean englishDefault(Context c) { return prefs(c).getBoolean("englishDefault", false); }
    static boolean chineseDefault(Context c) { return prefs(c).getBoolean("chineseDefault", false); }
}
