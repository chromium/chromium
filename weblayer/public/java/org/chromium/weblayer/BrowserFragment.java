// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProvider;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
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
        Bundle args = getArguments();
        if (args == null) {
            throw new RuntimeException("BrowserFragment was created without arguments.");
        }
        // If there is saved state, then it should be used and this method should not be called.
        assert !(new ViewModelProvider(this)).get(BrowserViewModel.class).hasSavedState();
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
            mImpl = mWebLayer.connectFragment(getRemoteFragmentClient(), args);
            return mImpl.asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    @Override
    public void onAttach(Context context) {
        ThreadCheck.ensureOnUiThread();
        BrowserViewModel browserViewModel = new ViewModelProvider(this).get(BrowserViewModel.class);
        if (browserViewModel.hasSavedState()) {
            configureFromViewModel(browserViewModel);
        }
        super.onAttach(context);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mBrowser != null) {
            // If mBrowser is non-null, it means mBrowser came from a ViewModel.
            return;
        }
        try {
            mBrowser = new Browser(mImpl.getBrowser(), this);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        if (useViewModel()) {
            saveToViewModel(new ViewModelProvider(this).get(BrowserViewModel.class));
        }
    }

    @Override
    @SuppressWarnings("ReferenceEquality")
    public void onDestroy() {
        ThreadCheck.ensureOnUiThread();
        // If a ViewModel is used, then the Browser is destroyed from the ViewModel, not here
        // (RemoteFragment won't call destroy on Browser either in this case).
        if (useViewModel()) {
            if (mBrowser.getFragment() == this) {
                // The browser is no long associated with this fragment. Null out the reference to
                // ensure BrowserFragment can be gc'd.
                mBrowser.setFragment(null);
            }
            super.onDestroy();
        } else {
            mBrowser.prepareForDestroy();
            super.onDestroy();
            mBrowser.onDestroyed();
        }
        mBrowser = null;
    }

    @Override
    void saveToViewModel(@NonNull ViewModelImpl model) {
        super.saveToViewModel(model);

        BrowserViewModel browserViewModel = (BrowserViewModel) model;
        browserViewModel.mImpl = mImpl;
        browserViewModel.mWebLayer = mWebLayer;
        browserViewModel.mBrowser = mBrowser;
    }

    @Override
    void configureFromViewModel(@NonNull ViewModelImpl model) {
        super.configureFromViewModel(model);

        BrowserViewModel browserViewModel = (BrowserViewModel) model;
        mBrowser = browserViewModel.mBrowser;
        mWebLayer = browserViewModel.mWebLayer;
        mImpl = browserViewModel.mImpl;
        mBrowser.setFragment(this);
    }

    private boolean useViewModel() {
        Bundle args = getArguments();
        return args == null ? false : args.getBoolean(BrowserFragmentArgs.USE_VIEW_MODEL, false);
    }

    /**
     * This class is an implementation detail and not intended for public use. It may change at any
     * time in incompatible ways, including being removed.
     * <p>
     * This class stores BrowserFragment specific state to a ViewModel so that it can reused if a
     * new Fragment is created that should share the same state. See RemoteFragment for details on
     * this.
     */
    public static final class BrowserViewModel extends RemoteFragment.ViewModelImpl {
        @Nullable
        private IBrowserFragment mImpl;
        @Nullable
        private WebLayer mWebLayer;
        @Nullable
        private Browser mBrowser;

        boolean hasSavedState() {
            return mBrowser != null;
        }

        @Override
        protected void onCleared() {
            ThreadCheck.ensureOnUiThread();
            if (mBrowser != null) {
                mBrowser.prepareForDestroy();
                super.onCleared();
                mBrowser.onDestroyed();
                mBrowser = null;
            } else {
                super.onCleared();
            }
        }
    }
}
