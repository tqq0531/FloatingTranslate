package com.fuyi.keyboard;

import android.content.*;
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
        if ("history".equals(key) && this.root!=null) renderHistory();
    };
    private LinearLayout root, candidateRow, keys, historyRows, toolbar;
    private HorizontalScrollView candidateScroll;
    private TextView englishView, compositionView;
    private Button modeButton, historyButton;
    private boolean privateField=true, latin=false, symbols=false, shifted=false, historyOpen=false;
    private boolean active=false, emoji=false, moreSymbols=false;
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
        var executor=Executors.newSingleThreadExecutor();
        executor.execute(() -> {
            try {
                var loaded=new PinyinDictionary(new InputStreamReader(getAssets().open("pinyin_simp.dict.yaml"),StandardCharsets.UTF_8));
                handler.post(() -> { dictionary=loaded; if(root!=null) refresh(); });
            } catch (Exception ignored) { handler.post(() -> { if(englishView!=null) englishView.setText("词库未就绪，请重新安装"); }); }
            finally { executor.shutdown(); }
        });
    }
    @Override public boolean onEvaluateFullscreenMode() { return false; }
    @Override public View onCreateInputView() {
        root=Ui.column(this);root.setBackgroundColor(Ui.KEYBOARD);
        root.setPadding(Ui.dp(this,2),0,Ui.dp(this,2),Ui.dp(this,4));
        root.setOnApplyWindowInsetsListener((v,insets)->{
            int bottom=android.os.Build.VERSION.SDK_INT>=30?insets.getInsets(WindowInsets.Type.navigationBars()).bottom:insets.getSystemWindowInsetBottom();
            v.setPadding(Ui.dp(this,2),0,Ui.dp(this,2),bottom+Ui.dp(this,4));return insets;
        });
        toolbar=new LinearLayout(this);toolbar.setGravity(Gravity.CENTER_VERTICAL);toolbar.setPadding(Ui.dp(this,10),0,Ui.dp(this,10),0);
        Button brand=Ui.tool(this,"译","浮译设置",this::openSettings);brand.setTextColor(Ui.GREEN);
        toolbar.addView(brand,new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38)));
        toolbar.addView(new View(this),new LinearLayout.LayoutParams(0,1,1));
        historyButton=Ui.tool(this,"剪贴板","剪贴板",()->{if(!privateField){flushRaw();historyOpen=true;renderHistory();}});
        toolbar.addView(historyButton,new LinearLayout.LayoutParams(Ui.dp(this,78),Ui.dp(this,38)));
        Button settings=Ui.tool(this,"⚙","设置",this::openSettings);addTool(toolbar,settings);
        Button hide=Ui.tool(this,"⌄","收起键盘",()->requestHideSelf(0));addTool(toolbar,hide);
        root.addView(toolbar,new LinearLayout.LayoutParams(-1,Ui.dp(this,50)));
        englishView=Ui.text(this,"",16,Ui.INK);englishView.setPadding(Ui.dp(this,14),Ui.dp(this,7),Ui.dp(this,14),Ui.dp(this,7));
        englishView.setMaxLines(2);englishView.setOnLongClickListener(v->{copyEnglish();return true;});root.addView(englishView);
        compositionView=Ui.text(this,"",13,Ui.MUTED);compositionView.setMaxLines(1);compositionView.setEllipsize(android.text.TextUtils.TruncateAt.END);compositionView.setPadding(Ui.dp(this,14),0,0,0);root.addView(compositionView);
        candidateScroll=new HorizontalScrollView(this);candidateScroll.setHorizontalScrollBarEnabled(false);
        candidateRow=new LinearLayout(this);candidateScroll.addView(candidateRow);root.addView(candidateScroll,new LinearLayout.LayoutParams(-1,Ui.dp(this,44)));
        keys=Ui.column(this);root.addView(keys);
        historyRows=Ui.column(this);root.addView(historyRows);
        renderKeys();refresh();renderHistory();return root;
    }
    private void openSettings(){startActivity(new Intent(this,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));}
    private void addTool(LinearLayout row,Button button){LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38));lp.leftMargin=Ui.dp(this,10);row.addView(button,lp);}
    private void updatePanels(){
        if(root==null)return;boolean historyShown=historyOpen&&!privateField;
        toolbar.setVisibility(historyShown||!raw.isEmpty()?View.GONE:View.VISIBLE);
        candidateScroll.setVisibility(!historyShown&&!raw.isEmpty()?View.VISIBLE:View.GONE);
        englishView.setVisibility(!historyShown&&!privateField&&!latin&&(!raw.isEmpty()||!sentence.isEmpty())?View.VISIBLE:View.GONE);
        // Keep sentence context accessible without spending a blank row on confirmed words.
        compositionView.setText(privateField?"":raw);
        compositionView.setVisibility(!historyShown&&!raw.isEmpty()?View.VISIBLE:View.GONE);
    }
    @Override public void onStartInput(EditorInfo info,boolean restarting) {
        super.onStartInput(info,restarting); active=true; privateField=InputSafety.privateField(info);
        latin=privateField;shifted=false;emoji=false;symbols=(info.inputType&InputType.TYPE_MASK_CLASS)==InputType.TYPE_CLASS_NUMBER || (info.inputType&InputType.TYPE_MASK_CLASS)==InputType.TYPE_CLASS_PHONE;
        raw="";sentence="";english="";translatedSource="";generation++;compositionStart=-1;
        lastSelection=info.initialSelStart;expectedCaret=lastSelection;historyOpen=false;
        handler.removeCallbacks(expiryTick);handler.postDelayed(expiryTick,1000);
        capture.capture();if(root!=null) {renderKeys();refresh();renderHistory();}
    }
    @Override public void onStartInputView(EditorInfo info,boolean restarting) {
        super.onStartInputView(info,restarting);active=true;capture.capture();if(root!=null) {renderKeys();refresh();renderHistory();}
    }
    @Override public void onFinishInput() {
        active=false;generation++;handler.removeCallbacks(expiryTick);
        if(translationWork!=null) handler.removeCallbacks(translationWork);
        raw="";sentence="";english="";translatedSource="";historyOpen=false;
        if(root!=null) {refresh();renderHistory();} super.onFinishInput();
    }
    @Override public void onUpdateSelection(int oldStart,int oldEnd,int newStart,int newEnd,int candStart,int candEnd) {
        super.onUpdateSelection(oldStart,oldEnd,newStart,newEnd,candStart,candEnd);
        if(raw.isEmpty() && !sentence.isEmpty() && expectedCaret>=0 && (newStart!=expectedCaret || newStart!=newEnd)) clearSentence();
        if(!raw.isEmpty() && candStart<0) {raw="";clearSentence();refresh();}
        lastSelection=newStart;
    }
    private boolean expired() { return android.os.SystemClock.elapsedRealtime()-touched > (sentence.matches("(?s).*[。！？.!?]$")?8000:60000); }
    private void clearSentence() {sentence="";english="";translatedSource="";generation++;if(root!=null)refresh();}
    private void toggleMode() {if(privateField)return;flushRaw();latin=!latin;symbols=false;emoji=false;shifted=false;renderKeys();refresh();}
    private void type(String value) {
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        if(privateField || latin || symbols || !value.matches("[a-z']")) {
            flushRaw();ic.commitText(value,1);expectedCaret=lastSelection+value.length();
            if(!privateField && !latin && !sentence.isEmpty() && value.matches("[，。！？,.!? ]")) {sentence+=value;touched=android.os.SystemClock.elapsedRealtime();refresh();}
            else clearSentence();return;
        }
        if(dictionary==null){englishView.setText("词库正在准备，请稍候");return;}
        if(raw.isEmpty()) { if(expired())clearSentence();compositionStart=lastSelection; }
        if(raw.length()>=80)return;
        raw+=value; ic.setComposingText(raw,1);refresh();
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
        if(!raw.isEmpty() && !candidates.isEmpty()) {
            if(Options.prefs(this).getBoolean("englishDefault",false)) insertEnglish();else choose(candidates.get(0));
        } else if(!raw.isEmpty()) {flushRaw();refresh();} else type(" ");
    }
    private void backspace() {
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        if(!raw.isEmpty()) {raw=raw.substring(0,raw.length()-1);ic.setComposingText(raw,1);refresh();return;}
        if(!sentence.isEmpty()) {sentence=sentence.substring(0,sentence.offsetByCodePoints(sentence.length(),-1));touched=android.os.SystemClock.elapsedRealtime();}
        ic.deleteSurroundingTextInCodePoints(1,0);expectedCaret=-1;refresh();
    }
    private String preview() {
        if(raw.isEmpty())return sentence;
        if(candidates.isEmpty())return sentence;
        var first=candidates.get(0);
        String rest=raw.substring(Math.min(raw.length(),first.consumed()));
        String tail=dictionary.complete(rest);
        return !rest.isEmpty()&&tail.isEmpty()?sentence:sentence+first.text()+tail;
    }
    private void refresh() {
        if(root==null)return;
        candidates=raw.isEmpty()||dictionary==null?List.of():dictionary.candidates(raw);
        modeButton.setText(privateField?"安全":latin?"EN":"中/英");
        modeButton.setContentDescription(privateField?"安全输入":latin?"切换中英文：英文":"切换中英文：中文拼音");
        historyButton.setEnabled(!privateField);
        candidateRow.removeAllViews();
        for(var candidate:candidates) {
            Button b=Ui.tool(this,candidate.text(),candidate.text(),()->choose(candidate));b.setTextSize(20);b.setBackgroundColor(android.graphics.Color.TRANSPARENT);
            candidateRow.addView(b,new LinearLayout.LayoutParams(-2,Ui.dp(this,42)));
        }
        compositionView.setText(privateField?"密码输入：不翻译、不读取剪贴板":latin?"":sentence.isEmpty()?raw:sentence+(raw.isEmpty()?"":" · "+raw));
        compositionView.setContentDescription(privateField?"安全输入":"当前中文句子："+sentence);
        // The preview also exposes confirmed context when the pinyin row is collapsed.
        englishView.setContentDescription(privateField?"安全输入":"当前中文句子："+sentence);updatePanels();
        if(translator!=null)translator.cancel();
        if(privateField||latin) {english="";translatedSource="";generation++;englishView.setText(privateField?"安全输入":"English · 直接输入");return;}
        String source=preview();generation++;int ticket=generation;
        if(translationWork!=null)handler.removeCallbacks(translationWork);
        english="";translatedSource="";
        if(source.isEmpty()){englishView.setText("中文输入 · 长按译文复制");return;}
        englishView.setText("正在翻译整句…");
        translationWork=() -> {
            if(translator==null)translator=new LocalTranslator(this);
            translator.translate(source,(text,status) -> {
                if(!active||privateField||ticket!=generation)return;
                english=text;translatedSource=text.isEmpty()?"":source;
                englishView.setText(text.isEmpty()?status:text);
            });
        };
        handler.postDelayed(translationWork,280);
    }
    private void insertEnglish() {
        if(privateField)return;
        if(raw.isEmpty()||english.isEmpty()||!translatedSource.equals(preview())) {if(englishView!=null)englishView.setText("译文尚未就绪，或没有待确认的拼音");return;}
        if(!sentence.isEmpty()) {englishView.setText("前文已上屏：请长按复制整句英文");return;}
        InputConnection ic=getCurrentInputConnection();if(ic==null)return;
        ic.commitText(english,1);raw="";clearSentence();refresh();
    }
    private void copyEnglish() {
        if(privateField||english.isEmpty())return;
        getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("浮译英文",english));
    }
    private void renderKeys() {
        if(keys==null)return;keys.removeAllViews();
        if(emoji){
            String[][] rows={{"😀","😄","😂","😊","🥰","😍","😎","🤔"},{"👍","👏","🙏","💪","🎉","❤️","🌹","✨"},{"😭","😅","😴","🤝","👌","✅","🔥","🎂"}};
            for(String[] row:rows){LinearLayout line=new LinearLayout(this);for(String value:row)addKey(line,value,"表情 "+value,1,android.graphics.Color.WHITE,()->type(value),false);keys.addView(line);}
        } else {
            String[] rows=symbols?(moreSymbols?new String[]{"[]{}<>^~`|","_+×÷≠≈€£¢","…—·「」『』"}:new String[]{"1234567890","@#¥%&*()-","，。！？；：/"}):new String[]{"qwertyuiop","asdfghjkl","zxcvbnm"};
            for(int r=0;r<rows.length;r++){
                LinearLayout line=new LinearLayout(this);
                if(!symbols&&r==1)addSpacer(line,.5f);
                if(r==2)addKey(line,symbols?"#+=":"⇧",symbols?"更多符号":"大写",1.5f,shifted?android.graphics.Color.WHITE:Ui.FUNCTION,()->{if(symbols){moreSymbols=!moreSymbols;renderKeys();refresh();}else{shifted=!shifted;renderKeys();refresh();}},false);
                for(int i=0;i<rows[r].length();i++){
                    String letter=rows[r].substring(i,i+1);String value=shifted&&!symbols?letter.toUpperCase(Locale.ROOT):letter;
                    String label=symbols?letter:letter.toUpperCase(Locale.ROOT);
                    addKey(line,label,"按键 "+value,1,android.graphics.Color.WHITE,()->{type(value);if(shifted&&!symbols){shifted=false;renderKeys();refresh();}},false);
                }
                if(!symbols&&r==1)addSpacer(line,.5f);
                if(r==2)addKey(line,"⌫","退格",1.5f,Ui.FUNCTION,this::backspace,true);
                keys.addView(line);
            }
        }
        LinearLayout bottom=new LinearLayout(this);
        addKey(bottom,symbols||emoji?"ABC":"123","数字与符号",1.35f,Ui.FUNCTION,()->{flushRaw();if(symbols||emoji){symbols=false;emoji=false;}else{symbols=true;moreSymbols=false;}shifted=false;renderKeys();refresh();},false);
        addKey(bottom,emoji?"键盘":"☺︎","表情键盘",.95f,Ui.FUNCTION,()->{flushRaw();emoji=!emoji;symbols=false;renderKeys();refresh();},false);
        addKey(bottom,latin?",":"，","逗号",.95f,android.graphics.Color.WHITE,()->type(latin?",":"，"),false);
        addKey(bottom,"空格","空格",3.15f,android.graphics.Color.WHITE,this::space,false);
        modeButton=addKey(bottom,privateField?"安全":latin?"EN":"中/英",privateField?"安全输入":latin?"切换中英文：英文":"切换中英文：中文拼音",1.1f,android.graphics.Color.WHITE,this::toggleMode,false);modeButton.setTextSize(15);
        Button enter=addKey(bottom,enterLabel(),"执行："+enterLabel(),1.9f,Ui.GREEN,this::enter,false);enter.setTextSize(18);
        keys.addView(bottom);
    }
    private void addSpacer(LinearLayout line,float weight){line.addView(new View(this),new LinearLayout.LayoutParams(0,1,weight));}
    private Button addKey(LinearLayout line,String label,String description,float weight,int color,Runnable action,boolean repeat){
        Button b=Ui.key(this,label,description,color,action,repeat);line.addView(b,new LinearLayout.LayoutParams(0,Ui.dp(this,58),weight));return b;
    }
    private String enterLabel(){
        EditorInfo info=getCurrentInputEditorInfo();if(info==null||(info.imeOptions&EditorInfo.IME_FLAG_NO_ENTER_ACTION)!=0)return "换行";
        return switch(info.imeOptions&EditorInfo.IME_MASK_ACTION){case EditorInfo.IME_ACTION_SEND->"发送";case EditorInfo.IME_ACTION_SEARCH->"搜索";case EditorInfo.IME_ACTION_GO->"前往";case EditorInfo.IME_ACTION_NEXT->"下一项";case EditorInfo.IME_ACTION_DONE->"完成";default->"换行";};
    }
    private void enter(){
        if(!raw.isEmpty()){if(!candidates.isEmpty()){if(Options.prefs(this).getBoolean("englishDefault",false))insertEnglish();else choose(candidates.get(0));}else{flushRaw();refresh();}return;}
        InputConnection ic=getCurrentInputConnection();EditorInfo info=getCurrentInputEditorInfo();if(ic==null)return;
        int action=info==null?EditorInfo.IME_ACTION_NONE:info.imeOptions&EditorInfo.IME_MASK_ACTION;
        if(info==null||(info.imeOptions&EditorInfo.IME_FLAG_NO_ENTER_ACTION)!=0||action==EditorInfo.IME_ACTION_NONE||action==EditorInfo.IME_ACTION_UNSPECIFIED)ic.commitText("\n",1);else ic.performEditorAction(action);
        clearSentence();
    }
    private void renderHistory() {
        if(keys==null)return;boolean show=historyOpen&&!privateField;keys.setVisibility(show?View.GONE:View.VISIBLE);historyRows.setVisibility(show?View.VISIBLE:View.GONE);updatePanels();
        historyRows.removeAllViews();if(!show)return;
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);header.setPadding(Ui.dp(this,10),0,Ui.dp(this,10),0);
        Button back=Ui.tool(this,"⌃","返回键盘",()->{historyOpen=false;renderHistory();});header.addView(back,new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38)));
        header.addView(new View(this),new LinearLayout.LayoutParams(0,1,1));
        TextView title=Ui.text(this,"剪贴板",16,Ui.INK);title.setGravity(Gravity.CENTER);title.setBackground(Ui.round(android.graphics.Color.WHITE,10,this));header.addView(title,new LinearLayout.LayoutParams(Ui.dp(this,88),Ui.dp(this,34)));
        TextView expiry=Ui.text(this,"24 小时",14,Ui.MUTED);expiry.setGravity(Gravity.CENTER);header.addView(expiry,new LinearLayout.LayoutParams(Ui.dp(this,74),Ui.dp(this,34)));
        header.addView(new View(this),new LinearLayout.LayoutParams(0,1,1));
        Button more=Ui.tool(this,"⋯","剪贴板操作",()->{});more.setOnClickListener(v->{PopupMenu menu=new PopupMenu(this,more);menu.getMenu().add("清空").setOnMenuItemClickListener(item->{history.clear();renderHistory();return true;});menu.show();});header.addView(more,new LinearLayout.LayoutParams(Ui.dp(this,38),Ui.dp(this,38)));
        historyRows.addView(header,new LinearLayout.LayoutParams(-1,Ui.dp(this,50)));
        ScrollView scroller=new ScrollView(this);scroller.setFillViewport(true);LinearLayout list=Ui.column(this);list.setPadding(Ui.dp(this,10),Ui.dp(this,5),Ui.dp(this,10),Ui.dp(this,4));
        historyDeadline=Long.MAX_VALUE;
        if(!Options.history(this)){
            TextView consent=Ui.text(this,"开启复制历史\n仅本机加密保存，24 小时自动过期。",16,Ui.MUTED);consent.setPadding(12,16,12,16);list.addView(consent);
            list.addView(Ui.button(this,"开启剪贴板历史",()->{Options.history(this,true);renderHistory();}));
        }else{
            var entries=history.list();
            for(var entry:entries){
                historyDeadline=Math.min(historyDeadline,entry.copiedAt()+HistoryPolicy.TTL);
                LinearLayout card=new LinearLayout(this);card.setGravity(Gravity.CENTER_VERTICAL);card.setBackground(Ui.round(android.graphics.Color.WHITE,12,this));
                LinearLayout.LayoutParams cardParams=new LinearLayout.LayoutParams(-1,Ui.dp(this,52));cardParams.bottomMargin=Ui.dp(this,8);
                long minutes=Math.max(1,(entry.copiedAt()+HistoryPolicy.TTL-System.currentTimeMillis()+59999)/60000);
                Button content=Ui.button(this,entry.text(),()->{
                    if(InputSafety.privateField(getCurrentInputEditorInfo()))return;
                    String value=history.resolve(entry.id());InputConnection ic=getCurrentInputConnection();if(value!=null&&ic!=null){flushRaw();ic.commitText(value,1);clearSentence();}renderHistory();
                });content.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);content.setPadding(Ui.dp(this,14),0,Ui.dp(this,6),0);content.setTextSize(17);content.setMaxLines(1);content.setEllipsize(android.text.TextUtils.TruncateAt.END);content.setStateListAnimator(null);content.setElevation(0);content.setBackgroundColor(android.graphics.Color.TRANSPARENT);content.setContentDescription("插入历史："+entry.text());
                card.addView(content,new LinearLayout.LayoutParams(0,-1,1));
                Button menuButton=Ui.tool(this,"⋮","记录操作："+entry.text(),()->{});menuButton.setTextColor(Ui.MUTED);menuButton.setTextSize(23);
                menuButton.setOnClickListener(v->{PopupMenu menu=new PopupMenu(this,menuButton);menu.getMenu().add((minutes>=60?minutes/60+" 小时":minutes+" 分钟")+"后过期").setEnabled(false);menu.getMenu().add("删除").setOnMenuItemClickListener(item->{history.remove(entry.id());renderHistory();return true;});menu.show();});
                card.addView(menuButton,new LinearLayout.LayoutParams(Ui.dp(this,44),-1));list.addView(card,cardParams);
            }
            if(entries.isEmpty()){TextView empty=Ui.text(this,"还没有复制记录\n复制文字后，点这里的卡片即可插入。",15,Ui.MUTED);empty.setGravity(Gravity.CENTER);list.addView(empty,new LinearLayout.LayoutParams(-1,Ui.dp(this,180)));}
        }
        scroller.addView(list);historyRows.addView(scroller,new LinearLayout.LayoutParams(-1,Ui.dp(this,232)));
    }
    @Override public void onDestroy(){Options.prefs(this).unregisterOnSharedPreferenceChangeListener(optionsChanged);generation++;handler.removeCallbacksAndMessages(null);if(capture!=null)capture.close();if(translator!=null)translator.close();super.onDestroy();}
}
