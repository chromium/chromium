// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Owns the c++ FullscreenCallbackProxy class, which is responsible for forwarding all
 * FullscreenDelegate delegate calls to this class, which in turn forwards to the
 * FullscreenCallbackClient.
 */
@JNINamespace("weblayer")
public final class FullscreenCallbackProxy {
    private long mNativeFullscreenCallbackProxy;
    private IFullscreenCallbackClient mClient;
    private TabImpl mTab;
    private FullscreenToast mToast;
    private boolean mIsNotifyingClientOfEnter;
    // Used so that only the most recent callback supplied to the client is acted on.
    private int mNextFullscreenId;

    FullscreenCallbackProxy(TabImpl tab, long nativeTab, IFullscreenCallbackClient client) {
        assert client != null;
        mClient = client;
        mTab = tab;
        mNativeFullscreenCallbackProxy =
                FullscreenCallbackProxyJni.get().createFullscreenCallbackProxy(this, nativeTab);
    }

    public void setClient(IFullscreenCallbackClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        FullscreenCallbackProxyJni.get().deleteFullscreenCallbackProxy(
                mNativeFullscreenCallbackProxy);
        mNativeFullscreenCallbackProxy = 0;
        destroyToast();
        mTab = null;
    }

    public void destroyToast() {
        if (mToast == null) return;
        mToast.destroy();
        mToast = null;
    }

    @VisibleForTesting
    public boolean didShowFullscreenToast() {
        return mToast != null && mToast.didShowFullscreenToast();
    }

    @CalledByNative
    private void enterFullscreen() throws RemoteException {
        final int id = ++mNextFullscreenId;
        ValueCallback<Void> exitFullscreenCallback = new ValueCallback<Void>() {
            @Override
            public void onReceiveValue(Void result) {
                ThreadUtils.assertOnUiThread();
                if (id != mNextFullscreenId) {
                    // This is an old fullscreen request. Ignore it.
                    return;
                }
                if (mNativeFullscreenCallbackProxy == 0) {
                    throw new IllegalStateException("Called after destroy()");
                }
                if (mIsNotifyingClientOfEnter) {
                    throw new IllegalStateException(
                            "Fullscreen callback must not be called synchronously");
                }
                destroyToast();
                FullscreenCallbackProxyJni.get().doExitFullscreen(mNativeFullscreenCallbackProxy);
            }
        };
        destroyToast();
        mToast = new FullscreenToast(mTab);
        mIsNotifyingClientOfEnter = true;
        try {
            mClient.enterFullscreen(ObjectWrapper.wrap(exitFullscreenCallback));
        } finally {
            mIsNotifyingClientOfEnter = false;
        }
    }

    @CalledByNative
    private void exitFullscreen() throws RemoteException {
        mClient.exitFullscreen();
        destroyToast();
    }

    @NativeMethods
    interface Natives {
        long createFullscreenCallbackProxy(FullscreenCallbackProxy proxy, long tab);
        void deleteFullscreenCallbackProxy(long proxy);
        void doExitFullscreen(long nativeFullscreenCallbackProxy);
    }
}
