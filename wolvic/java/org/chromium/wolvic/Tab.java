// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.WebContents;

@JNINamespace("content")
public class Tab {
    public WebContents createWebContents() {
        return TabJni.get().createWebContents();
    }

    public void setWebContentsDelegate(
            WebContents webContents, WebContentsDelegateAndroid delegate) {
        TabJni.get().setWebContentsDelegate(webContents, delegate);
    }

    @NativeMethods
    public interface Natives {
        WebContents createWebContents();
        void setWebContentsDelegate(WebContents webContents, WebContentsDelegateAndroid delegate);
    }
}
