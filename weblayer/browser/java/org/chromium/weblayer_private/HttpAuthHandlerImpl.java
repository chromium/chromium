// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.http_auth.LoginPrompt;
import org.chromium.url.GURL;

/**
 * Handles showing http auth prompt.
 */
@JNINamespace("weblayer")
public final class HttpAuthHandlerImpl implements LoginPrompt.Observer {
    private long mNativeHttpAuthHandlerImpl;
    private LoginPrompt mLoginPrompt;

    @CalledByNative
    public static HttpAuthHandlerImpl create(long nativeAuthHandler, TabImpl tab, GURL url) {
        return new HttpAuthHandlerImpl(nativeAuthHandler, tab.getBrowser().getContext(), url);
    }

    @CalledByNative
    void handlerDestroyed() {
        mNativeHttpAuthHandlerImpl = 0;
    }

    @CalledByNative
    private void closeDialog() {
        if (mLoginPrompt != null) mLoginPrompt.dismiss();
    }

    private HttpAuthHandlerImpl(long nativeHttpAuthHandlerImpl, Context context, GURL url) {
        mNativeHttpAuthHandlerImpl = nativeHttpAuthHandlerImpl;

        mLoginPrompt = new LoginPrompt(context, url.getHost(), url, this);
        mLoginPrompt.show();
    }

    @Override
    public void proceed(String username, String password) {
        if (mNativeHttpAuthHandlerImpl != 0) {
            HttpAuthHandlerImplJni.get().proceed(mNativeHttpAuthHandlerImpl, username, password);
        }
    }

    @Override
    public void cancel() {
        if (mNativeHttpAuthHandlerImpl != 0) {
            HttpAuthHandlerImplJni.get().cancel(mNativeHttpAuthHandlerImpl);
        }
    }

    @NativeMethods
    interface Natives {
        void proceed(long nativeHttpAuthHandlerImpl, String username, String password);
        void cancel(long nativeHttpAuthHandlerImpl);
    }
}
