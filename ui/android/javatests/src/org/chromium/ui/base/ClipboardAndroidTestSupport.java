// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** JNI methods for {@link ClipboardAndroidTest}. */
@JNINamespace("ui")
public class ClipboardAndroidTestSupport {
    /**
     * Writes HTML to the native side clipboard.
     * @param htmlText the htmlText to write.
     */
    public static boolean writeHtml(String htmlText) {
        return ClipboardAndroidTestSupportJni.get().nativeWriteHtml(htmlText);
    }

    /**
     * Checks that the native side clipboard contains.
     * @param text the expected text.
     */
    public static boolean clipboardContains(String text) {
        return ClipboardAndroidTestSupportJni.get().nativeClipboardContains(text);
    }

    @NativeMethods
    interface Natives {
        boolean nativeWriteHtml(String htmlText);

        boolean nativeClipboardContains(String text);
    }
}
