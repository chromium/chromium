// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import android.graphics.Rect;
import android.graphics.RectF;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindNotificationDetails;
import org.chromium.content_public.browser.WebContents;

@JNINamespace("wolvic")
public abstract class WolvicWebContentsDelegate extends WebContentsDelegateAndroid {
    @CalledByNative
    public abstract void onCreateNewWindow(WebContents webContents);

    @CalledByNative
    public abstract void onWebAppManifest(WebContents webContents, @NonNull String manifest);

    // Find in page callback methods.
    @CalledByNative
    protected abstract void onFindResultAvailable(FindNotificationDetails result);

    // Helper methods used by the native code to create types in the API.
    @CalledByNative
    private static Rect createRect(int x, int y, int right, int bottom) {
        return new Rect(x, y, right, bottom);
    }

    @CalledByNative
    private static FindNotificationDetails createFindNotificationDetails(
            int numberOfMatches,
            Rect rendererSelectionRect,
            int activeMatchOrdinal,
            boolean finalUpdate) {
        return new FindNotificationDetails(
                numberOfMatches, rendererSelectionRect, activeMatchOrdinal, finalUpdate);
    }
}
