package com.fuyi.keyboard;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.widget.*;
final class Ui {
    static final int INK=Color.rgb(24,34,53), MUTED=Color.rgb(103,114,135), BLUE=Color.rgb(41,98,255), BG=Color.rgb(244,246,251);
    static int dp(Context c,float n) { return Math.round(n*c.getResources().getDisplayMetrics().density); }
    static GradientDrawable round(int color,int radius,Context c) { GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(c,radius));return d; }
    static TextView text(Context c,String s,int size,int color) { TextView v=new TextView(c);v.setText(s);v.setTextSize(size);v.setTextColor(color);return v; }
    static Button button(Context c,String s,Runnable action) {
        Button b=new Button(c);b.setText(s);b.setTextSize(14);b.setTextColor(INK);b.setAllCaps(false);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setPadding(dp(c,8),0,dp(c,8),0);
        b.setBackground(round(Color.WHITE,10,c));b.setOnClickListener(v -> action.run());return b;
    }
    static LinearLayout column(Context c) { LinearLayout v=new LinearLayout(c);v.setOrientation(LinearLayout.VERTICAL);return v; }
}
