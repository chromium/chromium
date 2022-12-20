// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.RemoteException;

import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.IWebEngineDelegate;
import org.chromium.webengine.interfaces.IWebFragmentEventsDelegate;

/**
 * WebEngine is created via the WebSandbox and is used to interact with its content as well as to
 * obtain its Fragment.
 */
public class WebEngine {
    private WebSandbox mWebSandbox;

    private IWebEngineDelegate mDelegate;

    private WebFragment mFragment;

    private TabManager mTabManager;
    private CookieManager mCookieManager;

    private WebEngine(WebSandbox webSandbox, IWebEngineDelegate delegate,
            IWebFragmentEventsDelegate fragmentEventsDelegate,
            ITabManagerDelegate tabManagerDelegate, ICookieManagerDelegate cookieManagerDelegate) {
        ThreadCheck.ensureOnUiThread();
        mWebSandbox = webSandbox;

        mDelegate = delegate;
        mFragment = new WebFragment();
        try {
            mFragment.initialize(webSandbox, this, fragmentEventsDelegate);
        } catch (RemoteException e) {
        }
        mTabManager = new TabManager(tabManagerDelegate);
        mCookieManager = new CookieManager(cookieManagerDelegate);
    }

    static WebEngine create(WebSandbox webSandbox, IWebEngineDelegate delegate,
            IWebFragmentEventsDelegate fragmentEventsDelegate,
            ITabManagerDelegate tabManagerDelegate, ICookieManagerDelegate cookieManagerDelegate) {
        return new WebEngine(webSandbox, delegate, fragmentEventsDelegate, tabManagerDelegate,
                cookieManagerDelegate);
    }

    /**
     * Returns the TabManager.
     *
     * @return TabManager Tab manager to interact with tabs.
     */
    public TabManager getTabManager() {
        return mTabManager;
    }

    /**
     * Returns the CookieManager.
     *
     * @return CookiesManager CookieManager to interact with cookies of the associated Profile.
     */
    public CookieManager getCookieManager() {
        return mCookieManager;
    }

    void updateFragment(WebFragment fragment) {
        mFragment = fragment;
    }

    /**
     * Returns the Fragment.
     *
     * @return WebFragment Fragment that can be inflated in an Activity.
     */
    public WebFragment getFragment() {
        ThreadCheck.ensureOnUiThread();
        return mFragment;
    }

    void invalidate() {
        ThreadCheck.ensureOnUiThread();
        if (mTabManager != null) {
            mTabManager.invalidate();
            mTabManager = null;
        }

        if (mCookieManager != null) {
            mCookieManager.invalidate();
            mCookieManager = null;
        }
        if (mFragment != null) {
            mFragment.invalidate();
            mFragment = null;
        }

        if (mDelegate != null) {
            try {
                mDelegate.shutdown();
            } catch (RemoteException e) {
            }
            mDelegate = null;
        }

        if (mWebSandbox != null && !mWebSandbox.isShutdown()) {
            mWebSandbox.removeWebEngine(this);
        }
        mWebSandbox = null;
    }

    /**
     * Close this WebEngine.
     */
    public void close() {
        invalidate();
    }
}