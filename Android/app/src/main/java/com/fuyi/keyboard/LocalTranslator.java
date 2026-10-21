package com.fuyi.keyboard;
import com.google.mlkit.nl.translate.*;
import com.google.mlkit.common.model.*;
import com.google.mlkit.common.model.DownloadConditions;

final class LocalTranslator implements AutoCloseable {
    private boolean closed;
    private final Translator client = Translation.getClient(new TranslatorOptions.Builder()
            .setSourceLanguage(TranslateLanguage.CHINESE).setTargetLanguage(TranslateLanguage.ENGLISH).build());
    private final RemoteModelManager models = RemoteModelManager.getInstance();
    interface Result { void complete(String text, String status); }
    void translate(String text, Result result) {
        if (closed) return;
        models.getDownloadedModels(TranslateRemoteModel.class).addOnSuccessListener(available -> {
            if (closed) return;
            boolean chinese = available.stream().anyMatch(m -> m.getLanguage().equals(TranslateLanguage.CHINESE));
            if (!chinese) { result.complete("", "请在设置下载中英离线语言包"); return; }
            client.translate(text).addOnSuccessListener(value -> result.complete(value, ""))
                    .addOnFailureListener(e -> result.complete("", "本机翻译暂不可用，请检查语言包"));
        }).addOnFailureListener(e -> result.complete("", "无法检查语言包"));
    }
    void download(Result result) {
        client.downloadModelIfNeeded(new DownloadConditions.Builder().requireWifi().build())
                .addOnSuccessListener(v -> result.complete("", "语言包已就绪，可以断网翻译"))
                .addOnFailureListener(e -> result.complete("", "下载未完成，请检查 Wi-Fi 后重试"));
    }
    @Override public void close() { closed=true; client.close(); }
}
