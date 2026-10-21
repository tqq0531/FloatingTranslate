package com.fuyi.keyboard;
import android.text.InputType;
import android.view.inputmethod.EditorInfo;
final class InputSafety {
    static boolean privateField(EditorInfo info) {
        if (info == null) return true;
        int type = info.inputType, cls = type & InputType.TYPE_MASK_CLASS, variation = type & InputType.TYPE_MASK_VARIATION;
        return (info.imeOptions & EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING) != 0
                || (cls == InputType.TYPE_CLASS_NUMBER && variation == InputType.TYPE_NUMBER_VARIATION_PASSWORD)
                || (cls == InputType.TYPE_CLASS_TEXT && (variation == InputType.TYPE_TEXT_VARIATION_PASSWORD
                || variation == InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD || variation == InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD));
    }
}
