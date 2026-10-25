package com.fuyi.keyboard;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.view.Gravity;
import android.view.View;
import android.widget.*;

/** Settings-only styling; keyboard touch geometry and theme stay independent. */
final class SettingsUi {
    final int BG, INK, MUTED, ACCENT, BORDER, PALE, SURFACE, ON_ACCENT, GREEN, SUCCESS, DANGER, DANGER_BG;
    final boolean dark;
    SettingsUi(Context context) {
        KeyboardAppearance p = KeyboardAppearance.from(context);
        dark=p.dark; BG=p.bg; INK=p.ink; MUTED=p.muted; ACCENT=p.accent; BORDER=p.stroke;
        PALE=p.function; SURFACE=p.key; ON_ACCENT=p.onAccent;
        GREEN=dark?0xff9DD4B8:0xff26795B; SUCCESS=dark?0xff243B32:0xffEAF4EF;
        DANGER=dark?0xffEAB6B6:0xffA14C55; DANGER_BG=dark?0xff3F2C32:0xffF8EDEF;
    }
    static int dp(Context c,float value){return Ui.dp(c,value);}
    static TextView text(Context c,String value,int size,int color,boolean bold){
        TextView v=Ui.text(c,value,size,color);v.setIncludeFontPadding(false);v.setLineSpacing(dp(c,3),1);
        if(bold)v.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));return v;
    }
    static GradientDrawable shape(Context c,int fill,int radius){var d=Ui.round(fill,radius,c);return d;}
    static LinearLayout card(Context c){
        var box=Ui.column(c);box.setPadding(dp(c,18),dp(c,18),dp(c,18),dp(c,18));
        var p=new SettingsUi(c);var bg=shape(c,p.SURFACE,18);bg.setStroke(dp(c,0.5f),p.BORDER);box.setBackground(bg);return box;
    }
    static LinearLayout.LayoutParams fill(Context c,int height){return new LinearLayout.LayoutParams(-1,height<0?height:dp(c,height));}
    static void gap(LinearLayout box,int height){box.addView(new View(box.getContext()),fill(box.getContext(),height));}
    static void divider(LinearLayout box){View line=new View(box.getContext());line.setBackgroundColor(new SettingsUi(box.getContext()).BORDER);var lp=fill(box.getContext(),1);lp.topMargin=dp(box.getContext(),14);lp.bottomMargin=dp(box.getContext(),14);box.addView(line,lp);}
    static TextView chip(Context c,String text,int ink,int fill){
        TextView v=text(c,text,11,ink,true);v.setGravity(Gravity.CENTER);v.setPadding(dp(c,10),dp(c,5),dp(c,10),dp(c,5));v.setBackground(shape(c,fill,20));return v;
    }
    static Button button(Context c,String label,int fill,int ink,Runnable action){
        Button b=new Button(c);b.setText(label);b.setTextSize(14);b.setAllCaps(false);b.setIncludeFontPadding(false);b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        b.setMinWidth(0);b.setMinimumWidth(0);b.setMinHeight(0);b.setMinimumHeight(0);b.setPadding(dp(c,10),0,dp(c,10),0);b.setStateListAnimator(null);b.setElevation(0);
        var p=new SettingsUi(c);int[][] states={{-android.R.attr.state_enabled},{}};
        b.setTextColor(new ColorStateList(states,new int[]{p.MUTED,ink}));var bg=shape(c,fill,13);bg.setColor(new ColorStateList(states,new int[]{p.PALE,fill}));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf((p.ACCENT & 0x00FFFFFF) | 0x20000000),bg,null));b.setOnClickListener(v->action.run());return b;
    }
    static TextView badge(Context c,String value,int ink,int fill){TextView v=text(c,value,18,ink,true);v.setGravity(Gravity.CENTER);v.setBackground(shape(c,fill,13));return v;}
    static LinearLayout heading(Context c,String icon,String title,String subtitle,int color,int tint){
        var p=new SettingsUi(c);LinearLayout row=new LinearLayout(c);row.setGravity(Gravity.CENTER_VERTICAL);row.addView(badge(c,icon,color,tint),new LinearLayout.LayoutParams(dp(c,42),dp(c,42)));
        LinearLayout copy=Ui.column(c);copy.addView(text(c,title,16,p.INK,true));gap(copy,5);copy.addView(text(c,subtitle,12,p.MUTED,false));var lp=new LinearLayout.LayoutParams(0,-2,1);lp.leftMargin=dp(c,12);row.addView(copy,lp);return row;
    }
    static Switch toggle(Context c,String title,String subtitle,int id,LinearLayout card){
        var p=new SettingsUi(c);LinearLayout row=new LinearLayout(c);row.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout copy=Ui.column(c);copy.addView(text(c,title,15,p.INK,true));gap(copy,6);copy.addView(text(c,subtitle,12,p.MUTED,false));row.addView(copy,new LinearLayout.LayoutParams(0,-2,1));
        Switch toggle=new Switch(c);toggle.setId(id);toggle.setContentDescription(title);toggle.setShowText(false);toggle.setSwitchMinWidth(dp(c,44));toggle.setSplitTrack(false);
        toggle.setThumbTintList(ColorStateList.valueOf(p.dark?0xffE9ECF7:Color.WHITE));toggle.setTrackTintList(new ColorStateList(new int[][]{{android.R.attr.state_checked},{}},new int[]{p.ACCENT,p.PALE}));
        var lp=new LinearLayout.LayoutParams(dp(c,56),dp(c,48));lp.leftMargin=dp(c,12);row.addView(toggle,lp);row.setOnClickListener(v->toggle.setChecked(!toggle.isChecked()));card.addView(row,fill(c,-2));return toggle;
    }
    static void privacyRow(LinearLayout card,String mark,String title,String detail){
        Context c=card.getContext();var p=new SettingsUi(c);var row=new LinearLayout(c);row.setGravity(Gravity.TOP);
        row.addView(badge(c,mark,p.GREEN,p.SUCCESS),new LinearLayout.LayoutParams(dp(c,30),dp(c,30)));
        var copy=Ui.column(c);copy.addView(text(c,title,13,p.INK,true));gap(copy,5);copy.addView(text(c,detail,12,p.MUTED,false));var lp=new LinearLayout.LayoutParams(0,-2,1);lp.leftMargin=dp(c,11);row.addView(copy,lp);card.addView(row);}
}
