// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * WebLayer's browser fragment implementation.
 *
 * This class is an implementation detail and will eventually be hidden. Use
 * {@link Browser#fromFragment} to get the Browser from a
 * Fragment created by WebLayer.
 *
 * All the browser APIs, such as loading pages can be accessed via
 * {@link Browser}, which can be retrieved with {@link
 * FragmentSupport#getBrowserForFragment} after the fragment received onCreate the
 * call.
 *
 * Attaching a BrowserFragment to an Activity requires WebLayer to be initialized, so
 * BrowserFragment will block the thread in onAttach until it's done. To prevent this,
 * asynchronously "pre-warm" WebLayer using {@link WebLayer#loadAsync} prior to using
 * BrowserFragments.
 *
 * Unfortunately, when the system restores the BrowserFragment after killing the process, it
 * attaches the fragment immediately on activity's onCreate event, so there is currently no way to
 * asynchronously init WebLayer in that case.
 */
public final class BrowserFragment extends RemoteFragment {
    // Nonnull after first onAttach().
    private IBrowserFragment mImpl;
    private WebLayer mWebLayer;

    // Nonnull between onCreate() and onDestroy().
    private Browser mBrowser;

    /**
     * This constructor is for the system FragmentManager only. Please use
     * {@link WebLayer#createBrowserFragment}.
     */
    public BrowserFragment() {
        super();
    }

    /**
     * Returns the {@link Browser} associated with this fragment.
     * The browser is available only between BrowserFragment's onCreate() and onDestroy().
     */
    @NonNull
    Browser getBrowser() {
        ThreadCheck.ensureOnUiThread();
        if (mBrowser == null) {
            throw new RuntimeException("Browser is available only between "
                    + "BrowserFragment's onCreate() and onDestroy().");
        }
        return mBrowser;
    }

    @Override
    protected IRemoteFragment createRemoteFragment(Context appContext) {
        try {
            Bundle args = getArguments();
            if (args == null) {
                throw new RuntimeException("BrowserFragment was created without arguments.");
            }
            mWebLayer = WebLayer.loadSync(appContext);
            mImpl = mWebLayer.connectFragment(getRemoteFragmentClient(), args);
            return mImpl.asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            mBrowser = new Browser(mImpl.getBrowser(), this);
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
