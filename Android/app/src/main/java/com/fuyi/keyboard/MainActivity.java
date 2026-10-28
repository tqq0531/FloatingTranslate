package com.fuyi.keyboard;

import android.app.*;
import android.content.*;
import android.graphics.Color;
import android.os.Bundle;
import android.provider.Settings;
import android.text.InputType;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;

public final class MainActivity extends Activity {
    private LocalTranslator translator;
    private TextView status,downloadDetail,engineBadge;
    private ProgressBar downloadProgress;
    private Button downloadButton;
    private boolean downloading;
    private DownloadProgress latestProgress=new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1);
    private LinearLayout body;
    private Switch historySwitch,englishSwitch,chineseSwitch;
    private SettingsUi ui;
    private String renderedTheme;
    private SeekBar heightControl;
    private TextView heightValue;
    private boolean rebuilding,appearancePending;
    private final android.content.SharedPreferences.OnSharedPreferenceChangeListener optionsChanged=(prefs,key)->{
        if("keyboardTheme".equals(key) && !Options.theme(this).equals(renderedTheme) && !rebuilding) requestAppearanceRefresh();
        else syncOptions();
    };
    private void syncOptions(){
        if(historySwitch!=null)historySwitch.setChecked(Options.history(this));
        if(englishSwitch!=null)englishSwitch.setChecked(Options.englishDefault(this));
        if(chineseSwitch!=null)chineseSwitch.setChecked(Options.chineseDefault(this));
        if(heightControl!=null)heightControl.setProgress(Options.keyHeight(this)-44);
        if(heightValue!=null)heightValue.setText(Math.round(Options.keyHeight(this)*100f/54)+"%");
    }

    @Override public void onCreate(Bundle state){
        ui=new SettingsUi(this);
        setTheme(ui.dark?android.R.style.Theme_Material_NoActionBar:android.R.style.Theme_Material_Light_NoActionBar);
        super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        createContent();
        beginLanguageDownload();
    }
    private void createContent(){
        ui=new SettingsUi(this);renderedTheme=Options.theme(this);
        getWindow().setStatusBarColor(ui.BG);getWindow().setNavigationBarColor(ui.BG);
        int flags=getWindow().getDecorView().getSystemUiVisibility();
        flags=ui.dark?flags&~(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR):flags|View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        getWindow().getDecorView().setSystemUiVisibility(flags);
        if(android.os.Build.VERSION.SDK_INT>=30){
            var controller=getWindow().getInsetsController();
            if(controller!=null){int appearances=WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS|WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;controller.setSystemBarsAppearance(ui.dark?0:appearances,appearances);}
        }
        ScrollView scroll=new ScrollView(this);scroll.setId(R.id.settings_scroll);scroll.setFillViewport(true);scroll.setVerticalScrollBarEnabled(false);scroll.setBackgroundColor(ui.BG);
        body=Ui.column(this);body.setPadding(d(20),d(16),d(20),d(28));body.setFocusableInTouchMode(true);
        scroll.setOnApplyWindowInsetsListener((v,insets)->{int top,bottom;if(android.os.Build.VERSION.SDK_INT>=30){var bars=insets.getInsets(WindowInsets.Type.systemBars());top=bars.top;bottom=bars.bottom;}else{top=insets.getSystemWindowInsetTop();bottom=insets.getSystemWindowInsetBottom();}v.setPadding(0,top,0,bottom);return insets;});
        scroll.addView(body);setContentView(scroll);
        createHeader();createWelcome();createAppearance();createDictionary();createPreferences();createLanguagePack();createTrial();createPrivacy();
        var footer=SettingsUi.text(this,"浮译 Android  ·  "+BuildConfig.VERSION_NAME,11,ui.MUTED,false);footer.setGravity(Gravity.CENTER);footer.setPadding(0,d(24),0,0);body.addView(footer);
    }
    private boolean trialFocused(){
        View editor=findViewById(R.id.test_editor);return editor!=null&&editor.hasFocus();
    }
    private void requestAppearanceRefresh(){
        // The IME can change its own appearance while this editor is composing.
        // Replacing the Activity view then would create a new InputConnection and lose that session.
        if(trialFocused()){appearancePending=true;return;}
        rebuildAppearance();
    }
    private void rebuildAppearance(){
        rebuilding=true;appearancePending=false;
        EditText previous=findViewById(R.id.test_editor);
        CharSequence text=previous==null?"":previous.getText().toString();
        int start=previous==null?0:previous.getSelectionStart(),end=previous==null?0:previous.getSelectionEnd();
        boolean focused=previous!=null&&previous.hasFocus();
        ScrollView old=findViewById(R.id.settings_scroll);int scrollY=old==null?0:old.getScrollY();
        createContent();
        EditText editor=findViewById(R.id.test_editor);editor.setText(text);
        editor.setSelection(Math.max(0,Math.min(start,text.length())),Math.max(0,Math.min(end,text.length())));
        if(focused)editor.requestFocus();
        ScrollView scroll=findViewById(R.id.settings_scroll);scroll.post(()->scroll.scrollTo(0,scrollY));
        renderDownloadProgress(latestProgress);downloadButton.setEnabled(!downloading);
        rebuilding=false;
    }
    private int d(float value){return SettingsUi.dp(this,value);}
    private void section(String title,String note){
        LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(d(2),d(22),d(2),d(10));
        row.addView(SettingsUi.text(this,title,13,ui.INK,true),new LinearLayout.LayoutParams(0,-2,1));
        if(note!=null)row.addView(SettingsUi.text(this,note,11,ui.MUTED,false));body.addView(row);
    }
    private void createHeader(){
        var row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);
        ImageView icon=new ImageView(this);icon.setImageResource(R.mipmap.ic_launcher);icon.setContentDescription("浮译图标");row.addView(icon,new LinearLayout.LayoutParams(d(44),d(44)));
        var names=Ui.column(this);names.addView(SettingsUi.text(this,"浮译",23,ui.INK,true));SettingsUi.gap(names,4);names.addView(SettingsUi.text(this,"中文 English，自在表达",11,ui.MUTED,false));
        var lp=new LinearLayout.LayoutParams(0,-2,1);lp.leftMargin=d(10);row.addView(names,lp);row.addView(SettingsUi.chip(this,"设置",ui.ACCENT,ui.PALE));body.addView(row);SettingsUi.gap(body,22);
    }
    private void createWelcome(){
        var card=SettingsUi.card(this);
        card.addView(SettingsUi.text(this,"好好打字，轻松翻译。",22,ui.INK,true));SettingsUi.gap(card,8);
        card.addView(SettingsUi.text(this,"中英输入、双向翻译、24 小时复制历史。",12,ui.MUTED,false));SettingsUi.gap(card,18);
        var actions=new LinearLayout(this);
        Button enable=SettingsUi.button(this,"启用键盘",ui.ACCENT,ui.ON_ACCENT,()->startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS)));enable.setContentDescription("启用浮译键盘");
        actions.addView(enable,new LinearLayout.LayoutParams(0,d(44),1));
        Button select=SettingsUi.button(this,"切换输入法",ui.PALE,ui.INK,()->getSystemService(InputMethodManager.class).showInputMethodPicker());
        var lp=new LinearLayout.LayoutParams(0,d(44),1);lp.leftMargin=d(10);actions.addView(select,lp);card.addView(actions);body.addView(card);
    }
    private void createAppearance(){
        section("键盘外观","按你的手感调整");var card=SettingsUi.card(this);
        card.addView(SettingsUi.text(this,"颜色模式",15,ui.INK,true));SettingsUi.gap(card,12);
        var choices=new LinearLayout(this);String[] values={"system","light","dark"};String[] labels={"跟随系统","浅色","深色"};
        for(int i=0;i<values.length;i++){
            String value=values[i];boolean selected=Options.theme(this).equals(value);
            Button button=SettingsUi.button(this,labels[i],selected?ui.ACCENT:ui.PALE,selected?ui.ON_ACCENT:ui.INK,()->{body.requestFocus();Options.setTheme(this,value);});
            button.setTag("theme_"+value);button.setSelected(selected);button.setContentDescription(labels[i]+(selected?"，已选中":""));
            var lp=new LinearLayout.LayoutParams(0,d(44),1);if(i>0)lp.leftMargin=d(6);choices.addView(button,lp);
        }
        card.addView(choices);SettingsUi.gap(card,9);
        card.addView(SettingsUi.text(this,"跟随系统会自动切换白天与夜间外观。",11,ui.MUTED,false));SettingsUi.divider(card);
        var heading=new LinearLayout(this);heading.setGravity(Gravity.CENTER_VERTICAL);
        heading.addView(SettingsUi.text(this,"按键高度",15,ui.INK,true),new LinearLayout.LayoutParams(0,-2,1));
        heightValue=SettingsUi.text(this,Math.round(Options.keyHeight(this)*100f/54)+"%",13,ui.ACCENT,true);heightValue.setTag("keyboard_height_value");heading.addView(heightValue);card.addView(heading);SettingsUi.gap(card,7);
        heightControl=new SeekBar(this);heightControl.setTag("keyboard_height");heightControl.setContentDescription("键盘按键高度");heightControl.setMax(24);heightControl.setProgress(Options.keyHeight(this)-44);
        heightControl.setProgressTintList(android.content.res.ColorStateList.valueOf(ui.ACCENT));heightControl.setThumbTintList(android.content.res.ColorStateList.valueOf(ui.ACCENT));
        heightControl.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(SeekBar view,int value,boolean fromUser){heightValue.setText(Math.round((value+44)*100f/54)+"%");if(fromUser)Options.setKeyHeight(MainActivity.this,value+44);}
            public void onStartTrackingTouch(SeekBar view){}
            public void onStopTrackingTouch(SeekBar view){}
        });
        card.addView(heightControl,new LinearLayout.LayoutParams(-1,d(44)));
        var range=new LinearLayout(this);range.addView(SettingsUi.text(this,"紧凑",11,ui.MUTED,false),new LinearLayout.LayoutParams(0,-2,1));
        range.addView(SettingsUi.text(this,"舒展",11,ui.MUTED,false));card.addView(range);SettingsUi.gap(card,8);
        card.addView(SettingsUi.button(this,"恢复默认高度",ui.PALE,ui.INK,()->Options.setKeyHeight(this,54)));
        SettingsUi.divider(card);
        var preview=SettingsUi.toggle(this,"按键气泡","按下字母时显示放大预览",View.NO_ID,card);
        preview.setChecked(Options.prefs(this).getBoolean("keyPreview",true));
        preview.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("keyPreview",on).apply());
        var haptic=SettingsUi.toggle(this,"按键振动","跟随手机的触感反馈设置",View.NO_ID,card);
        haptic.setChecked(Options.prefs(this).getBoolean("keyHaptic",true));
        haptic.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("keyHaptic",on).apply());
        body.addView(card);
    }
    private void createDictionary(){
        section("拼音词库","两种词库均在手机本地运行");var card=SettingsUi.card(this);
        var toggle=SettingsUi.toggle(this,"扩展开源词库 · 雾凇拼音","开启：基础词库＋雾凇常用词；关闭：仅基础词库",View.NO_ID,card);
        toggle.setContentDescription("扩展开源词库");toggle.setChecked(Options.extendedDictionary(this));
        toggle.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("extendedDictionary",on).apply());
        SettingsUi.gap(card,8);
        card.addView(SettingsUi.text(this,"已内置 · 无需下载 · 离线使用\n基础：Rime / AOSP 拼音词库\n扩展：iDvel/rime-ice，GPL-3.0；常用词精选版。\n支持单字简拼、词语简拼及全拼混输。",12,ui.MUTED,false));
        card.addView(SettingsUi.button(this,"词库来源与许可证",ui.PALE,ui.INK,()->{
            try(var reader=new java.io.BufferedReader(new java.io.InputStreamReader(getAssets().open("licenses/RIME-ICE-NOTICE.txt")))){
                StringBuilder text=new StringBuilder();String line;while((line=reader.readLine())!=null)text.append(line).append('\n');
                new android.app.AlertDialog.Builder(this).setTitle("扩展词库来源").setMessage(text.toString()).setPositiveButton("关闭",null).show();
            }catch(java.io.IOException ignored){Toast.makeText(this,"无法读取词库说明",Toast.LENGTH_SHORT).show();}
        }));body.addView(card);
    }
    private void createPreferences(){
        section("输入偏好","按自己的习惯来");var card=SettingsUi.card(this);
        var learning=SettingsUi.toggle(this,"选词记忆与联想","记住选过的字词，下次相同拼音优先显示；仅本机加密保存",View.NO_ID,card);
        learning.setChecked(Options.learning(this));learning.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("learning",on).apply());
        card.addView(SettingsUi.button(this,"清空选词记忆",ui.PALE,ui.INK,()->{
            PersonalDictionary.get(this).clear();Options.prefs(this).edit().putLong("learningReset",System.currentTimeMillis()).apply();Toast.makeText(this,"选词记忆已清空",Toast.LENGTH_SHORT).show();
        }));SettingsUi.divider(card);
        englishSwitch=SettingsUi.toggle(this,"中文输入，默认输出英文","拼音选词后，空格优先提交英文译文",R.id.english_switch,card);englishSwitch.setChecked(Options.prefs(this).getBoolean("englishDefault",false));
        englishSwitch.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("englishDefault",on).apply());
        SettingsUi.divider(card);
        chineseSwitch=SettingsUi.toggle(this,"英文输入，默认输出中文","英文输入后，优先提交中文译文",View.NO_ID,card);chineseSwitch.setTag("chinese_default");chineseSwitch.setChecked(Options.chineseDefault(this));
        chineseSwitch.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("chineseDefault",on).apply());
        SettingsUi.gap(card,9);card.addView(SettingsUi.text(this,"也可在键盘中直接选择发送原文或译文。",11,ui.MUTED,false));SettingsUi.divider(card);
        historySwitch=SettingsUi.toggle(this,"复制历史","仅本机保存，24 小时自动过期",R.id.history_switch,card);historySwitch.setChecked(Options.history(this));
        historySwitch.setOnCheckedChangeListener((v,enabled)->{if(enabled!=Options.history(this))Options.history(this,enabled);});
        SettingsUi.gap(card,12);var footer=new LinearLayout(this);footer.setGravity(Gravity.CENTER_VERTICAL);
        footer.addView(SettingsUi.text(this,"最多 100 条 · 不保存图片",11,ui.MUTED,false),new LinearLayout.LayoutParams(0,-2,1));
        Button clear=SettingsUi.button(this,"清空记录",ui.DANGER_BG,ui.DANGER,this::confirmClearHistory);clear.setTextSize(13);clear.setPadding(d(8),0,d(8),0);clear.setId(R.id.clear_history);clear.setContentDescription("清空复制历史");footer.addView(clear,new LinearLayout.LayoutParams(d(86),d(44)));card.addView(footer);body.addView(card);
    }
    private void confirmClearHistory(){new AlertDialog.Builder(new ContextThemeWrapper(this,ui.dark?android.R.style.Theme_Material_Dialog_Alert:android.R.style.Theme_Material_Light_Dialog_Alert)).setTitle("清空全部历史？").setMessage("删除浮译保存的复制记录。系统当前剪贴板不受影响。").setNegativeButton("取消",null).setPositiveButton("清空",(dialog,which)->{new ClipboardStore(this).clear();Toast.makeText(this,"历史已清空",Toast.LENGTH_SHORT).show();}).show();}
    private void createLanguagePack(){
        section("本机翻译","正文在设备上处理");var card=SettingsUi.card(this);
        var title=SettingsUi.heading(this,"译","中英离线语言包","模型已随应用内置，无需下载",ui.ACCENT,ui.PALE);
        engineBadge=SettingsUi.chip(this,"离线模型",ui.MUTED,ui.BG);var badgeLp=new LinearLayout.LayoutParams(-2,-2);badgeLp.leftMargin=d(8);title.addView(engineBadge,badgeLp);card.addView(title);SettingsUi.gap(card,16);
        status=SettingsUi.text(this,"从安装包初始化模型，全程无需网络。",12,ui.MUTED,false);status.setId(R.id.download_status);card.addView(status);
        downloadProgress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);downloadProgress.setId(R.id.download_progress);downloadProgress.setMax(100);downloadProgress.setProgressTintList(android.content.res.ColorStateList.valueOf(ui.ACCENT));downloadProgress.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(ui.ACCENT));
        var progressLayout=new LinearLayout.LayoutParams(-1,d(8));progressLayout.topMargin=d(12);progressLayout.bottomMargin=d(8);card.addView(downloadProgress,progressLayout);downloadProgress.setVisibility(View.GONE);
        downloadDetail=SettingsUi.text(this,"",12,ui.ACCENT,true);downloadDetail.setId(R.id.download_detail);downloadDetail.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);downloadDetail.setVisibility(View.GONE);card.addView(downloadDetail);SettingsUi.gap(card,14);
        downloadButton=SettingsUi.button(this,"检查内置模型",ui.PALE,ui.ACCENT,this::beginLanguageDownload);downloadButton.setId(R.id.download_button);card.addView(downloadButton,new LinearLayout.LayoutParams(-1,d(46)));body.addView(card);
    }
    private void createTrial(){
        section("试一试","用一句话，感受浮译");var card=SettingsUi.card(this);
        EditText test=new EditText(this);test.setId(R.id.test_editor);
        test.setOnFocusChangeListener((v,focused)->{
            if(!focused&&appearancePending)v.post(()->{if(!isDestroyed()&&appearancePending&&!trialFocused())rebuildAppearance();});
        });test.setHint("输入 nihao，或切换英文试一试…");test.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);test.setMinLines(2);test.setTextSize(16);test.setTextColor(ui.INK);test.setHintTextColor(ui.MUTED);test.setBackground(SettingsUi.shape(this,ui.BG,13));test.setPadding(d(14),d(12),d(14),d(12));card.addView(test,new LinearLayout.LayoutParams(-1,d(88)));SettingsUi.gap(card,12);
        var sampleRow=new LinearLayout(this);sampleRow.setGravity(Gravity.CENTER_VERTICAL);var sample=SettingsUi.text(this,"你好，今天去北京。",12,ui.MUTED,false);sample.setTextIsSelectable(true);sampleRow.addView(sample,new LinearLayout.LayoutParams(0,-2,1));
        var copy=SettingsUi.button(this,"复制示例",ui.PALE,ui.ACCENT,()->{getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("测试","你好，今天去北京。"));Toast.makeText(this,"示例已复制",Toast.LENGTH_SHORT).show();});copy.setTextSize(13);copy.setPadding(d(8),0,d(8),0);copy.setContentDescription("复制这句测试文字");sampleRow.addView(copy,new LinearLayout.LayoutParams(d(86),d(44)));card.addView(sampleRow);body.addView(card);
    }
    private void createPrivacy(){
        section("隐私与安全","清楚知道数据去向");var card=SettingsUi.card(this);
        SettingsUi.privacyRow(card,"✓","本机加密保存","复制记录与选词记忆仅存于应用私有目录，禁止系统备份。");SettingsUi.gap(card,16);
        SettingsUi.privacyRow(card,"·","敏感输入保护","密码、验证码、密钥建议切回系统键盘。");SettingsUi.gap(card,16);
        SettingsUi.privacyRow(card,"✓","无需联网","模型随应用内置，此版本没有联网权限。");SettingsUi.divider(card);
        var detail=SettingsUi.text(this,"复制历史使用 Android Keystore 加密，24 小时到期后不再展示或允许插入。休眠、关机或强行停止可能推迟文件清理，再次访问会先过滤到期记录。关闭保存开关会立即清空。\n\n密码框、不允许个性化学习的输入框，以及带敏感标记的剪贴板会跳过。来源应用没有正确标记密码时无法可靠识别。\n\n正文与译文使用本机模型处理，没有开发者服务器。开启选词记忆后，仅加密保存选中的字词、对应拼音与相邻选词关系，最多 2000 条；可以关闭或清空，不保存整段输入正文。此版本使用 APK 内置的中英双向翻译模型，已移除 Google ML Kit 和应用联网权限。不下载模型、不调用云翻译，不发送使用统计。此说明适用于本应用；系统及其他应用的网络行为独立。",12,ui.MUTED,false);detail.setId(R.id.privacy_details);detail.setVisibility(View.GONE);
        Button expand=SettingsUi.button(this,"查看完整隐私说明  ⌄",Color.TRANSPARENT,ui.ACCENT,()->{});expand.setId(R.id.privacy_expand);expand.setOnClickListener(v->{boolean show=detail.getVisibility()!=View.VISIBLE;detail.setVisibility(show?View.VISIBLE:View.GONE);expand.setText(show?"收起隐私说明  ⌃":"查看完整隐私说明  ⌄");});card.addView(expand,new LinearLayout.LayoutParams(-1,d(44)));card.addView(detail);body.addView(card);
    }
    void beginLanguageDownload(){
        if(downloading)return;downloading=true;downloadButton.setEnabled(false);
        renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1));
        if(translator==null)translator=new LocalTranslator(this);
        translator.prepare(value->{if(!isDestroyed())renderDownloadProgress(value);},(text,message)->{
            if(isDestroyed())return;downloading=false;boolean ready=message.contains("已就绪");
            renderDownloadProgress(new DownloadProgress(ready?DownloadProgress.Stage.READY:DownloadProgress.Stage.FAILED,latestProgress.downloaded(),latestProgress.total()));
            downloadButton.setEnabled(true);downloadButton.setText(ready?"检查内置模型":"重新初始化模型");
        });
    }
    void renderDownloadProgress(DownloadProgress value){
        latestProgress=value;status.setText(value.message());
        boolean ready=value.stage()==DownloadProgress.Stage.READY,failed=value.stage()==DownloadProgress.Stage.FAILED;
        String phase=switch(value.stage()){case READY->"已就绪";case FAILED->"待重试";case EXTRACTING->"解压中";case VERIFYING->"加载中";default->"准备中";};
        engineBadge.setText(phase);
        if(downloading)downloadButton.setText(value.stage()==DownloadProgress.Stage.EXTRACTING?"正在解压模型…":value.stage()==DownloadProgress.Stage.VERIFYING?"正在加载引擎…":"正在准备语言包…");
        engineBadge.setTextColor(ready?ui.GREEN:ui.ACCENT);engineBadge.setBackground(SettingsUi.shape(this,ready?ui.SUCCESS:ui.PALE,20));downloadProgress.setVisibility(View.VISIBLE);downloadDetail.setVisibility(View.VISIBLE);
        downloadProgress.setIndeterminate(!value.determinate()&&value.stage()!=DownloadProgress.Stage.FAILED);
        if(value.determinate())downloadProgress.setProgress(value.percent());
        downloadProgress.setContentDescription("内置模型："+value.message()+"，"+value.detail());downloadDetail.setText(value.detail());
    }
    @Override protected void onResume(){
        super.onResume();Options.prefs(this).registerOnSharedPreferenceChangeListener(optionsChanged);
        if(!Options.theme(this).equals(renderedTheme)||ui.dark!=KeyboardAppearance.from(this).dark)requestAppearanceRefresh();
        syncOptions();new ClipboardStore(this).list();
    }
    @Override protected void onPause(){Options.prefs(this).unregisterOnSharedPreferenceChangeListener(optionsChanged);super.onPause();}
    @Override protected void onDestroy(){if(translator!=null)translator.close();super.onDestroy();}
}
