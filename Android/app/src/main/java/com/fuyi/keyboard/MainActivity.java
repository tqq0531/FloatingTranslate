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
    private TextView status,downloadDetail;
    private ProgressBar downloadProgress;
    private Button downloadButton;
    private ModelDownloadMonitor downloadMonitor;
    private boolean downloading;
    private DownloadProgress latestProgress=new DownloadProgress(DownloadProgress.Stage.PREPARING,0,-1);
    private LinearLayout body;
    private Switch historySwitch, englishSwitch;
    private final android.content.SharedPreferences.OnSharedPreferenceChangeListener optionsChanged=(prefs,key)->syncOptions();
    private void syncOptions(){if(historySwitch!=null)historySwitch.setChecked(Options.history(this));if(englishSwitch!=null)englishSwitch.setChecked(Options.prefs(this).getBoolean("englishDefault",false));}
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);
        body=Ui.column(this);body.setPadding(Ui.dp(this,24),Ui.dp(this,20),Ui.dp(this,24),Ui.dp(this,24));body.setBackgroundColor(Ui.BG);
        scroll.setOnApplyWindowInsetsListener((v,insets) -> {int top, bottom; if(android.os.Build.VERSION.SDK_INT>=30){var bars=insets.getInsets(WindowInsets.Type.systemBars());top=bars.top;bottom=bars.bottom;}else{top=insets.getSystemWindowInsetTop();bottom=insets.getSystemWindowInsetBottom();}v.setPadding(0,top,0,bottom);return insets;});
        scroll.addView(body);setContentView(scroll);
        add("浮译",32,Ui.INK);add("ANDROID · 0.1.2 本地试用版",12,Ui.BLUE);
        add("输入中文，看见整句英文",20,Ui.INK);
        add("拼音选词、English 直接输入，以及只保存 24 小时的复制历史。",14,Ui.MUTED);
        body.addView(Ui.button(this,"1  启用浮译键盘",() -> startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS))));
        body.addView(Ui.button(this,"2  切换到浮译",() -> getSystemService(InputMethodManager.class).showInputMethodPicker()));
        add("复制历史",19,Ui.INK);
        Switch history=new Switch(this);historySwitch=history;history.setText("保存剪贴板历史 · 24 小时");history.setTextColor(Ui.INK);history.setChecked(Options.history(this));
        history.setOnCheckedChangeListener((v,enabled) -> {if(enabled!=Options.history(this))Options.history(this,enabled);});body.addView(history);
        add("开启后开始收集复制的文字。点键盘“剪贴板”可选用、删除或清空；关闭开关会立即清空。最多 100 条，每条不超过 10,000 字符，不保存图片。",13,Ui.MUTED);
        body.addView(Ui.button(this,"清空复制历史",() -> new AlertDialog.Builder(this).setTitle("清空全部历史？").setMessage("会删除浮译保存的全部复制记录，系统当前剪贴板不受影响。")
                .setNegativeButton("取消",null).setPositiveButton("清空",(d,w) -> {new ClipboardStore(this).clear();Toast.makeText(this,"历史已清空",Toast.LENGTH_SHORT).show();}).show()));
        Switch english=new Switch(this);englishSwitch=english;english.setText("拼音转换后默认输出英文");english.setTextColor(Ui.INK);english.setChecked(Options.prefs(this).getBoolean("englishDefault",false));
        english.setOnCheckedChangeListener((v,on) -> Options.prefs(this).edit().putBoolean("englishDefault",on).apply());body.addView(english);
        add("前文已按中文上屏时，请长按译文复制整句；不会擅自替换前文。",13,Ui.MUTED);
        add("本机翻译",19,Ui.INK);
        status=Ui.text(this,"下载中英语言包后可在本机翻译。",14,Ui.MUTED);status.setId(R.id.download_status);body.addView(status);
        downloadProgress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);downloadProgress.setId(R.id.download_progress);downloadProgress.setMax(100);downloadProgress.setProgressTintList(android.content.res.ColorStateList.valueOf(Ui.BLUE));downloadProgress.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(Ui.BLUE));
        LinearLayout.LayoutParams progressLayout=new LinearLayout.LayoutParams(-1,Ui.dp(this,8));progressLayout.topMargin=Ui.dp(this,10);progressLayout.bottomMargin=Ui.dp(this,8);body.addView(downloadProgress,progressLayout);downloadProgress.setVisibility(View.GONE);
        downloadDetail=Ui.text(this,"",13,Ui.BLUE);downloadDetail.setId(R.id.download_detail);downloadDetail.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);downloadDetail.setVisibility(View.GONE);body.addView(downloadDetail);
        downloadButton=Ui.button(this,"通过 Wi-Fi 准备离线语言包",()->new AlertDialog.Builder(this).setTitle("下载本机翻译语言包")
                .setMessage("通过 Google ML Kit 下载中英模型，需要 Wi-Fi。正文和译文在设备上处理，不发送给开发者；SDK 可能联网下载模型、更新以及发送使用与性能指标。")
                .setNegativeButton("取消",null).setPositiveButton("下载",(d,w)->beginLanguageDownload()).show());downloadButton.setId(R.id.download_button);body.addView(downloadButton);
        if(Options.prefs(this).getBoolean("languageDownloadRequested",false))beginLanguageDownload();
        add("试一试",19,Ui.INK);
        EditText test=new EditText(this);test.setId(R.id.test_editor);test.setHint("输入 nihao，空格确认中文");test.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_MULTI_LINE);test.setMinLines(2);test.setTextSize(18);test.setBackground(Ui.round(Color.WHITE,12,this));test.setPadding(20,16,20,16);body.addView(test,new LinearLayout.LayoutParams(-1,Ui.dp(this,100)));
        TextView sample=Ui.text(this,"测试复制：你好，今天去北京。",15,Ui.INK);sample.setTextIsSelectable(true);sample.setPadding(0,Ui.dp(this,12),0,Ui.dp(this,12));body.addView(sample);
        body.addView(Ui.button(this,"复制这句测试文字",() -> getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("测试", "你好，今天去北京。"))));
        add("隐私与密码",19,Ui.INK);
        add("复制历史使用 Android Keystore 加密，保存在应用私有目录，禁止系统备份。到期立即不再展示或选用，系统清理任务可能因休眠稍后运行；打开应用或键盘时也会清理。\n\n密码框、不允许个性化学习的输入框，以及带敏感标记的剪贴板会跳过。第三方应用未标记密码时无法可靠识别，输入密码、验证码或密钥请切回系统键盘。\n\n没有开发者服务器和输入历史记录。安卓与 Mac 的网络边界不同：ML Kit 会连接 Google 下载或更新模型，并发送 SDK 使用与性能指标。",13,Ui.MUTED);
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
        latestProgress=value;status.setText(value.message());downloadProgress.setVisibility(View.VISIBLE);downloadDetail.setVisibility(View.VISIBLE);
        downloadProgress.setIndeterminate(!value.determinate()&&value.stage()!=DownloadProgress.Stage.FAILED);
        if(value.determinate())downloadProgress.setProgress(value.percent());
        downloadProgress.setContentDescription("语言包下载："+value.message()+"，"+value.detail());downloadDetail.setText(value.detail());
    }
    private void startDownloadMonitor(){if(downloadMonitor==null){downloadMonitor=new ModelDownloadMonitor(this,value->{if(downloading&&!isDestroyed())renderDownloadProgress(value);});downloadMonitor.start();}}
    private void stopDownloadMonitor(){if(downloadMonitor!=null){downloadMonitor.close();downloadMonitor=null;}}
    @Override protected void onStart(){super.onStart();if(downloading)startDownloadMonitor();}
    @Override protected void onStop(){stopDownloadMonitor();super.onStop();}
    private void add(String value,int size,int color){TextView t=Ui.text(this,value,size,color);t.setPadding(0,Ui.dp(this,14),0,Ui.dp(this,10));body.addView(t);}
    @Override protected void onResume(){super.onResume();Options.prefs(this).registerOnSharedPreferenceChangeListener(optionsChanged);syncOptions();new ClipboardStore(this).list();}
    @Override protected void onPause(){Options.prefs(this).unregisterOnSharedPreferenceChangeListener(optionsChanged);super.onPause();}
    @Override protected void onDestroy(){stopDownloadMonitor();if(translator!=null)translator.close();super.onDestroy();}
}
