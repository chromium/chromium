// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * JNI methods for {@link ClipboardAndroidTest}.
 */
@JNINamespace("ui")
public class ClipboardAndroidTestSupport {
    /**
     * Cleans up clipboard on native side.
     */
    public static void cleanup() {
        ClipboardAndroidTestSupportJni.get().cleanup();
    }

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
        void cleanup();
        boolean nativeWriteHtml(String htmlText);
        boolean nativeClipboardContains(String text);
    }
}
