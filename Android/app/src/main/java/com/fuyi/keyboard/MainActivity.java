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
    private ModelDownloadMonitor downloadMonitor;
    private boolean downloading;
    private DownloadProgress latestProgress=new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1);
    private LinearLayout body;
    private Switch historySwitch,englishSwitch;
    private final android.content.SharedPreferences.OnSharedPreferenceChangeListener optionsChanged=(prefs,key)->syncOptions();
    private void syncOptions(){if(historySwitch!=null)historySwitch.setChecked(Options.history(this));if(englishSwitch!=null)englishSwitch.setChecked(Options.prefs(this).getBoolean("englishDefault",false));}

    @Override public void onCreate(Bundle state){
        super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        ScrollView scroll=new ScrollView(this);scroll.setId(R.id.settings_scroll);scroll.setFillViewport(true);scroll.setVerticalScrollBarEnabled(false);scroll.setBackgroundColor(SettingsUi.BG);
        body=Ui.column(this);body.setPadding(d(20),d(16),d(20),d(28));body.setFocusableInTouchMode(true);
        scroll.setOnApplyWindowInsetsListener((v,insets)->{int top,bottom;if(android.os.Build.VERSION.SDK_INT>=30){var bars=insets.getInsets(WindowInsets.Type.systemBars());top=bars.top;bottom=bars.bottom;}else{top=insets.getSystemWindowInsetTop();bottom=insets.getSystemWindowInsetBottom();}v.setPadding(0,top,0,bottom);return insets;});
        scroll.addView(body);setContentView(scroll);
        createHeader();createWelcome();createPreferences();createLanguagePack();createTrial();createPrivacy();
        var footer=SettingsUi.text(this,"浮译 Android  ·  0.1.3",11,SettingsUi.MUTED,false);footer.setGravity(Gravity.CENTER);footer.setPadding(0,d(24),0,0);body.addView(footer);
        if(Options.prefs(this).getBoolean("languageDownloadRequested",false))beginLanguageDownload();else checkInstalledLanguagePack();
    }
    private int d(float value){return SettingsUi.dp(this,value);}
    private void section(String title,String note){
        LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(d(2),d(22),d(2),d(10));
        row.addView(SettingsUi.text(this,title,13,SettingsUi.INK,true),new LinearLayout.LayoutParams(0,-2,1));
        if(note!=null)row.addView(SettingsUi.text(this,note,11,SettingsUi.MUTED,false));body.addView(row);
    }
    private void createHeader(){
        var row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);
        ImageView icon=new ImageView(this);icon.setImageResource(R.mipmap.ic_launcher);icon.setContentDescription("浮译图标");row.addView(icon,new LinearLayout.LayoutParams(d(44),d(44)));
        var names=Ui.column(this);names.addView(SettingsUi.text(this,"浮译",23,SettingsUi.INK,true));SettingsUi.gap(names,4);names.addView(SettingsUi.text(this,"让表达，多一种可能",11,SettingsUi.MUTED,false));
        var lp=new LinearLayout.LayoutParams(0,-2,1);lp.leftMargin=d(10);row.addView(names,lp);row.addView(SettingsUi.chip(this,"设置",SettingsUi.ACCENT,SettingsUi.PALE));body.addView(row);SettingsUi.gap(body,22);
    }
    private void createWelcome(){
        var hero=Ui.column(this);hero.setPadding(d(20),d(20),d(20),d(18));
        var gradient=new android.graphics.drawable.GradientDrawable(android.graphics.drawable.GradientDrawable.Orientation.TL_BR,new int[]{0xff203755,0xff142238});gradient.setCornerRadius(d(22));hero.setBackground(gradient);
        hero.addView(SettingsUi.text(this,"输入中文，英文随行。",23,Color.WHITE,true));SettingsUi.gap(hero,9);
        hero.addView(SettingsUi.text(this,"分段输入，也能看见完整的英文表达。",12,0xffB7C7DE,false));SettingsUi.gap(hero,20);
        var actions=new LinearLayout(this);
        Button enable=SettingsUi.button(this,"启用键盘",SettingsUi.ACCENT,Color.WHITE,()->startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS)));enable.setContentDescription("启用浮译键盘");
        actions.addView(enable,new LinearLayout.LayoutParams(0,d(46),1));
        Button select=SettingsUi.button(this,"切换输入法",0xff314660,Color.WHITE,()->getSystemService(InputMethodManager.class).showInputMethodPicker());
        var lp=new LinearLayout.LayoutParams(0,d(46),1);lp.leftMargin=d(10);actions.addView(select,lp);hero.addView(actions);body.addView(hero);
    }
    private void createPreferences(){
        section("输入偏好","按自己的习惯来");var card=SettingsUi.card(this);
        englishSwitch=SettingsUi.toggle(this,"默认输出英文","拼音选词后，优先提交英文",R.id.english_switch,card);englishSwitch.setChecked(Options.prefs(this).getBoolean("englishDefault",false));
        englishSwitch.setOnCheckedChangeListener((v,on)->Options.prefs(this).edit().putBoolean("englishDefault",on).apply());
        SettingsUi.gap(card,9);card.addView(SettingsUi.text(this,"前文已按中文上屏时，长按译文可复制整句。",11,SettingsUi.MUTED,false));SettingsUi.divider(card);
        historySwitch=SettingsUi.toggle(this,"复制历史","仅本机保存，24 小时自动过期",R.id.history_switch,card);historySwitch.setChecked(Options.history(this));
        historySwitch.setOnCheckedChangeListener((v,enabled)->{if(enabled!=Options.history(this))Options.history(this,enabled);});
        SettingsUi.gap(card,12);var footer=new LinearLayout(this);footer.setGravity(Gravity.CENTER_VERTICAL);
        footer.addView(SettingsUi.text(this,"最多 100 条 · 不保存图片",11,SettingsUi.MUTED,false),new LinearLayout.LayoutParams(0,-2,1));
        Button clear=SettingsUi.button(this,"清空记录",0xffFFF1F0,0xffBF5A54,this::confirmClearHistory);clear.setTextSize(13);clear.setPadding(d(8),0,d(8),0);clear.setId(R.id.clear_history);clear.setContentDescription("清空复制历史");footer.addView(clear,new LinearLayout.LayoutParams(d(86),d(44)));card.addView(footer);body.addView(card);
    }
    private void confirmClearHistory(){new AlertDialog.Builder(this).setTitle("清空全部历史？").setMessage("删除浮译保存的复制记录。系统当前剪贴板不受影响。").setNegativeButton("取消",null).setPositiveButton("清空",(dialog,which)->{new ClipboardStore(this).clear();Toast.makeText(this,"历史已清空",Toast.LENGTH_SHORT).show();}).show();}
    private void createLanguagePack(){
        section("本机翻译","正文在设备上处理");var card=SettingsUi.card(this);
        var title=SettingsUi.heading(this,"译","中英离线语言包","一次准备，随时离线使用",SettingsUi.ACCENT,SettingsUi.PALE);
        engineBadge=SettingsUi.chip(this,"离线模型",SettingsUi.MUTED,SettingsUi.BG);var badgeLp=new LinearLayout.LayoutParams(-2,-2);badgeLp.leftMargin=d(8);title.addView(engineBadge,badgeLp);card.addView(title);SettingsUi.gap(card,16);
        status=SettingsUi.text(this,"首次准备需要 Wi-Fi，下载后即可本机翻译。",12,SettingsUi.MUTED,false);status.setId(R.id.download_status);card.addView(status);
        downloadProgress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);downloadProgress.setId(R.id.download_progress);downloadProgress.setMax(100);downloadProgress.setProgressTintList(android.content.res.ColorStateList.valueOf(SettingsUi.ACCENT));downloadProgress.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(SettingsUi.ACCENT));
        var progressLayout=new LinearLayout.LayoutParams(-1,d(8));progressLayout.topMargin=d(12);progressLayout.bottomMargin=d(8);card.addView(downloadProgress,progressLayout);downloadProgress.setVisibility(View.GONE);
        downloadDetail=SettingsUi.text(this,"",12,SettingsUi.ACCENT,true);downloadDetail.setId(R.id.download_detail);downloadDetail.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);downloadDetail.setVisibility(View.GONE);card.addView(downloadDetail);SettingsUi.gap(card,14);
        downloadButton=SettingsUi.button(this,"准备离线语言包",SettingsUi.PALE,SettingsUi.ACCENT,()->new AlertDialog.Builder(this).setTitle("下载本机翻译语言包").setMessage("通过 Google ML Kit 下载中英模型，需要 Wi-Fi。正文和译文在设备上处理，不发送给开发者；SDK 可能联网下载模型、更新以及发送使用与性能指标。").setNegativeButton("取消",null).setPositiveButton("下载",(dialog,which)->beginLanguageDownload()).show());downloadButton.setId(R.id.download_button);card.addView(downloadButton,new LinearLayout.LayoutParams(-1,d(46)));body.addView(card);
    }
    private void createTrial(){
        section("试一试","用一句话，感受浮译");var card=SettingsUi.card(this);
        EditText test=new EditText(this);test.setId(R.id.test_editor);test.setHint("输入 nihao，看看英文…");test.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);test.setMinLines(2);test.setTextSize(16);test.setTextColor(SettingsUi.INK);test.setHintTextColor(0xffA0ACBC);test.setBackground(SettingsUi.shape(this,SettingsUi.BG,13));test.setPadding(d(14),d(12),d(14),d(12));card.addView(test,new LinearLayout.LayoutParams(-1,d(88)));SettingsUi.gap(card,12);
        var sampleRow=new LinearLayout(this);sampleRow.setGravity(Gravity.CENTER_VERTICAL);var sample=SettingsUi.text(this,"你好，今天去北京。",12,SettingsUi.MUTED,false);sample.setTextIsSelectable(true);sampleRow.addView(sample,new LinearLayout.LayoutParams(0,-2,1));
        var copy=SettingsUi.button(this,"复制示例",SettingsUi.PALE,SettingsUi.ACCENT,()->{getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("测试","你好，今天去北京。"));Toast.makeText(this,"示例已复制",Toast.LENGTH_SHORT).show();});copy.setTextSize(13);copy.setPadding(d(8),0,d(8),0);copy.setContentDescription("复制这句测试文字");sampleRow.addView(copy,new LinearLayout.LayoutParams(d(86),d(44)));card.addView(sampleRow);body.addView(card);
    }
    private void createPrivacy(){
        section("隐私与安全","清楚知道数据去向");var card=SettingsUi.card(this);
        SettingsUi.privacyRow(card,"✓","本机加密保存","复制记录仅存于应用私有目录，禁止系统备份。");SettingsUi.gap(card,16);
        SettingsUi.privacyRow(card,"·","敏感输入保护","密码、验证码、密钥建议切回系统键盘。");SettingsUi.gap(card,16);
        SettingsUi.privacyRow(card,"↗","明确网络边界","语言包下载与 SDK 使用、性能指标可能联网。");SettingsUi.divider(card);
        var detail=SettingsUi.text(this,"复制历史使用 Android Keystore 加密，24 小时到期后不再展示或允许插入。休眠、关机或强行停止可能推迟文件清理，再次访问会先过滤到期记录。关闭保存开关会立即清空。\n\n密码框、不允许个性化学习的输入框，以及带敏感标记的剪贴板会跳过。来源应用没有正确标记密码时无法可靠识别。\n\n正文与译文使用本机模型处理，没有开发者服务器或输入历史记录。ML Kit 可能连接 Google 下载、更新模型并发送 SDK 使用与性能指标，不能承诺完全不联网。",12,SettingsUi.MUTED,false);detail.setId(R.id.privacy_details);detail.setVisibility(View.GONE);
        Button expand=SettingsUi.button(this,"查看完整隐私说明  ⌄",Color.TRANSPARENT,SettingsUi.ACCENT,()->{});expand.setId(R.id.privacy_expand);expand.setOnClickListener(v->{boolean show=detail.getVisibility()!=View.VISIBLE;detail.setVisibility(show?View.VISIBLE:View.GONE);expand.setText(show?"收起隐私说明  ⌃":"查看完整隐私说明  ⌄");});card.addView(expand,new LinearLayout.LayoutParams(-1,d(44)));card.addView(detail);body.addView(card);
    }
    private void checkInstalledLanguagePack(){
        com.google.mlkit.common.model.RemoteModelManager.getInstance().getDownloadedModels(com.google.mlkit.nl.translate.TranslateRemoteModel.class).addOnSuccessListener(models->{
            if(!isDestroyed()&&!downloading&&models.stream().anyMatch(model->model.getLanguage().equals(com.google.mlkit.nl.translate.TranslateLanguage.CHINESE))){renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.READY,0,-1));downloadButton.setText("检查语言包");}
        });
    }
    void beginLanguageDownload(){
        if(downloading)return;downloading=true;Options.prefs(this).edit().putBoolean("languageDownloadRequested",true).apply();
        downloadButton.setEnabled(false);downloadButton.setText("正在准备语言包…");
        renderDownloadProgress(new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1));startDownloadMonitor();
        if(translator==null)translator=new LocalTranslator();
        translator.download((text,message)->{
            if(isDestroyed())return;downloading=false;Options.prefs(this).edit().putBoolean("languageDownloadRequested",false).apply();stopDownloadMonitor();
            boolean ready=message.contains("已就绪");
            renderDownloadProgress(new DownloadProgress(ready?DownloadProgress.Stage.READY:DownloadProgress.Stage.FAILED,latestProgress.downloaded(),latestProgress.total()));
            downloadButton.setEnabled(true);downloadButton.setText(ready?"检查语言包":"重试下载语言包");
        });
    }
    void renderDownloadProgress(DownloadProgress value){
        latestProgress=value;status.setText(value.message());
        boolean ready=value.stage()==DownloadProgress.Stage.READY,failed=value.stage()==DownloadProgress.Stage.FAILED;
        String phase=switch(value.stage()){case READY->"已就绪";case FAILED->"待重试";case DOWNLOADING->"下载中";case WAITING_WIFI,WAITING_NETWORK->"等待网络";case RETRYING->"重试中";case VERIFYING->"安装中";default->"准备中";};
        engineBadge.setText(phase);
        if(downloading)downloadButton.setText(value.stage()==DownloadProgress.Stage.DOWNLOADING?"正在下载语言包…":value.stage()==DownloadProgress.Stage.VERIFYING?"正在安装语言包…":"正在准备语言包…");
        engineBadge.setTextColor(ready?SettingsUi.GREEN:SettingsUi.ACCENT);engineBadge.setBackground(SettingsUi.shape(this,ready?0xffE9F6F1:SettingsUi.PALE,20));downloadProgress.setVisibility(View.VISIBLE);downloadDetail.setVisibility(View.VISIBLE);
        downloadProgress.setIndeterminate(!value.determinate()&&value.stage()!=DownloadProgress.Stage.FAILED);
        if(value.determinate())downloadProgress.setProgress(value.percent());
        downloadProgress.setContentDescription("语言包下载："+value.message()+"，"+value.detail());downloadDetail.setText(value.detail());
    }
    private void startDownloadMonitor(){if(downloadMonitor==null){downloadMonitor=new ModelDownloadMonitor(this,value->{if(downloading&&!isDestroyed())renderDownloadProgress(value);});downloadMonitor.start();}}
    private void stopDownloadMonitor(){if(downloadMonitor!=null){downloadMonitor.close();downloadMonitor=null;}}
    @Override protected void onStart(){super.onStart();if(downloading)startDownloadMonitor();}
    @Override protected void onStop(){stopDownloadMonitor();super.onStop();}
    @Override protected void onResume(){super.onResume();Options.prefs(this).registerOnSharedPreferenceChangeListener(optionsChanged);syncOptions();new ClipboardStore(this).list();}
    @Override protected void onPause(){Options.prefs(this).unregisterOnSharedPreferenceChangeListener(optionsChanged);super.onPause();}
    @Override protected void onDestroy(){stopDownloadMonitor();if(translator!=null)translator.close();super.onDestroy();}
}
