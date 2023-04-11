// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Callback;
import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ICookieManagerDelegate;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.IWebEngineDelegate;
import org.chromium.webengine.interfaces.IWebFragmentEventsDelegate;

/**
 * WebEngine is created via the WebSandbox and is used to interact with its content as well as to
 * obtain its Fragment.
 */
public class WebEngine {
    @NonNull
    private WebSandbox mWebSandbox;

    @NonNull
    private String mTag;

    @NonNull
    private IWebEngineDelegate mDelegate;

    @NonNull
    private WebFragment mFragment;

    @NonNull
    private TabManager mTabManager;

    @NonNull
    private CookieManager mCookieManager;

    private WebEngine(WebSandbox webSandbox, IWebEngineDelegate delegate,
            IWebFragmentEventsDelegate fragmentEventsDelegate,
            ITabManagerDelegate tabManagerDelegate, ICookieManagerDelegate cookieManagerDelegate,
            String tag) {
        ThreadCheck.ensureOnUiThread();
        mWebSandbox = webSandbox;
        mTag = tag;

        mDelegate = delegate;
        mFragment = new WebFragment();
        try {
            mFragment.initialize(webSandbox, this, fragmentEventsDelegate);
        } catch (RemoteException e) {
        }
        mTabManager = new TabManager(tabManagerDelegate, this);
        mCookieManager = new CookieManager(cookieManagerDelegate);
    }

    static WebEngine create(WebSandbox webSandbox, IWebEngineDelegate delegate,
            IWebFragmentEventsDelegate fragmentEventsDelegate,
            ITabManagerDelegate tabManagerDelegate, ICookieManagerDelegate cookieManagerDelegate,
            String tag) {
        return new WebEngine(webSandbox, delegate, fragmentEventsDelegate, tabManagerDelegate,
                cookieManagerDelegate, tag);
    }

    void initializeTabManager(Callback<Void> initializationFinishedCallback) {
        mTabManager.initialize(initializationFinishedCallback);
    }

    /**
     * Returns the TabManager.
     */
    @NonNull
    public TabManager getTabManager() {
        return mTabManager;
    }

    /**
     * Returns the CookieManager.
     */
    @NonNull
    public CookieManager getCookieManager() {
        return mCookieManager;
    }

    void updateFragment(WebFragment fragment) {
        mFragment = fragment;
    }

    private final class RequestNavigationCallback extends IBooleanCallback.Stub {
        private CallbackToFutureAdapter.Completer<Boolean> mCompleter;

        RequestNavigationCallback(CallbackToFutureAdapter.Completer<Boolean> completer) {
            mCompleter = completer;
        }

        @Override
        public void onResult(boolean didNavigate) {
            mCompleter.set(didNavigate);
        }
        @Override
        public void onException(@ExceptionType int type, String msg) {
            mCompleter.setException(ExceptionHelper.createException(type, msg));
        }
    }

    /**
     * Tries to navigate back inside the WebEngine session and returns a Future with a Boolean
     * which is true if the back navigation was successful.
     *
     * Only recommended to use if no switching of Tabs is used.
     *
     * Navigates back inside the currently active tab if possible. If that is not possible,
     * checks if any Tab was added to the WebEngine before the currently active Tab,
     * if so, the currently active Tab is closed and this Tab is set to active.
     */
    @NonNull
    public ListenableFuture<Boolean> tryNavigateBack() {
        ThreadCheck.ensureOnUiThread();
        if (mDelegate == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            mDelegate.tryNavigateBack(new RequestNavigationCallback(completer));
            // Debug string.
            return "Did navigate back Future";
        });
    }

    /**
     * Returns the WebFragment.
     */
    @NonNull
    public WebFragment getFragment() {
        return mFragment;
    }

    /**
     * Returns the tag associated with this WebEngine instance.
     */
    @NonNull
    public String getTag() {
        return mTag;
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
            mWebSandbox.removeWebEngine(mTag, this);
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