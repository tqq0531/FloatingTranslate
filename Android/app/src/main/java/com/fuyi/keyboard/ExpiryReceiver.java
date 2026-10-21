package com.fuyi.keyboard;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class ExpiryReceiver extends BroadcastReceiver {
    static final String ACTION_EXPIRE="com.fuyi.keyboard.EXPIRE_HISTORY";
    @Override public void onReceive(Context context,Intent intent) {
        String action=intent.getAction();
        if (ACTION_EXPIRE.equals(action)||Intent.ACTION_BOOT_COMPLETED.equals(action)||Intent.ACTION_TIME_CHANGED.equals(action)) new ClipboardStore(context).list();
    }
}
