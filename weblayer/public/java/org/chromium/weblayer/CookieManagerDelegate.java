// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.ICookieManagerDelegate;
import org.chromium.browserfragment.interfaces.IStringCallback;

/**
 * This class acts as a proxy between the embedding app's BrowserFragment and
 * the WebLayer implementation.
 */
class CookieManagerDelegate extends ICookieManagerDelegate.Stub {
    private CookieManager mCookieManager;
    private Handler mHandler = new Handler(Looper.getMainLooper());

    CookieManagerDelegate(CookieManager cookieManager) {
        mCookieManager = cookieManager;
    }

    @Override
    public void setCookie(String uri, String value, IBooleanCallback callback) {
        mHandler.post(() -> {
            mCookieManager.setCookie(Uri.parse(uri), value, (Boolean v) -> {
                try {
                    callback.onResult(v);
                } catch (RemoteException e) {
                }
            });
        });
    }

    @Override
    public void getCookie(String uri, IStringCallback callback) {
        mHandler.post(() -> {
            mCookieManager.getCookie(Uri.parse(uri), (String result) -> {
                try {
                    callback.onResult(result);
                } catch (RemoteException e) {
                }
            });
        });
    }
}
