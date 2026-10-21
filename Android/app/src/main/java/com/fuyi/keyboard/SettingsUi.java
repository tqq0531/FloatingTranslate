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
    static final int BG=0xffF4F6FA, INK=0xff19283E, MUTED=0xff7B8799, ACCENT=0xff315FEA;
    static final int BORDER=0xffE9EDF3, PALE=0xffEDF2FF, NAVY=0xff182943, GREEN=0xff168A70;
    static int dp(Context c,float value){return Ui.dp(c,value);}
    static TextView text(Context c,String value,int size,int color,boolean bold){
        TextView v=Ui.text(c,value,size,color);v.setIncludeFontPadding(false);v.setLineSpacing(dp(c,3),1);
        if(bold)v.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));return v;
    }
    static GradientDrawable shape(Context c,int fill,int radius){var d=Ui.round(fill,radius,c);return d;}
    static LinearLayout card(Context c){
        var box=Ui.column(c);box.setPadding(dp(c,18),dp(c,18),dp(c,18),dp(c,18));
        var bg=shape(c,Color.WHITE,22);bg.setStroke(dp(c,1),BORDER);box.setBackground(bg);return box;
    }
    static LinearLayout.LayoutParams fill(Context c,int height){return new LinearLayout.LayoutParams(-1,height<0?height:dp(c,height));}
    static void gap(LinearLayout box,int height){box.addView(new View(box.getContext()),fill(box.getContext(),height));}
    static void divider(LinearLayout box){View line=new View(box.getContext());line.setBackgroundColor(BORDER);var lp=fill(box.getContext(),1);lp.topMargin=dp(box.getContext(),14);lp.bottomMargin=dp(box.getContext(),14);box.addView(line,lp);}
    static TextView chip(Context c,String text,int ink,int fill){
        TextView v=text(c,text,11,ink,true);v.setGravity(Gravity.CENTER);v.setPadding(dp(c,10),dp(c,5),dp(c,10),dp(c,5));v.setBackground(shape(c,fill,20));return v;
    }
    static Button button(Context c,String label,int fill,int ink,Runnable action){
        Button b=new Button(c);b.setText(label);b.setTextSize(14);b.setAllCaps(false);b.setIncludeFontPadding(false);b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        b.setMinWidth(0);b.setMinimumWidth(0);b.setMinHeight(0);b.setMinimumHeight(0);b.setPadding(dp(c,10),0,dp(c,10),0);b.setStateListAnimator(null);b.setElevation(0);
        int[][] states={{-android.R.attr.state_enabled},{}};
        b.setTextColor(new ColorStateList(states,new int[]{MUTED,ink}));var bg=shape(c,fill,13);bg.setColor(new ColorStateList(states,new int[]{0xffE9EDF4,fill}));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x18315FEA),bg,null));b.setOnClickListener(v->action.run());return b;
    }
    static TextView badge(Context c,String value,int ink,int fill){TextView v=text(c,value,18,ink,true);v.setGravity(Gravity.CENTER);v.setBackground(shape(c,fill,13));return v;}
    static LinearLayout heading(Context c,String icon,String title,String subtitle,int color,int tint){
        LinearLayout row=new LinearLayout(c);row.setGravity(Gravity.CENTER_VERTICAL);row.addView(badge(c,icon,color,tint),new LinearLayout.LayoutParams(dp(c,42),dp(c,42)));
        LinearLayout copy=Ui.column(c);copy.addView(text(c,title,16,INK,true));gap(copy,5);copy.addView(text(c,subtitle,12,MUTED,false));var lp=new LinearLayout.LayoutParams(0,-2,1);lp.leftMargin=dp(c,12);row.addView(copy,lp);return row;
    }
    static Switch toggle(Context c,String title,String subtitle,int id,LinearLayout card){
        LinearLayout row=new LinearLayout(c);row.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout copy=Ui.column(c);copy.addView(text(c,title,15,INK,true));gap(copy,6);copy.addView(text(c,subtitle,12,MUTED,false));row.addView(copy,new LinearLayout.LayoutParams(0,-2,1));
        Switch toggle=new Switch(c);toggle.setId(id);toggle.setContentDescription(title);toggle.setShowText(false);toggle.setSwitchMinWidth(dp(c,44));toggle.setSplitTrack(false);
        toggle.setThumbTintList(ColorStateList.valueOf(Color.WHITE));toggle.setTrackTintList(new ColorStateList(new int[][]{{android.R.attr.state_checked},{}},new int[]{ACCENT,0xffCED6E2}));
        var lp=new LinearLayout.LayoutParams(dp(c,56),dp(c,48));lp.leftMargin=dp(c,12);row.addView(toggle,lp);row.setOnClickListener(v->toggle.setChecked(!toggle.isChecked()));card.addView(row,fill(c,-2));return toggle;
    }
    static void privacyRow(LinearLayout card,String mark,String title,String detail){
        Context c=card.getContext();var row=new LinearLayout(c);row.setGravity(Gravity.TOP);
        row.addView(badge(c,mark,GREEN,0xffE9F6F1),new LinearLayout.LayoutParams(dp(c,30),dp(c,30)));
        var copy=Ui.column(c);copy.addView(text(c,title,13,INK,true));gap(copy,5);copy.addView(text(c,detail,12,MUTED,false));var lp=new LinearLayout.LayoutParams(0,-2,1);lp.leftMargin=dp(c,11);row.addView(copy,lp);card.addView(row);}
}
