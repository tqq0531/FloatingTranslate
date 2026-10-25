package com.fuyi.keyboard;

import android.content.Context;
import android.content.res.Configuration;

/** An immutable palette resolved for the current preference and system appearance. */
final class KeyboardAppearance {
    final int bg, key, function, ink, muted, accent, onAccent, surface, stroke;
    final boolean dark;

    private KeyboardAppearance(boolean dark) {
        this.dark = dark;
        bg = dark ? 0xff17191F : 0xffECEEF3;
        key = dark ? 0xff30333D : 0xffFFFFFF;
        function = dark ? 0xff252832 : 0xffDDE1EB;
        ink = dark ? 0xffF1F2F7 : 0xff252934;
        muted = dark ? 0xffA7ADBD : 0xff737B8C;
        accent = dark ? 0xffAFB8FF : 0xff5968D9;
        onAccent = dark ? 0xff202846 : 0xffFFFFFF;
        surface = dark ? 0xff20232B : 0xffF8F9FC;
        stroke = dark ? 0xff3B404D : 0xffD6DBE5;
    }

    static KeyboardAppearance from(Context context) {
        String choice = Options.theme(context);
        boolean systemDark = (context.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        return new KeyboardAppearance(choice.equals("dark") || (choice.equals("system") && systemDark));
    }
}
