// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.interfaces.IProfileManagerDelegate;
import org.chromium.webengine.interfaces.IStringListCallback;

import java.util.List;

/**
 * Manages Profiles in a WebSandbox.
 */
public class ProfileManager {
    @NonNull
    private IProfileManagerDelegate mDelegate;

    ProfileManager(IProfileManagerDelegate delegate) {
        mDelegate = delegate;
    }

    void invalidate() {
        mDelegate = null;
    }

    /**
     * Returns a ListenableFuture that resolves to a List of Profile names.
     */
    public ListenableFuture<List<String>> getAllProfileNames() {
        ThreadCheck.ensureOnUiThread();

        return CallbackToFutureAdapter.getFuture(completer -> {
            mDelegate.getAllProfileNames(new IStringListCallback.Stub() {
                @Override
                public void onResult(List<String> profileNames) {
                    completer.set(profileNames);
                }

                @Override
                public void onException(int type, String msg) {
                    completer.setException(ExceptionHelper.createException(type, msg));
                }
            });

            return "Profile Names Future";
        });
    }
}
