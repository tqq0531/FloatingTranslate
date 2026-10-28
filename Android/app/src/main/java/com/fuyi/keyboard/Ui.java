package com.fuyi.keyboard;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
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
        KeyboardAppearance palette=KeyboardAppearance.from(c);
        Button b=new Button(c);b.setText(s);b.setTextSize(14);b.setTextColor(palette.ink);b.setAllCaps(false);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setPadding(dp(c,8),0,dp(c,8),0);
        b.setBackground(round(palette.surface,10,c));b.setOnClickListener(v -> action.run());return b;
    }
    static final int KEYBOARD=Color.rgb(232,240,239), FUNCTION=Color.rgb(207,224,222), GREEN=Color.rgb(22,119,116);
    // InputMethodService uses the platform Theme.InputMethod, not an AppCompat Activity.
    @android.annotation.SuppressLint("AppCompatCustomView")
    static Button key(Context c,String label,String description,int color,Runnable action,boolean repeat) {
        KeyboardAppearance palette=KeyboardAppearance.from(c);
        boolean accent=color==palette.accent;
        Button b=new Button(c) {
            boolean repeated;
            PopupWindow preview;
            private void hidePreview(){if(preview!=null){preview.dismiss();preview=null;}}
            private void showPreview(){
                if(!label.matches("[A-Za-z]") || !Options.prefs(c).getBoolean("keyPreview",true))return;
                TextView text=Ui.text(c,label,30,palette.ink);text.setGravity(Gravity.CENTER);text.setContentDescription("按键预览 "+label);
                text.setBackground(Ui.round(palette.key,9,c));
                preview=new PopupWindow(text,Math.max(getWidth()+dp(c,14),dp(c,46)),dp(c,64),false);
                preview.setTouchable(false);preview.setClippingEnabled(true);preview.setElevation(dp(c,5));
                int[] position=new int[2];getLocationInWindow(position);
                preview.showAtLocation(getRootView(),Gravity.TOP|Gravity.LEFT,Math.max(0,position[0]-dp(c,7)),Math.max(0,position[1]-dp(c,60)));
            }
            final Runnable tick=new Runnable(){public void run(){repeated=true;action.run();postDelayed(this,55);}};
            @Override public boolean onTouchEvent(MotionEvent event){
                int a=event.getActionMasked();
                if(a==MotionEvent.ACTION_DOWN){repeated=false;showPreview();if(Options.prefs(c).getBoolean("keyHaptic",true))performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);if(repeat)postDelayed(tick,380);}
                if(a==MotionEvent.ACTION_UP||a==MotionEvent.ACTION_CANCEL||(a==MotionEvent.ACTION_MOVE&&(event.getX()<0||event.getX()>getWidth()||event.getY()<0||event.getY()>getHeight())))removeCallbacks(tick);
                if(a==MotionEvent.ACTION_UP||a==MotionEvent.ACTION_CANCEL)hidePreview();
                if(a==MotionEvent.ACTION_CANCEL)repeated=false;
                return super.onTouchEvent(event);
            }
            @Override protected void onDraw(android.graphics.Canvas canvas){
                if(!label.equals("⇧")&&!label.equals("⌫")){super.onDraw(canvas);return;}
                float unit=dp(c,22),cx=getWidth()/2f,cy=getHeight()/2f;
                var paint=new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);paint.setColor(getCurrentTextColor());paint.setStyle(android.graphics.Paint.Style.STROKE);paint.setStrokeWidth(c.getResources().getDisplayMetrics().density*1.65f);paint.setStrokeJoin(android.graphics.Paint.Join.ROUND);paint.setStrokeCap(android.graphics.Paint.Cap.ROUND);
                var path=new android.graphics.Path();
                if(label.equals("⇧")){path.moveTo(cx-unit*.48f,cy);path.lineTo(cx,cy-unit*.48f);path.lineTo(cx+unit*.48f,cy);path.lineTo(cx+unit*.2f,cy);path.lineTo(cx+unit*.2f,cy+unit*.45f);path.lineTo(cx-unit*.2f,cy+unit*.45f);path.lineTo(cx-unit*.2f,cy);path.close();}
                else{path.moveTo(cx-unit*.53f,cy);path.lineTo(cx-unit*.2f,cy-unit*.33f);path.lineTo(cx+unit*.5f,cy-unit*.33f);path.lineTo(cx+unit*.5f,cy+unit*.33f);path.lineTo(cx-unit*.2f,cy+unit*.33f);path.close();canvas.drawLine(cx-unit*.03f,cy-unit*.14f,cx+unit*.26f,cy+unit*.14f,paint);canvas.drawLine(cx+unit*.26f,cy-unit*.14f,cx-unit*.03f,cy+unit*.14f,paint);}
                canvas.drawPath(path,paint);
            }
            @Override public boolean performClick(){if(repeated){repeated=false;return true;}return super.performClick();}
            @Override protected void onDetachedFromWindow(){hidePreview();removeCallbacks(tick);super.onDetachedFromWindow();}
            @Override protected void onWindowVisibilityChanged(int visibility){if(visibility!=VISIBLE){hidePreview();if(tick!=null)removeCallbacks(tick);}super.onWindowVisibilityChanged(visibility);}
        };
        boolean character=label.codePointCount(0,label.length())==1;
        boolean space=label.equals("空格");
        b.setText(label);b.setContentDescription(description);b.setTextSize(character?22:14);
        b.setTypeface(android.graphics.Typeface.create(character||space?"sans-serif":"sans-serif-medium",android.graphics.Typeface.NORMAL));
        b.setTextColor(accent?palette.onAccent:space?palette.muted:palette.ink);b.setAllCaps(false);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setMinHeight(0);b.setMinimumHeight(0);b.setPadding(0,0,0,0);b.setIncludeFontPadding(false);
        b.setGravity(Gravity.CENTER);b.setLetterSpacing(0);b.setStateListAnimator(null);b.setElevation(0);
        // Draw a subtle bottom edge rather than a large platform button shadow.
        var shadow=round(palette.dark?Color.argb(95,0,0,0):Color.argb(50,56,64,76),7,c);
        var shape=round(color,7,c);shape.setStroke(Math.max(1,dp(c,0.4f)),accent?color:palette.stroke);
        var face=new RippleDrawable(ColorStateList.valueOf(palette.dark?0x28FFFFFF:0x18000000),shape,round(Color.WHITE,7,c));
        var keyFace=new LayerDrawable(new android.graphics.drawable.Drawable[]{shadow,face});
        keyFace.setLayerInset(0,0,dp(c,1),0,0);
        keyFace.setLayerInset(1,0,0,0,dp(c,1));
        b.setBackground(new InsetDrawable(keyFace,dp(c,2),dp(c,3),dp(c,2),dp(c,3)));
        b.setOnClickListener(v->action.run());return b;
    }
    static Button tool(Context c,String label,String description,Runnable action){
        KeyboardAppearance palette=KeyboardAppearance.from(c);
        Button b=button(c,label,action);b.setContentDescription(description);b.setTextSize(label.length()>2?13:16);b.setTypeface(android.graphics.Typeface.create("sans-serif-medium",android.graphics.Typeface.NORMAL));b.setStateListAnimator(null);b.setElevation(0);
        b.setMinHeight(0);b.setMinimumHeight(0);b.setIncludeFontPadding(false);b.setGravity(Gravity.CENTER);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(palette.dark?0x20FFFFFF:0x12000000),round(Color.TRANSPARENT,7,c),round(Color.WHITE,7,c)));return b;
    }
    static Button icon(Context c,String kind,String description,Runnable action){
        var palette=KeyboardAppearance.from(c);
        Button button=new Button(c){
            @Override protected void onDraw(android.graphics.Canvas canvas){
                float unit=dp(c,22),x=getWidth()/2f,y=getHeight()/2f;
                var p=new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);p.setColor(palette.ink);p.setStyle(android.graphics.Paint.Style.STROKE);p.setStrokeWidth(dp(c,1.6f));p.setStrokeCap(android.graphics.Paint.Cap.ROUND);p.setStrokeJoin(android.graphics.Paint.Join.ROUND);
                if(kind.equals("down")||kind.equals("back")){
                    var path=new android.graphics.Path();
                    if(kind.equals("down")){path.moveTo(x-unit*.32f,y-unit*.15f);path.lineTo(x,y+unit*.16f);path.lineTo(x+unit*.32f,y-unit*.15f);}
                    else {path.moveTo(x+unit*.15f,y-unit*.32f);path.lineTo(x-unit*.16f,y);path.lineTo(x+unit*.15f,y+unit*.32f);}canvas.drawPath(path,p);
                }else if(kind.equals("clipboard")){
                    canvas.drawRoundRect(x-unit*.3f,y-unit*.36f,x+unit*.3f,y+unit*.4f,dp(c,2),dp(c,2),p);
                    canvas.drawRoundRect(x-unit*.15f,y-unit*.44f,x+unit*.15f,y-unit*.25f,dp(c,2),dp(c,2),p);
                    for(int n=0;n<3;n++)canvas.drawLine(x-unit*.15f,y+unit*(-.06f+n*.14f),x+unit*.15f,y+unit*(-.06f+n*.14f),p);
                }else if(kind.equals("settings")){
                    for(int n=0;n<3;n++){float yy=y+unit*(n-1)*.3f;canvas.drawLine(x-unit*.36f,yy,x+unit*.36f,yy,p);canvas.drawCircle(x+unit*(n==1?.15f:-.15f),yy,unit*.08f,p);}
                }else{p.setStyle(android.graphics.Paint.Style.FILL);p.setTextSize(dp(c,21));p.setTextAlign(android.graphics.Paint.Align.CENTER);canvas.drawText("译",x,y-(p.ascent()+p.descent())/2,p);}
            }
        };
        button.setContentDescription(description);button.setMinWidth(0);button.setMinimumWidth(0);button.setMinHeight(0);button.setMinimumHeight(0);button.setPadding(0,0,0,0);button.setStateListAnimator(null);
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(palette.dark?0x30FFFFFF:0x18000000),round(palette.key,24,c),round(Color.WHITE,24,c)));
        button.setOnClickListener(v->action.run());return button;
    }
    static LinearLayout column(Context c) { LinearLayout v=new LinearLayout(c);v.setOrientation(LinearLayout.VERTICAL);return v; }
}
