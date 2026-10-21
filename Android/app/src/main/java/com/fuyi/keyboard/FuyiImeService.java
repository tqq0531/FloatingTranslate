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
    private LinearLayout root, candidateRow, keys, historyRows;
    private TextView englishView, compositionView;
    private Button modeButton, historyButton;
    private boolean privateField=true, latin=false, symbols=false, shifted=false, historyOpen=false;
    private boolean active=false;
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
        root=Ui.column(this);root.setPadding(Ui.dp(this,6),Ui.dp(this,6),Ui.dp(this,6),Ui.dp(this,6));root.setBackgroundColor(Ui.BG);
        root.setOnApplyWindowInsetsListener((v,insets)->{
            int bottom=android.os.Build.VERSION.SDK_INT>=30?insets.getInsets(WindowInsets.Type.navigationBars()).bottom:insets.getSystemWindowInsetBottom();
            v.setPadding(Ui.dp(this,6),Ui.dp(this,6),Ui.dp(this,6),bottom+Ui.dp(this,6));return insets;
        });
        LinearLayout bar=new LinearLayout(this); bar.setGravity(Gravity.CENTER_VERTICAL);
        modeButton=Ui.button(this,"中 / EN",this::toggleMode);
        historyButton=Ui.button(this,"剪贴板",() -> { if(privateField) return; flushRaw(); historyOpen=!historyOpen; renderHistory(); });
        Button settings=Ui.button(this,"设置",() -> startActivity(new Intent(this,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)));
        for(Button b:List.of(modeButton,historyButton,settings)) bar.addView(b,new LinearLayout.LayoutParams(0,Ui.dp(this,38),1));
        root.addView(bar);
        englishView=Ui.text(this,"中文输入 · 整句英文会显示在这里",16,Ui.INK);englishView.setPadding(Ui.dp(this,10),Ui.dp(this,6),Ui.dp(this,10),Ui.dp(this,6));
        englishView.setMaxLines(2);englishView.setMinHeight(Ui.dp(this,44));englishView.setOnLongClickListener(v -> { copyEnglish(); return true; });
        root.addView(englishView);
        compositionView=Ui.text(this,"",12,Ui.MUTED);compositionView.setMaxLines(1);compositionView.setEllipsize(android.text.TextUtils.TruncateAt.END);compositionView.setPadding(Ui.dp(this,10),0,0,0); root.addView(compositionView);
        HorizontalScrollView scroll=new HorizontalScrollView(this);scroll.setHorizontalScrollBarEnabled(false);
        candidateRow=new LinearLayout(this);scroll.addView(candidateRow);root.addView(scroll,new LinearLayout.LayoutParams(-1,Ui.dp(this,44)));
        keys=Ui.column(this);root.addView(keys);
        historyRows=Ui.column(this);root.addView(historyRows);
        renderKeys();refresh();return root;
    }
    @Override public void onStartInput(EditorInfo info,boolean restarting) {
        super.onStartInput(info,restarting); active=true; privateField=InputSafety.privateField(info);
        latin=privateField;symbols=(info.inputType&InputType.TYPE_MASK_CLASS)==InputType.TYPE_CLASS_NUMBER || (info.inputType&InputType.TYPE_MASK_CLASS)==InputType.TYPE_CLASS_PHONE;
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
    private void toggleMode() {flushRaw();latin=!latin;symbols=false;renderKeys();refresh();}
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
        modeButton.setText(privateField?"安全输入":latin?"EN · 英文":"中 · 拼音");
        historyButton.setEnabled(!privateField);
        candidateRow.removeAllViews();
        for(var candidate:candidates) {
            Button b=Ui.button(this,candidate.text(),() -> choose(candidate));b.setTextSize(20);
            candidateRow.addView(b,new LinearLayout.LayoutParams(-2,Ui.dp(this,42)));
        }
        compositionView.setText(privateField?"密码输入：不翻译、不读取剪贴板":latin?"":sentence.isEmpty()?raw:sentence+(raw.isEmpty()?"":" · "+raw));
        compositionView.setContentDescription(privateField?"安全输入":"当前中文句子："+sentence);
        if(privateField||latin) {english="";translatedSource="";generation++;englishView.setText(privateField?"安全输入":"English · 直接输入");return;}
        String source=preview();generation++;int ticket=generation;
        if(translationWork!=null)handler.removeCallbacks(translationWork);
        english="";translatedSource="";
        if(source.isEmpty()){englishView.setText("中文输入 · 长按译文复制");return;}
        englishView.setText("正在翻译整句…");
        translationWork=() -> {
            if(translator==null)translator=new LocalTranslator();
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
        String[] rows=symbols?new String[]{"1234567890","@#¥%&*()-","，。！？；：/\"'"}:new String[]{"qwertyuiop","asdfghjkl","zxcvbnm"};
        for(String row:rows) {
            LinearLayout line=new LinearLayout(this);
            for(int i=0;i<row.length();i++) {
                String letter=row.substring(i,i+1);String value=shifted&&!symbols?letter.toUpperCase(Locale.ROOT):letter;
                Button b=Ui.button(this,value,() -> type(value));b.setTextSize(20);
                LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,Ui.dp(this,48),1);lp.setMargins(Ui.dp(this,2),Ui.dp(this,3),Ui.dp(this,2),Ui.dp(this,3));line.addView(b,lp);
            }
            keys.addView(line);
        }
        LinearLayout bottom=new LinearLayout(this);
        addKey(bottom,symbols?"ABC":"123",1,() -> {flushRaw();symbols=!symbols;renderKeys();refresh();});
        addKey(bottom,"⇧",1,() -> {shifted=!shifted;renderKeys();});
        addKey(bottom,"空格",2.5f,this::space);
        addKey(bottom,"⌫",1,this::backspace);
        addKey(bottom,"↵",1,() -> {
            if(!raw.isEmpty()&&!candidates.isEmpty()){if(Options.prefs(this).getBoolean("englishDefault",false))insertEnglish();else choose(candidates.get(0));return;}
            InputConnection ic=getCurrentInputConnection();EditorInfo e=getCurrentInputEditorInfo();if(ic==null)return;
            int action=e==null?EditorInfo.IME_ACTION_NONE:e.imeOptions&EditorInfo.IME_MASK_ACTION;
            if(action==EditorInfo.IME_ACTION_NONE||action==EditorInfo.IME_ACTION_UNSPECIFIED)ic.commitText("\n",1);else ic.performEditorAction(action);
            clearSentence();
        });keys.addView(bottom);
    }
    private void addKey(LinearLayout line,String label,float weight,Runnable action) {
        Button b=Ui.button(this,label,action);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,Ui.dp(this,46),weight);lp.setMargins(2,4,2,2);line.addView(b,lp);
    }
    private void renderHistory() {
        if(keys==null)return;
        boolean show=historyOpen&&!privateField;keys.setVisibility(show?View.GONE:View.VISIBLE);historyRows.setVisibility(show?View.VISIBLE:View.GONE);
        historyRows.removeAllViews();if(!show)return;
        if(!Options.history(this)) {
            historyRows.addView(Ui.text(this,"开启后保存复制的文字，24 小时自动过期。仅保存在本机，可随时清空。",15,Ui.MUTED));
            historyRows.addView(Ui.button(this,"开启剪贴板历史",() -> {Options.history(this,true);renderHistory();}));return;
        }
        LinearLayout header=new LinearLayout(this);TextView title=Ui.text(this,"复制历史 · 24 小时",14,Ui.MUTED);header.addView(title,new LinearLayout.LayoutParams(0,-2,1));
        header.addView(Ui.button(this,"清空",() -> {history.clear();renderHistory();}));historyRows.addView(header);
        ScrollView scroller=new ScrollView(this);LinearLayout list=Ui.column(this);
        historyDeadline=Long.MAX_VALUE;
        for(var entry:history.list()) {
            historyDeadline=Math.min(historyDeadline,entry.copiedAt()+HistoryPolicy.TTL);
            LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);
            long left=Math.max(1,(entry.copiedAt()+HistoryPolicy.TTL-System.currentTimeMillis()+59999)/60000);
            Button text=Ui.button(this,entry.text()+"\n"+(left>=60?left/60+" 小时":left+" 分钟")+"后过期",() -> {
                if(InputSafety.privateField(getCurrentInputEditorInfo()))return;
                String value=history.resolve(entry.id());InputConnection ic=getCurrentInputConnection();
                if(value!=null&&ic!=null) {flushRaw();ic.commitText(value,1);clearSentence();}
                renderHistory();
            });text.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);text.setMaxLines(3);text.setContentDescription("插入历史："+entry.text());
            row.addView(text,new LinearLayout.LayoutParams(0,Ui.dp(this,68),1));
            row.addView(Ui.button(this,"删除",() -> {history.remove(entry.id());renderHistory();}));list.addView(row);
        }
        if(history.list().isEmpty())list.addView(Ui.text(this,"还没有记录\n在其他应用复制文字后，会显示在这里。",15,Ui.MUTED));
        scroller.addView(list);historyRows.addView(scroller,new LinearLayout.LayoutParams(-1,Ui.dp(this,190)));
    }
    @Override public void onDestroy(){Options.prefs(this).unregisterOnSharedPreferenceChangeListener(optionsChanged);generation++;handler.removeCallbacksAndMessages(null);if(capture!=null)capture.close();if(translator!=null)translator.close();super.onDestroy();}
}
