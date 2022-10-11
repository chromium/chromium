// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.graphics.Bitmap;
import android.os.RemoteException;

import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IFaviconFetcher;
import org.chromium.weblayer_private.interfaces.IFaviconFetcherClient;
import org.chromium.weblayer_private.interfaces.ITab;

/**
 * {@link FaviconFetcher} is responsible for downloading a favicon for the current navigation.
 * {@link FaviconFetcher} maintains an on disk cache of favicons downloading favicons as necessary.
 */
class FaviconFetcher {
    private final FaviconCallback mCallback;
    private IFaviconFetcher mImpl;
    private @Nullable Bitmap mBitmap;

    // Constructor for test mocking.
    protected FaviconFetcher() {
        mCallback = null;
    }

    FaviconFetcher(ITab iTab, FaviconCallback callback) {
        mCallback = callback;
        try {
            mImpl = iTab.createFaviconFetcher(new FaviconFetcherClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Destroys this FaviconFetcher. The callback will no longer be notified. This is implicitly
     * called when the Tab is destroyed.
     */
    public void destroy() {
        ThreadCheck.ensureOnUiThread();
        // As the implementation may implicitly destroy this, allow destroy() to be called multiple
        // times.
        if (mImpl == null) return;
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the favicon for the current navigation. This returns a null Bitmap if the favicon
     * isn't available yet (which includes no navigation).
     *
     * @return The favicon.
     */
    public @Nullable Bitmap getFaviconForCurrentNavigation() {
        ThreadCheck.ensureOnUiThread();
        return mBitmap;
    }

    private final class FaviconFetcherClientImpl extends IFaviconFetcherClient.Stub {
        @Override
        public void onDestroyed() {
            mImpl = null;
        }

        @Override
        public void onFaviconChanged(Bitmap bitmap) {
            mBitmap = bitmap;
            mCallback.onFaviconChanged(bitmap);
        }
    }
}
