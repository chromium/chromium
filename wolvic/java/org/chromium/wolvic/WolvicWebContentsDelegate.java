// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.WebContents;

@JNINamespace("wolvic")
public abstract class WolvicWebContentsDelegate extends WebContentsDelegateAndroid {
    @CalledByNative
    public abstract void onCreateNewWindow(WebContents webContents);

    @CalledByNative
    public abstract void onWebAppManifest(WebContents webContents, @NonNull String manifest);
}
