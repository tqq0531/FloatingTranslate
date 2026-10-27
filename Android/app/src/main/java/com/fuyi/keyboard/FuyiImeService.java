package com.fuyi.keyboard;

import android.content.*;
import android.content.res.Configuration;
import android.graphics.Color;
import android.inputmethodservice.InputMethodService;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.*;
import android.view.inputmethod.*;
import android.widget.*;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.Executors;

public final class FuyiImeService extends InputMethodService {
    private final Handler handler=new Handler(Looper.getMainLooper());
    private PinyinDictionary dictionary;
    private LocalTranslator translator;
    private ClipboardStore history;
    private ClipboardCapture capture;
    private final android.content.SharedPreferences.OnSharedPreferenceChangeListener optionsChanged=(prefs,key) -> {
        if (this.root==null) return;
        if ("extendedDictionary".equals(key)) loadDictionary();
        else if ("history".equals(key)) renderHistory();
        else if ("keyboardTheme".equals(key)||"keyHeight".equals(key)) handler.post(this::rebuildKeyboard);
    };
    private LinearLayout root, candidateRow, keys, historyRows, toolbar, translationRow, appearanceRows;
    private HorizontalScrollView candidateScroll;
    private TextView englishView, compositionView;
    private Button modeButton, historyButton, directionButton, useTranslationButton;
    private boolean appearanceOpen=false;
    private KeyboardAppearance palette;
    private boolean privateField=true, latin=false, symbols=false, shifted=false, historyOpen=false;
    private boolean active=false, moreSymbols=false;
    private String raw="", sentence="", english="", translatedSource="";
    private List<PinyinDictionary.Candidate> candidates=List.of();
    private int generation=0, lastSelection=-1, expectedCaret=-1, compositionStart=-1;
    private long touched=0, historyDeadline=Long.MAX_VALUE;
    private Runnable translationWork;
    private final Runnable expiryTick=new Runnable() { public void run() {
        if (historyOpen && System.currentTimeMillis()>=historyDeadline) renderHistory();
        if (!sentence.isEmpty() && expired()) clearSentence();
        if (active) handler.postDelayed(this,1000);
    }};
    @Override public void onCreate() {
        super.onCreate(); history=new ClipboardStore(this);
        Options.prefs(this).registerOnSharedPreferenceChangeListener(optionsChanged);
        capture=new ClipboardCapture(this,() -> privateField,() -> { if(historyOpen) renderHistory(); });
        loadDictionary();
    }
    private int dictionaryGeneration;
    private void loadDictionary() {
        int request=++dictionaryGeneration;
        boolean extended=Options.extendedDictionary(this);
        var executor=Executors.newSingleThreadExecutor();
        executor.execute(() -> {
            try {
                var basic=new InputStreamReader(getAssets().open("pinyin_simp.dict.yaml"),StandardCharsets.UTF_8);
                PinyinDictionary loaded;
                if(extended) {
                    try(basic;var extra=new InputStreamReader(getAssets().open("dictionary/ice-common.dict.yaml"),StandardCharsets.UTF_8)) {
                        loaded=new PinyinDictionary(basic,extra);
                    }
                } else loaded=new PinyinDictionary(basic);
                handler.post(() -> {if(request==dictionaryGeneration){dictionary=loaded;if(root!=null)refresh();}});
            } catch(Exception failure) {handler.post(()->{if(request==dictionaryGeneration&&englishView!=null)englishView.setText("词库加载失败，请切换基础词库重试");});}
            finally {executor.shutdown();}
        });
    }
    @Override public boolean onEvaluateFullscreenMode() { return false; }
    @Override public View onCreateInputView() {
        palette=KeyboardAppearance.from(this);
        root=Ui.column(this);root.setBackgroundColor(palette.bg);
        root.setPadding(Ui.dp(this,3),0,Ui.dp(this,3),Ui.dp(this,4));
        root.setOnApplyWindowInsetsListener((v,insets)->{
            int bottom=android.os.Build.VERSION.SDK_INT>=30?insets.getInsets(WindowInsets.Type.navigationBars()).bottom:insets.getSystemWindowInsetBottom();
            v.setPadding(Ui.dp(this,3),0,Ui.dp(this,3),bottom+Ui.dp(this,4));return insets;
        });
        var window=getWindow().getWindow();
        if(window!=null){window.setNavigationBarColor(palette.bg);if(android.os.Build.VERSION.SDK_INT>=30 && window.getInsetsController()!=null)window.getInsetsController().setSystemBarsAppearance(palette.dark?0:WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS,WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS);}
        toolbar=new LinearLayout(this);toolbar.setGravity(Gravity.CENTER_VERTICAL);toolbar.setPadding(Ui.dp(this,7),0,Ui.dp(this,7),0);
        historyButton=Ui.tool(this,"历史","剪贴板",()->{if(!privateField){historyOpen=true;appearanceOpen=false;renderHistory();renderAppearance();}});
        toolbar.addView(historyButton,new LinearLayout.LayoutParams(Ui.dp(this,52),Ui.dp(this,40)));
        directionButton=Ui.tool(this,"中 → 英","切换翻译方向",this::toggleMode);directionButton.setTextColor(palette.accent);directionButton.setTextSize(14);
        toolbar.addView(directionButton,new LinearLayout.LayoutParams(0,Ui.dp(this,40),1));
        Button appearance=Ui.tool(this,"调节","键盘高度与主题",()->{appearanceOpen=true;historyOpen=false;renderHistory();renderAppearance();});
        toolbar.addView(appearance,new LinearLayout.LayoutParams(Ui.dp(this,52),Ui.dp(this,40)));
        Button hide=Ui.tool(this,"⌄","收起键盘",()->requestHideSelf(0));toolbar.addView(hide,new LinearLayout.LayoutParams(Ui.dp(this,36),Ui.dp(this,40)));
        root.addView(toolbar,new LinearLayout.LayoutParams(-1,Ui.dp(this,56)));
        translationRow=new LinearLayout(this);translationRow.setGravity(Gravity.CENTER_VERTICAL);translationRow.setBackground(Ui.round(palette.surface,10,this));translationRow.setPadding(Ui.dp(this,10),0,Ui.dp(this,4),0);
        TextView badge=Ui.text(this,"译",13,palette.accent);badge.setGravity(Gravity.CENTER);translationRow.addView(badge,new LinearLayout.LayoutParams(Ui.dp(this,24),-1));
        englishView=Ui.text(this,"",15,palette.ink);englishView.setPadding(Ui.dp(this,6),Ui.dp(this,6),Ui.dp(this,6),Ui.dp(this,6));englishView.setMaxLines(2);englishView.setEllipsize(android.text.TextUtils.TruncateAt.END);englishView.setOnLongClickListener(v->{copyEnglish();return true;});
        translationRow.addView(englishView,new LinearLayout.LayoutParams(0,-2,1));
        useTranslationButton=Ui.tool(this,"用英文","使用译文",this::insertEnglish);useTranslationButton.setTextColor(palette.accent);useTranslationButton.setTextSize(12);
        translationRow.addView(useTranslationButton,new LinearLayout.LayoutParams(Ui.dp(this,58),Ui.dp(this,42)));
        LinearLayout.LayoutParams translationParams=new LinearLayout.LayoutParams(-1,Ui.dp(this,42));translationParams.setMargins(Ui.dp(this,5),0,Ui.dp(this,5),Ui.dp(this,4));root.addView(translationRow,translationParams);
        compositionView=Ui.text(this,"",12,palette.muted);compositionView.setMaxLines(1);compositionView.setEllipsize(android.text.TextUtils.TruncateAt.END);compositionView.setPadding(Ui.dp(this,12),0,0,0);root.addView(compositionView,new LinearLayout.LayoutParams(-1,Ui.dp(this,16)));
        candidateScroll=new HorizontalScrollView(this);candidateScroll.setHorizontalScrollBarEnabled(false);
        candidateRow=new LinearLayout(this);candidateScroll.addView(candidateRow);root.addView(candidateScroll,new LinearLayout.LayoutParams(-1,Ui.dp(this,40)));
        keys=Ui.column(this);root.addView(keys);
        historyRows=Ui.column(this);root.addView(historyRows);
        appearanceRows=Ui.column(this);root.addView(appearanceRows);
        renderKeys();refresh();renderHistory();renderAppearance();return root;
    }
    private void rebuildKeyboard(){if(root!=null){setInputView(onCreateInputView());root.requestApplyInsets();}}
    @Override public void onConfigurationChanged(Configuration config){super.onConfigurationChanged(config);rebuildKeyboard();}
    private void openSettings(){startActivity(new Intent(this,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));}
    private void updatePanels(){
        if(root==null)return;boolean overlay=(historyOpen&&!privateField)||appearanceOpen;
        boolean composing=!latin&&!raw.isEmpty()&&!overlay;
        toolbar.setVisibility(overlay||composing?View.GONE:View.VISIBLE);
        candidateScroll.setVisibility(composing?View.VISIBLE:View.GONE);
        translationRow.setVisibility(!overlay&&!privateField?View.VISIBLE:View.GONE);
        compositionView.setText(privateField?"":raw);
        compositionView.setVisibility(composing?View.VISIBLE:View.GONE);
        directionButton.setText(latin?"英 → 中":"中 → 英");directionButton.setEnabled(!privateField);
        useTranslationButton.setText(latin?"用中文":"用英文");useTranslationButton.setContentDescription(latin?"使用中文译文":"使用英文译文");
        useTranslationButton.setEnabled(!english.isEmpty());useTranslationButton.setAlpha(english.isEmpty()?0.4f:1f);
        keys.setVisibility(overlay?View.GONE:View.VISIBLE);
    }
    private void renderAppearance(){
        if(appearanceRows==null)return;appearanceRows.removeAllViews();appearanceRows.setVisibility(appearanceOpen?View.VISIBLE:View.GONE);updatePanels();if(!appearanceOpen)return;
        var header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);
        Button back=Ui.tool(this,"‹","返回键盘",()->{appearanceOpen=false;renderAppearance();});header.addView(back,new LinearLayout.LayoutParams(Ui.dp(this,44),Ui.dp(this,44)));
        header.addView(Ui.text(this,"键盘调节",15,palette.ink),new LinearLayout.LayoutParams(0,-2,1));
        header.addView(Ui.tool(this,"设置","打开浮译设置",this::openSettings),new LinearLayout.LayoutParams(Ui.dp(this,60),Ui.dp(this,44)));appearanceRows.addView(header);
        var controls=Ui.column(this);controls.setPadding(Ui.dp(this,18),Ui.dp(this,12),Ui.dp(this,18),Ui.dp(this,12));
        controls.addView(Ui.text(this,"外观",12,palette.muted));var themes=new LinearLayout(this);
        String[] names={"跟随系统","浅色","深色"},values={"system","light","dark"};
        for(int i=0;i<names.length;i++){String value=values[i];var button=Ui.tool(this,names[i],"主题："+names[i],()->Options.setTheme(this,value));button.setTextSize(14);button.setTextColor(Options.theme(this).equals(value)?palette.onAccent:palette.ink);button.setBackground(Ui.round(Options.theme(this).equals(value)?palette.accent:palette.key,8,this));var lp=new LinearLayout.LayoutParams(0,Ui.dp(this,40),1);lp.setMargins(Ui.dp(this,2),Ui.dp(this,8),Ui.dp(this,2),Ui.dp(this,16));themes.addView(button,lp);}controls.addView(themes);
        TextView height=Ui.text(this,"键盘高度  "+Math.round(Options.keyHeight(this)*100f/54)+"%",14,palette.ink);controls.addView(height);
        SeekBar seek=new SeekBar(this);seek.setContentDescription("键盘高度");seek.setMax(24);seek.setProgress(Options.keyHeight(this)-44);seek.setProgressTintList(android.content.res.ColorStateList.valueOf(palette.accent));seek.setThumbTintList(android.content.res.ColorStateList.valueOf(palette.accent));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar bar,int value,boolean user){height.setText("键盘高度  "+Math.round((44+value)*100f/54)+"%");}public void onStartTrackingTouch(SeekBar bar){}public void onStopTrackingTouch(SeekBar bar){Options.setKeyHeight(FuyiImeService.this,44+bar.getProgress());}});
        controls.addView(seek,new LinearLayout.LayoutParams(-1,Ui.dp(this,44)));
        var scale=new LinearLayout(this);scale.addView(Ui.text(this,"紧凑",12,palette.muted),new LinearLayout.LayoutParams(0,-2,1));scale.addView(Ui.text(this,"舒展",12,palette.muted));controls.addView(scale);
        var reset=Ui.tool(this,"恢复默认高度","恢复默认高度",()->Options.setKeyHeight(this,54));reset.setTextSize(12);controls.addView(reset,new LinearLayout.LayoutParams(-1,Ui.dp(this,40)));
        ScrollView scroll=new ScrollView(this);scroll.addView(controls);appearanceRows.addView(scroll,new LinearLayout.LayoutParams(-1,Ui.dp(this,Options.keyHeight(this)*4+54)));
    }
    @Override public void onStartInput(EditorInfo info,boolean restarting) {
        super.onStartInput(info,restarting); active=true; privateField=InputSafety.privateField(info);
        latin=privateField;shifted=false;symbols=(info.inputType&InputType.TYPE_MASK_CLASS)==InputType.TYPE_CLASS_NUMBER || (info.inputType&InputType.TYPE_MASK_CLASS)==InputType.TYPE_CLASS_PHONE;
        raw="";sentence="";english="";translatedSource="";generation++;compositionStart=-1;appearanceOpen=false;
        lastSelection=info.initialSelStart;expectedCaret=lastSelection;historyOpen=false;
        handler.removeCallbacks(expiryTick);handler.postDelayed(expiryTick,1000);
        capture.capture();if(root!=null) {renderKeys();refresh();renderHistory();renderAppearance();}
    }
    @Override public void onStartInputView(EditorInfo info,boolean restarting) {
        super.onStartInputView(info,restarting);active=true;capture.capture();if(root!=null) {renderKeys();refresh();renderHistory();renderAppearance();}
    }
    @Override public void onFinishInput() {
        active=false;generation++;handler.removeCallbacks(expiryTick);
        if(translationWork!=null) handler.removeCallbacks(translationWork);
        if(translator!=null)translator.cancel();
        raw="";sentence="";english="";translatedSource="";historyOpen=false;appearanceOpen=false;
        if(root!=null) {refresh();renderHistory();} super.onFinishInput();
    }
    @Override public void onUpdateSelection(int oldStart,int oldEnd,int newStart,int newEnd,int candStart,int candEnd) {
        super.onUpdateSelection(oldStart,oldEnd,newStart,newEnd,candStart,candEnd);
        if(raw.isEmpty() && !sentence.isEmpty() && expectedCaret>=0 && (newStart!=expectedCaret || newStart!=newEnd)) clearSentence();
        if(!raw.isEmpty() && (newStart!=newEnd || (candStart>=0 && newStart!=candEnd))) {
            InputConnection ic=getCurrentInputConnection();if(ic!=null)ic.finishComposingText();raw="";clearSentence();
        }
        if(!raw.isEmpty() && candStart<0 && newStart!=expectedCaret) {raw="";clearSentence();refresh();}
        lastSelection=newStart;
    }
    private boolean expired() { return android.os.SystemClock.elapsedRealtime()-touched > (sentence.matches("(?s).*[。！？.!?]$")?8000:60000); }
    private void clearSentence() {sentence="";english="";translatedSource="";generation++;if(root!=null)refresh();}
    private void toggleMode() {if(privateField)return;flushRaw();clearSentence();latin=!latin;symbols=false;shifted=false;renderKeys();refresh();}
    private void type(String value) {
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        if(latin&&!privateField&&!symbols){
            if(raw.length()>=1000)return;
            if(raw.isEmpty())compositionStart=lastSelection;
            raw+=value;expectedCaret=compositionStart<0?-1:compositionStart+raw.length();ic.setComposingText(raw,1);touched=android.os.SystemClock.elapsedRealtime();refresh();return;
        }
        if(privateField || symbols || !value.matches("[a-z']")) {
            flushRaw();ic.commitText(value,1);expectedCaret=lastSelection+value.length();
            if(!privateField && !latin && !sentence.isEmpty()) {sentence+=value;touched=android.os.SystemClock.elapsedRealtime();refresh();}
            else clearSentence();return;
        }
        if(dictionary==null){englishView.setText("词库正在准备，请稍候");return;}
        if(raw.isEmpty()) { if(expired())clearSentence();compositionStart=lastSelection; }
        if(raw.length()>=80)return;
        raw+=value;expectedCaret=compositionStart<0?-1:compositionStart+raw.length();ic.setComposingText(raw,1);refresh();
    }
    private void choose(PinyinDictionary.Candidate candidate) {
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        int start=compositionStart;
        String rest=raw.substring(Math.min(raw.length(),candidate.consumed()));
        expectedCaret=start<0?-1:start+candidate.text().length();
        ic.beginBatchEdit();
        try {
        ic.commitText(candidate.text(),1);
        sentence+=candidate.text();if(sentence.length()>1000)sentence=candidate.text();
        touched=android.os.SystemClock.elapsedRealtime();raw=rest;compositionStart=expectedCaret;
        if(!raw.isEmpty())ic.setComposingText(raw,1);
        } finally {ic.endBatchEdit();}
        refresh();
    }
    private void flushRaw() {
        InputConnection ic=getCurrentInputConnection();
        if(!raw.isEmpty() && ic!=null) {ic.commitText(raw,1);raw="";clearSentence();}
    }
    private void space() {
        if(latin){type(" ");return;}
        if(!raw.isEmpty() && !candidates.isEmpty()) {
            if(Options.prefs(this).getBoolean("englishDefault",false)) insertEnglish();else choose(candidates.get(0));
        } else if(!raw.isEmpty()) {flushRaw();refresh();} else type(" ");
    }
    private void backspace() {
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        if(!raw.isEmpty()) {raw=raw.substring(0,raw.offsetByCodePoints(raw.length(),-1));expectedCaret=compositionStart<0?-1:compositionStart+raw.length();ic.setComposingText(raw,1);refresh();return;}
        if(!sentence.isEmpty()) {sentence=sentence.substring(0,sentence.offsetByCodePoints(sentence.length(),-1));touched=android.os.SystemClock.elapsedRealtime();}
        ic.deleteSurroundingTextInCodePoints(1,0);expectedCaret=-1;refresh();
    }
    private String preview() {
        if(latin)return raw.trim();
        if(raw.isEmpty())return sentence;
        if(candidates.isEmpty())return sentence;
        var first=candidates.get(0);
        String rest=raw.substring(Math.min(raw.length(),first.consumed()));
        String tail=dictionary.complete(rest);
        return !rest.isEmpty()&&tail.isEmpty()?sentence:sentence+first.text()+tail;
    }
    private void refresh() {
        if(root==null)return;
        candidates=latin||raw.isEmpty()||dictionary==null?List.of():dictionary.candidates(raw);
        modeButton.setText(privateField?"安全":latin?"EN":"中/英");
        modeButton.setContentDescription(privateField?"安全输入":latin?"切换中英文：英文":"切换中英文：中文拼音");
        historyButton.setEnabled(!privateField);
        candidateRow.removeAllViews();
        for(var candidate:candidates) {
            Button b=Ui.tool(this,candidate.text(),candidate.text(),()->choose(candidate));b.setTextSize(19);b.setTextColor(candidate==candidates.get(0)?palette.accent:palette.ink);b.setBackgroundColor(Color.TRANSPARENT);
            LinearLayout.LayoutParams candidateParams=new LinearLayout.LayoutParams(-2,Ui.dp(this,38));candidateParams.setMargins(Ui.dp(this,4),Ui.dp(this,2),Ui.dp(this,4),Ui.dp(this,2));candidateRow.addView(b,candidateParams);
        }
        compositionView.setText(privateField?"密码输入：不翻译、不读取剪贴板":latin?"":sentence.isEmpty()?raw:sentence+(raw.isEmpty()?"":" · "+raw));
        compositionView.setContentDescription(privateField?"安全输入":"当前中文句子："+sentence);
        // The preview also exposes confirmed context when the pinyin row is collapsed.
        englishView.setContentDescription(privateField?"安全输入":latin?"当前英文句子："+raw:"当前中文句子："+sentence);updatePanels();
        if(translator!=null)translator.cancel();
        if(privateField||!active) {english="";translatedSource="";generation++;englishView.setText("安全输入");updatePanels();return;}
        String source=preview();generation++;int ticket=generation;
        if(translationWork!=null)handler.removeCallbacks(translationWork);
        english="";translatedSource="";
        if(source.isEmpty()){englishView.setText(latin?"输入英文，即时译成中文":"输入中文，即时译成英文");englishView.setTextColor(palette.muted);updatePanels();return;}
        englishView.setTextColor(palette.muted);englishView.setText("正在翻译…");updatePanels();
        translationWork=() -> {
            if(translator==null)translator=new LocalTranslator(this);
            translator.translate(source,latin,(text,status) -> {
                if(!active||privateField||ticket!=generation)return;
                english=text;translatedSource=text.isEmpty()?"":source;
                englishView.setText(text.isEmpty()?status:text);englishView.setTextColor(text.isEmpty()?palette.muted:palette.ink);updatePanels();
            });
        };
        handler.postDelayed(translationWork,280);
    }
    private void insertEnglish() {
        if(privateField)return;
        if(english.isEmpty()||!translatedSource.equals(preview())) {if(englishView!=null)englishView.setText("译文准备好后再试");return;}
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        // Replace only the exact draft owned by this keyboard. Never delete arbitrary editor text.
        String owned=latin?raw:sentence+raw;
        CharSequence before=ic.getTextBeforeCursor(owned.length(),0);
        if(owned.isEmpty()||before==null||!owned.contentEquals(before)) {englishView.setText("光标已移动，长按复制译文");return;}
        String value=english;
        ic.beginBatchEdit();try{ic.finishComposingText();ic.deleteSurroundingText(owned.length(),0);ic.commitText(value,1);raw="";sentence="";expectedCaret=-1;}finally{ic.endBatchEdit();}
        clearSentence();refresh();
    }
    private void copyEnglish() {
        if(privateField||english.isEmpty())return;
        getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("浮译译文",english));
        Toast.makeText(this,"译文已复制",Toast.LENGTH_SHORT).show();
    }
    private void renderKeys() {
        if(keys==null)return;keys.removeAllViews();
        {
            String[] rows=symbols?(moreSymbols?new String[]{"[]{}<>^~`|","_+×÷≠≈€£¢","…—·「」『』"}:new String[]{"1234567890","@#¥%&*()-","，。！？；：/"}):new String[]{"qwertyuiop","asdfghjkl","zxcvbnm"};
            for(int r=0;r<rows.length;r++){
                LinearLayout line=new LinearLayout(this);
                if(!symbols&&r==1)addSpacer(line,.5f);
                if(r==2)addKey(line,symbols?"#+=":"⇧",symbols?"更多符号":"大写",1.5f,shifted?palette.key:palette.function,()->{if(symbols){moreSymbols=!moreSymbols;renderKeys();refresh();}else{shifted=!shifted;renderKeys();refresh();}},false);
                for(int i=0;i<rows[r].length();i++){
                    String letter=rows[r].substring(i,i+1);String value=shifted&&!symbols?letter.toUpperCase(Locale.ROOT):letter;
                    String label=symbols?letter:letter.toUpperCase(Locale.ROOT);
                    addKey(line,label,"按键 "+value,1,palette.key,()->{type(value);if(shifted&&!symbols){shifted=false;renderKeys();refresh();}},false);
                }
                if(!symbols&&r==1)addSpacer(line,.5f);
                if(r==2)addKey(line,"⌫","退格",1.5f,palette.function,this::backspace,true);
                keys.addView(line);
            }
        }
        LinearLayout bottom=new LinearLayout(this);
        addKey(bottom,symbols?"ABC":"123","数字与符号",1.2f,palette.function,()->{flushRaw();symbols=!symbols;moreSymbols=false;shifted=false;renderKeys();refresh();},false);
        addKey(bottom,latin?",":"，","逗号",.85f,palette.key,()->type(latin?",":"，"),false);
        addKey(bottom,"空格","空格",4f,palette.key,this::space,false).setTextSize(13);
        addKey(bottom,latin?".":"。","句号",.85f,palette.key,()->type(latin?".":"。"),false);
        modeButton=addKey(bottom,privateField?"安全":latin?"EN":"中/英",privateField?"安全输入":latin?"切换中英文：英文":"切换中英文：中文拼音",1.2f,palette.function,this::toggleMode,false);modeButton.setTextSize(14);
        Button enter=addKey(bottom,enterLabel(),"执行："+enterLabel(),2f,palette.accent,this::enter,false);enter.setTextSize(16);
        keys.addView(bottom);
    }
    private void addSpacer(LinearLayout line,float weight){line.addView(new View(this),new LinearLayout.LayoutParams(0,1,weight));}
    private Button addKey(LinearLayout line,String label,String description,float weight,int color,Runnable action,boolean repeat){
        Button b=Ui.key(this,label,description,color,action,repeat);line.addView(b,new LinearLayout.LayoutParams(0,Ui.dp(this,Options.keyHeight(this)),weight));return b;
    }
    private String enterLabel(){
        EditorInfo info=getCurrentInputEditorInfo();if(info==null||(info.imeOptions&EditorInfo.IME_FLAG_NO_ENTER_ACTION)!=0)return "换行";
        return switch(info.imeOptions&EditorInfo.IME_MASK_ACTION){case EditorInfo.IME_ACTION_SEND->"发送";case EditorInfo.IME_ACTION_SEARCH->"搜索";case EditorInfo.IME_ACTION_GO->"前往";case EditorInfo.IME_ACTION_NEXT->"下一项";case EditorInfo.IME_ACTION_DONE->"完成";default->"换行";};
    }
    private void enter(){
        if(latin&&!raw.isEmpty()){
            if(Options.prefs(this).getBoolean("chineseDefault",false)){insertEnglish();return;}
            flushRaw();refresh();return;
        }
        if(!raw.isEmpty()){if(!candidates.isEmpty()){if(Options.prefs(this).getBoolean("englishDefault",false))insertEnglish();else choose(candidates.get(0));}else{flushRaw();refresh();}return;}
        InputConnection ic=getCurrentInputConnection();EditorInfo info=getCurrentInputEditorInfo();if(ic==null)return;
        int action=info==null?EditorInfo.IME_ACTION_NONE:info.imeOptions&EditorInfo.IME_MASK_ACTION;
        if(info==null||(info.imeOptions&EditorInfo.IME_FLAG_NO_ENTER_ACTION)!=0||action==EditorInfo.IME_ACTION_NONE||action==EditorInfo.IME_ACTION_UNSPECIFIED)ic.commitText("\n",1);else ic.performEditorAction(action);
        clearSentence();
    }
    private void renderHistory() {
        if(keys==null)return;boolean show=historyOpen&&!privateField&&!appearanceOpen;keys.setVisibility(show||appearanceOpen?View.GONE:View.VISIBLE);historyRows.setVisibility(show?View.VISIBLE:View.GONE);updatePanels();
        historyRows.removeAllViews();if(!show)return;
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);header.setPadding(Ui.dp(this,10),0,Ui.dp(this,10),0);
        Button back=Ui.tool(this,"⌃","返回键盘",()->{historyOpen=false;renderHistory();});header.addView(back,new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38)));
        header.addView(new View(this),new LinearLayout.LayoutParams(0,1,1));
        TextView title=Ui.text(this,"剪贴板",16,palette.ink);title.setGravity(Gravity.CENTER);title.setBackground(Ui.round(palette.key,10,this));header.addView(title,new LinearLayout.LayoutParams(Ui.dp(this,88),Ui.dp(this,34)));
        TextView expiry=Ui.text(this,"24 小时",14,palette.muted);expiry.setGravity(Gravity.CENTER);header.addView(expiry,new LinearLayout.LayoutParams(Ui.dp(this,74),Ui.dp(this,34)));
        header.addView(new View(this),new LinearLayout.LayoutParams(0,1,1));
        Button more=Ui.tool(this,"⋯","剪贴板操作",()->{});more.setOnClickListener(v->{PopupMenu menu=new PopupMenu(this,more);menu.getMenu().add("清空").setOnMenuItemClickListener(item->{history.clear();renderHistory();return true;});menu.show();});header.addView(more,new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38)));
        historyRows.addView(header,new LinearLayout.LayoutParams(-1,Ui.dp(this,50)));
        ScrollView scroller=new ScrollView(this);scroller.setFillViewport(true);LinearLayout list=Ui.column(this);list.setPadding(Ui.dp(this,10),Ui.dp(this,5),Ui.dp(this,10),Ui.dp(this,4));
        historyDeadline=Long.MAX_VALUE;
        if(!Options.history(this)){
            TextView consent=Ui.text(this,"开启复制历史\n仅本机加密保存，24 小时自动过期。",16,palette.muted);consent.setPadding(12,16,12,16);list.addView(consent);
            list.addView(Ui.button(this,"开启剪贴板历史",()->{Options.history(this,true);renderHistory();}));
        }else{
            var entries=history.list();
            for(var entry:entries){
                historyDeadline=Math.min(historyDeadline,entry.copiedAt()+HistoryPolicy.TTL);
                LinearLayout card=new LinearLayout(this);card.setGravity(Gravity.CENTER_VERTICAL);card.setBackground(Ui.round(palette.key,12,this));
                LinearLayout.LayoutParams cardParams=new LinearLayout.LayoutParams(-1,Ui.dp(this,52));cardParams.bottomMargin=Ui.dp(this,8);
                long minutes=Math.max(1,(entry.copiedAt()+HistoryPolicy.TTL-System.currentTimeMillis()+59999)/60000);
                Button content=Ui.button(this,entry.text(),()->{
                    if(InputSafety.privateField(getCurrentInputEditorInfo()))return;
                    String value=history.resolve(entry.id());InputConnection ic=getCurrentInputConnection();if(value!=null&&ic!=null){flushRaw();ic.commitText(value,1);clearSentence();}renderHistory();
                });content.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);content.setPadding(Ui.dp(this,14),0,Ui.dp(this,6),0);content.setTextSize(16);content.setTextColor(palette.ink);content.setMaxLines(1);content.setEllipsize(android.text.TextUtils.TruncateAt.END);content.setStateListAnimator(null);content.setElevation(0);content.setBackgroundColor(android.graphics.Color.TRANSPARENT);content.setContentDescription("插入历史："+entry.text());
                card.addView(content,new LinearLayout.LayoutParams(0,-1,1));
                Button menuButton=Ui.tool(this,"⋮","记录操作："+entry.text(),()->{});menuButton.setTextColor(palette.muted);menuButton.setTextSize(23);
                menuButton.setOnClickListener(v->{PopupMenu menu=new PopupMenu(this,menuButton);menu.getMenu().add((minutes>=60?minutes/60+" 小时":minutes+" 分钟")+"后过期").setEnabled(false);menu.getMenu().add("删除").setOnMenuItemClickListener(item->{history.remove(entry.id());renderHistory();return true;});menu.show();});
                card.addView(menuButton,new LinearLayout.LayoutParams(Ui.dp(this,44),-1));list.addView(card,cardParams);
            }
            if(entries.isEmpty()){TextView empty=Ui.text(this,"还没有复制记录\n复制文字后，点这里的卡片即可插入。",15,palette.muted);empty.setGravity(Gravity.CENTER);list.addView(empty,new LinearLayout.LayoutParams(-1,Ui.dp(this,180)));}
        }
        scroller.addView(list);historyRows.addView(scroller,new LinearLayout.LayoutParams(-1,Ui.dp(this,Options.keyHeight(this)*4+48)));
    }
    @Override public void onDestroy(){Options.prefs(this).unregisterOnSharedPreferenceChangeListener(optionsChanged);generation++;handler.removeCallbacksAndMessages(null);if(capture!=null)capture.close();if(translator!=null)translator.close();super.onDestroy();}
}
