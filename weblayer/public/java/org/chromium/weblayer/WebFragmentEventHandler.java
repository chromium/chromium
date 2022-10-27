// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 *
 */
final class WebFragmentEventHandler extends RemoteFragmentEventHandler {
    // Nonnull after first onAttach().
    private IBrowserFragment mImpl;
    private WebLayer mWebLayer;

    // Nonnull between onCreate() and onDestroy().
    private Browser mBrowser;

    public WebFragmentEventHandler(Bundle args) {
        super(args);
        assert args != null;
    }

    /**
     * Returns the {@link Browser} associated with this fragment.
     * The browser is available only between WebFragment's onCreate() and onDestroy().
     */
    @NonNull
    Browser getBrowser() {
        ThreadCheck.ensureOnUiThread();
        if (mBrowser == null) {
            throw new RuntimeException("Browser is available only between "
                    + "WebFragment's onCreate() and onDestroy().");
        }
        return mBrowser;
    }

    @Override
    protected IRemoteFragment createRemoteFragmentEventHandler(Context appContext) {
        Bundle args = getArguments();
        try {
            mWebLayer = WebLayer.loadSync(appContext);
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
        // Ideally this would be in WebLayer when the fragment is created, but at that time we don't
        // want to trigger loading WebLayer.
        if (args.getBoolean(BrowserFragmentArgs.IS_INCOGNITO, false)) {
            String name = args.getString(BrowserFragmentArgs.PROFILE_NAME);
        }
        try {
            mImpl = mWebLayer.connectFragment(args);
            return mImpl.asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    void onCreate(Bundle savedInstanceState, @Nullable TabListCallback tabListCallback) {
        if (mBrowser != null) {
            // If mBrowser is non-null, it means mBrowser came from a ViewModel.
            return;
        }

        super.onCreate(savedInstanceState);
        try {
            mBrowser = new Browser(mImpl.getBrowser(), tabListCallback);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public void onDestroy() {
        ThreadCheck.ensureOnUiThread();
        mBrowser.prepareForDestroy();
        super.onDestroy();
        mBrowser.onDestroyed();
        mBrowser = null;
    }
}
