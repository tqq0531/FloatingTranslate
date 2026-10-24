package com.fuyi.keyboard;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.RippleDrawable;
import android.content.res.ColorStateList;
import android.view.MotionEvent;
import android.view.HapticFeedbackConstants;
import android.view.Gravity;
import android.widget.*;
final class Ui {
    static final int INK=Color.rgb(24,34,53), MUTED=Color.rgb(103,114,135), BLUE=Color.rgb(22,119,116), BG=Color.rgb(244,246,251);
    static int dp(Context c,float n) { return Math.round(n*c.getResources().getDisplayMetrics().density); }
    static GradientDrawable round(int color,int radius,Context c) { GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(c,radius));return d; }
    static TextView text(Context c,String s,int size,int color) { TextView v=new TextView(c);v.setText(s);v.setTextSize(size);v.setTextColor(color);return v; }
    static Button button(Context c,String s,Runnable action) {
        Button b=new Button(c);b.setText(s);b.setTextSize(14);b.setTextColor(INK);b.setAllCaps(false);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setPadding(dp(c,8),0,dp(c,8),0);
        b.setBackground(round(Color.WHITE,10,c));b.setOnClickListener(v -> action.run());return b;
    }
    static final int KEYBOARD=Color.rgb(232,240,239), FUNCTION=Color.rgb(207,224,222), GREEN=Color.rgb(22,119,116);
    // InputMethodService uses the platform Theme.InputMethod, not an AppCompat Activity.
    @android.annotation.SuppressLint("AppCompatCustomView")
    static Button key(Context c,String label,String description,int color,Runnable action,boolean repeat) {
        Button b=new Button(c) {
            boolean repeated;
            final Runnable tick=new Runnable(){public void run(){repeated=true;action.run();postDelayed(this,55);}};
            @Override public boolean onTouchEvent(MotionEvent event){
                int a=event.getActionMasked();
                if(a==MotionEvent.ACTION_DOWN){repeated=false;performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);if(repeat)postDelayed(tick,380);}
                if(a==MotionEvent.ACTION_UP||a==MotionEvent.ACTION_CANCEL||(a==MotionEvent.ACTION_MOVE&&(event.getX()<0||event.getX()>getWidth()||event.getY()<0||event.getY()>getHeight())))removeCallbacks(tick);
                if(a==MotionEvent.ACTION_CANCEL)repeated=false;
                return super.onTouchEvent(event);
            }
            @Override protected void onDraw(android.graphics.Canvas canvas){
                if(!label.equals("⇧")&&!label.equals("⌫")){super.onDraw(canvas);return;}
                float unit=dp(c,24),cx=getWidth()/2f,cy=getHeight()/2f;
                var paint=new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);paint.setColor(INK);paint.setStyle(android.graphics.Paint.Style.STROKE);paint.setStrokeWidth(dp(c,1.8f));paint.setStrokeJoin(android.graphics.Paint.Join.ROUND);
                var path=new android.graphics.Path();
                if(label.equals("⇧")){path.moveTo(cx-unit*.48f,cy);path.lineTo(cx,cy-unit*.48f);path.lineTo(cx+unit*.48f,cy);path.lineTo(cx+unit*.2f,cy);path.lineTo(cx+unit*.2f,cy+unit*.45f);path.lineTo(cx-unit*.2f,cy+unit*.45f);path.lineTo(cx-unit*.2f,cy);path.close();}
                else{path.moveTo(cx-unit*.53f,cy);path.lineTo(cx-unit*.2f,cy-unit*.33f);path.lineTo(cx+unit*.5f,cy-unit*.33f);path.lineTo(cx+unit*.5f,cy+unit*.33f);path.lineTo(cx-unit*.2f,cy+unit*.33f);path.close();canvas.drawLine(cx-unit*.03f,cy-unit*.14f,cx+unit*.26f,cy+unit*.14f,paint);canvas.drawLine(cx+unit*.26f,cy-unit*.14f,cx-unit*.03f,cy+unit*.14f,paint);}
                canvas.drawPath(path,paint);
            }
            @Override public boolean performClick(){if(repeated){repeated=false;return true;}return super.performClick();}
            @Override protected void onDetachedFromWindow(){removeCallbacks(tick);super.onDetachedFromWindow();}
            @Override protected void onWindowVisibilityChanged(int visibility){if(visibility!=VISIBLE&&tick!=null)removeCallbacks(tick);super.onWindowVisibilityChanged(visibility);}
        };
        b.setText(label);b.setContentDescription(description);b.setTextSize(21);b.setTypeface(android.graphics.Typeface.create("sans-serif-medium",0));b.setTextColor(color==GREEN?Color.WHITE:Color.rgb(32,56,57));b.setAllCaps(false);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setMinHeight(0);b.setMinimumHeight(0);b.setPadding(0,0,0,0);b.setIncludeFontPadding(false);
        b.setStateListAnimator(null);b.setElevation(0);
        var shape=round(color,10,c);shape.setStroke(dp(c,0.5f),color==GREEN?GREEN:Color.rgb(208,224,222));
        var ripple=new RippleDrawable(ColorStateList.valueOf(Color.argb(45,22,119,116)),shape,null);
        b.setBackground(new InsetDrawable(ripple,dp(c,3),dp(c,4),dp(c,3),dp(c,4)));
        b.setOnClickListener(v->action.run());return b;
    }
    static Button tool(Context c,String label,String description,Runnable action){
        Button b=button(c,label,action);b.setContentDescription(description);b.setTextSize(17);b.setStateListAnimator(null);b.setElevation(0);
        b.setMinHeight(0);b.setMinimumHeight(0);b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x22192330),round(Color.rgb(244,249,248),12,c),null));return b;
    }
    static LinearLayout column(Context c) { LinearLayout v=new LinearLayout(c);v.setOrientation(LinearLayout.VERTICAL);return v; }
}
