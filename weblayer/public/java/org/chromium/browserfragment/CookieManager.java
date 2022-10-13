// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.ICookieManagerDelegate;
import org.chromium.browserfragment.interfaces.IStringCallback;

import java.util.HashSet;

/**
 * CookieManager allows you to set/get cookies within the context of the BrowserFragment's
 * profile.
 */
public class CookieManager {
    private ICookieManagerDelegate mDelegate;

    private HashSet<CallbackToFutureAdapter.Completer<Void>> mPendingSetCompleters =
            new HashSet<>();
    private HashSet<CallbackToFutureAdapter.Completer<String>> mPendingGetCompleters =
            new HashSet<>();

    CookieManager(ICookieManagerDelegate delegate) {
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
        if (mDelegate == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("Browser has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            mPendingSetCompleters.add(completer);
            try {
                mDelegate.setCookie(uri, value, new IBooleanCallback.Stub() {
                    @Override
                    public void onResult(boolean result) {
                        if (result) {
                            completer.set(null);
                        } else {
                            // TODO(rayankans): Improve exception reporting.
                            completer.setException(new IllegalArgumentException("Invalid cookie"));
                        }
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
                    new IllegalStateException("Browser has been destroyed"));
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
