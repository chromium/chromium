// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;

@JNINamespace("wolvic")
public class WolvicBrowserContext implements BrowserContextHandle {
    /** Pointer to the Native-side WolvicBrowserContext. */
    private long mNativeWolvicBrowserContext;

    public static BrowserContextHandle fromWebContents(WebContents webContents) {
        return (BrowserContextHandle)
                WolvicBrowserContextJni.get().fromWebContents(webContents);
    }

    public static BrowserContextHandle fromRenderFrameHost(RenderFrameHost rfh) {
        return (BrowserContextHandle)
                WolvicBrowserContextJni.get().fromWebContents(
                        WebContentsStatics.fromRenderFrameHost(rfh));
    }

    private WolvicBrowserContext(long nativeWolvicBrowserContext) {
        mNativeWolvicBrowserContext = nativeWolvicBrowserContext;
    }

    @CalledByNative
    private static WolvicBrowserContext create(long nativeWolvicBrowserContext) {
        return new WolvicBrowserContext(nativeWolvicBrowserContext);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeWolvicBrowserContext = 0;
    }

    @Override
    public long getNativeBrowserContextPointer() {
        return WolvicBrowserContextJni.get().getBrowserContextPointer(
                mNativeWolvicBrowserContext);
    }

    @NativeMethods
    public interface Natives {
        Object fromWebContents(WebContents webContents);
        long getBrowserContextPointer(long nativeWolvicBrowserContext);
    }
}
