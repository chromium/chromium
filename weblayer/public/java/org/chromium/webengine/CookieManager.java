// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.IStringCallback;

import java.util.HashSet;

/**
 * CookieManager allows you to set/get cookies within the context of the WebFragment's
 * profile.
 */
public class CookieManager {
    @NonNull
    private ICookieManagerDelegate mDelegate;

    @NonNull
    private HashSet<CallbackToFutureAdapter.Completer<Void>> mPendingSetCompleters =
            new HashSet<>();

    @NonNull
    private HashSet<CallbackToFutureAdapter.Completer<String>> mPendingGetCompleters =
            new HashSet<>();

    CookieManager(@NonNull ICookieManagerDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Sets a cookie for the given URL.
     *
     * @param uri the URI for which the cookie is to be set.
     * @param value the cookie string, using the format of the 'Set-Cookie' HTTP response header.
     */
    @NonNull
    public ListenableFuture<Void> setCookie(@NonNull String uri, @NonNull String value) {
        ThreadCheck.ensureOnUiThread();

        if (mDelegate == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            mPendingSetCompleters.add(completer);
            try {
                mDelegate.setCookie(uri, value, new IBooleanCallback.Stub() {
                    @Override
                    public void onResult(boolean set) {
                        if (!set) {
                            completer.setException(
                                    new IllegalArgumentException("Cookie not set: " + value));
                        }
                        completer.set(null);
                        mPendingSetCompleters.remove(completer);
                    }
                    @Override
                    public void onException(@ExceptionType int type, String msg) {
                        completer.setException(ExceptionHelper.createException(type, msg));
                        mPendingSetCompleters.remove(completer);
                    }
                });
            } catch (RemoteException e) {
                completer.setException(e);
                mPendingSetCompleters.remove(completer);
            }
            // Debug string.
            return "CookieManager.setCookie Future";
        });
    }

    /**
     * Gets the cookies for the given URL.
     *
     * @param uri the URI for which the cookie is to be set.
     */
    @NonNull
    public ListenableFuture<String> getCookie(@NonNull String uri) {
        if (mDelegate == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            mPendingGetCompleters.add(completer);
            try {
                mDelegate.getCookie(uri, new IStringCallback.Stub() {
                    @Override
                    public void onResult(String result) {
                        if (result != null) {
                            completer.set(result);
                        } else {
                            // TODO(rayankans): Improve exception reporting.
                            completer.setException(
                                    new IllegalArgumentException("Failed to get cookie"));
                        }
                        mPendingGetCompleters.remove(completer);
                    }
                    @Override
                    public void onException(@ExceptionType int type, String msg) {
                        completer.setException(ExceptionHelper.createException(type, msg));
                        mPendingGetCompleters.remove(completer);
                    }
                });
            } catch (RemoteException e) {
                completer.setException(e);
                mPendingGetCompleters.remove(completer);
            }

            // Debug string.
            return "CookieManager.getCookie Future";
        });
    }

    void invalidate() {
        mDelegate = null;
        for (var completer : mPendingSetCompleters) {
            completer.setCancelled();
        }
        for (var completer : mPendingGetCompleters) {
            completer.setCancelled();
        }
        mPendingSetCompleters.clear();
        mPendingGetCompleters.clear();
    }

    // TODO(rayankans): Add a Cookie Observer.
}
