// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.view.View;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.Set;

/**
 * Browser contains any number of Tabs, with one active Tab. The active Tab is visible to the user,
 * all other Tabs are hidden.
 *
 * By default Browser has a single active Tab.
 */
public class Browser {
    // Set to null once destroyed (or for tests).
    private IBrowser mImpl;
    private BrowserFragment mFragment;
    private final ObserverList<TabListCallback> mTabListCallbacks;
    private final UrlBarController mUrlBarController;

    private final ObserverList<BrowserRestoreCallback> mBrowserRestoreCallbacks;

    // Constructor for test mocking.
    protected Browser() {
        mImpl = null;
        mTabListCallbacks = null;
        mUrlBarController = null;
        mBrowserRestoreCallbacks = null;
    }

    Browser(IBrowser impl, BrowserFragment fragment) {
        mImpl = impl;
        mFragment = fragment;
        mTabListCallbacks = new ObserverList<TabListCallback>();
        mBrowserRestoreCallbacks = new ObserverList<BrowserRestoreCallback>();

        try {
            mImpl.setClient(new BrowserClientImpl());
            mUrlBarController = new UrlBarController(mImpl.getUrlBarController());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void throwIfDestroyed() {
        if (mImpl == null) {
            throw new IllegalStateException("Browser can not be used once destroyed");
        }
    }

    /**
     * Returns the Browser for the supplied Fragment; null if
     * {@link fragment} was not created by WebLayer.
     *
     * @return the Browser
     */
    @Nullable
    public static Browser fromFragment(@Nullable Fragment fragment) {
        return fragment instanceof BrowserFragment ? ((BrowserFragment) fragment).getBrowser()
                                                   : null;
    }

    // Called prior to notifying IBrowser of destroy().
    void prepareForDestroy() {
        mFragment = null;
        for (TabListCallback callback : mTabListCallbacks) {
            callback.onWillDestroyBrowserAndAllTabs();
        }

        // See comment in Tab$TabClientImpl.onTabDestroyed for details on this.
        if (WebLayer.getSupportedMajorVersionInternal() >= 87) return;

        for (Tab tab : getTabs()) {
            Tab.unregisterTab(tab);
        }
    }

    // Called after the browser was destroyed.
    void onDestroyed() {
        mImpl = null;
    }

    /**
     * Sets the active (visible) Tab. Only one Tab is visible at a time.
     *
     * @param tab The Tab to make active.
     *
     * @throws IllegalStateException if {@link tab} was not added to this
     *         Browser.
     *
     * @see #addTab()
     */
    public void setActiveTab(@NonNull Tab tab) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            if (getActiveTab() != tab && !mImpl.setActiveTab(tab.getITab())) {
                throw new IllegalStateException("attachTab() must be called before "
                        + "setActiveTab");
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a tab to this Browser. If {link tab} is the active Tab of another Browser, then the
     * other Browser's active tab is set to null. This does nothing if {@link tab} is already
     * contained in this Browser.
     *
     * @param tab The Tab to add.
     */
    public void addTab(@NonNull Tab tab) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (tab.getBrowser() == this) return;
        try {
            mImpl.addTab(tab.getITab());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the active (visible) Tab associated with this
     * Browser.
     *
     * @return The Tab.
     */
    @Nullable
    public Tab getActiveTab() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            Tab tab = Tab.getTabById(mImpl.getActiveTabId());
            assert tab == null || tab.getBrowser() == this;
            return tab;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the set of Tabs contained in this Browser.
     *
     * @return The Tabs
     */
    @NonNull
    public Set<Tab> getTabs() {
        ThreadCheck.ensureOnUiThread();
        return Tab.getTabsInBrowser(this);
    }

    /**
     * Disposes a Tab. If {@link tab} is the active Tab, no Tab is made active. After this call
     *  {@link tab} should not be used.
     *
     * Note this will skip any beforeunload handlers. To run those first, use
     * {@link Tab#dispatchBeforeUnloadAndClose} instead.
     *
     * @param tab The Tab to dispose.
     *
     * @throws IllegalStateException is {@link tab} is not in this Browser.
     */
    public void destroyTab(@NonNull Tab tab) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (tab.getBrowser() != this) {
            throw new IllegalStateException("destroyTab() must be called on a Tab in the Browser");
        }
        try {
            mImpl.destroyTab(tab.getITab());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a TabListCallback.
     *
     * @param callback The TabListCallback.
     */
    public void registerTabListCallback(@NonNull TabListCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mTabListCallbacks.addObserver(callback);
    }

    /**
     * Removes a TabListCallback.
     *
     * @param callback The TabListCallback.
     */
    public void unregisterTabListCallback(@NonNull TabListCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mTabListCallbacks.removeObserver(callback);
    }

    /**
     * Returns true if this Browser is in the process of restoring the previous state.
     *
     * @param True if restoring previous state.
     *
     * @since 87
     */
    public boolean isRestoringPreviousState() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        try {
            return mImpl.isRestoringPreviousState();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a BrowserRestoreCallback.
     *
     * @param callback The BrowserRestoreCallback.
     *
     * @since 87
     */
    public void registerBrowserRestoreCallback(@NonNull BrowserRestoreCallback callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        mBrowserRestoreCallbacks.addObserver(callback);
    }

    /**
     * Removes a BrowserRestoreCallback.
     *
     * @param callback The BrowserRestoreCallback.
     *
     * @since 87
     */
    public void unregisterBrowserRestoreCallback(@NonNull BrowserRestoreCallback callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        mBrowserRestoreCallbacks.removeObserver(callback);
    }

    /**
     * Sets the View shown at the top of the browser. A value of null removes the view. The
     * top-view is typically used to show the uri. The top-view scrolls with the page.
     *
     * @param view The new top-view.
     */
    public void setTopView(@Nullable View view) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.setTopView(ObjectWrapper.wrap(view));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets the View shown at the top of the browser. The top-view is typically used to show the
     * uri. This method also allows you to control the scrolling behavior of the top-view by setting
     * a minimum height it will scroll to, and pinning the top-view to the top of the web contents.
     *
     * @param view The new top-view, or null to remove the view.
     * @param minHeight The minimum height in pixels that the top controls can scoll up to. A value
     *        of 0 means the top-view should scroll entirely off screen.
     * @param onlyExpandControlsAtPageTop Whether the top-view should only be expanded when the web
     *        content is scrolled to the top. A true value makes the top-view behave as though it
     *        were inserted into the top of the page content. If true, the top-view should NOT be
     *        used to display the URL, as this will prevent it from expanding in security-sensitive
     *        contexts where the URL should be visible to the user.
     * @param animate Whether or not any height/visibility changes that result from this call
     *        should be animated.
     *
     * @since 86
     */
    public void setTopView(@Nullable View view, int minHeight, boolean onlyExpandControlsAtPageTop,
            boolean animate) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.setTopViewAndScrollingBehavior(
                    ObjectWrapper.wrap(view), minHeight, onlyExpandControlsAtPageTop, animate);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets the View shown at the bottom of the browser. A value of null removes the view.
     *
     * @param view The new bottom-view.
     *
     * @since 84
     */
    public void setBottomView(@Nullable View view) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.setBottomView(ObjectWrapper.wrap(view));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Creates a new tab attached to this browser. This will call {@link TabListCallback#onTabAdded}
     * with the new tab.
     *
     * @since 85
     */
    public @NonNull Tab createTab() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (WebLayer.getSupportedMajorVersionInternal() < 85) {
            throw new UnsupportedOperationException();
        }
        try {
            ITab iTab = mImpl.createTab();
            Tab tab = Tab.getTabById(iTab.getId());
            assert tab != null;
            return tab;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Control support for embedding use cases such as animations. This should be enabled when the
     * container view of the fragment is animated in any way, needs to be rotated or blended, or
     * need to control z-order with other views or other BrowserFragmentImpls. Note embedder should
     * keep WebLayer in the default non-embedding mode when user is interacting with the web
     * content. Embedding mode does not support encrypted video.
     *
     * @param enable Whether to support embedding
     * @param callback {@link Callback} to be called with a boolean indicating whether request
     * succeeded. A request might fail if it is subsumed by a subsequent request, or if this object
     * is destroyed.
     */
    public void setSupportsEmbedding(boolean enable, @NonNull Callback<Boolean> callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            mImpl.setSupportsEmbedding(
                    enable, ObjectWrapper.wrap((ValueCallback<Boolean>) callback::onResult));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns {@link Profile} associated with this Browser Fragment. Multiple fragments can share
     * the same Profile.
     */
    @NonNull
    public Profile getProfile() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            return Profile.of(mImpl.getProfile());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the UrlBarController.
     * @since 82
     */
    @NonNull
    public UrlBarController getUrlBarController() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return mUrlBarController;
    }

    private final class BrowserClientImpl extends IBrowserClient.Stub {
        @Override
        public void onActiveTabChanged(int activeTabId) {
            StrictModeWorkaround.apply();
            Tab tab = Tab.getTabById(activeTabId);
            for (TabListCallback callback : mTabListCallbacks) {
                callback.onActiveTabChanged(tab);
            }
        }

        @Override
        public void onTabAdded(ITab iTab) {
            StrictModeWorkaround.apply();
            int id = 0;
            try {
                id = iTab.getId();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
            Tab tab = Tab.getTabById(id);
            if (tab == null) {
                tab = new Tab(iTab, Browser.this);
            } else {
                tab.setBrowser(Browser.this);
            }
            for (TabListCallback callback : mTabListCallbacks) {
                callback.onTabAdded(tab);
            }
        }

        @Override
        public void onTabRemoved(int tabId) {
            StrictModeWorkaround.apply();
            Tab tab = Tab.getTabById(tabId);
            // This should only be called with a previously created tab.
            assert tab != null;
            // And this should only be called for tabs attached to this browser.
            assert tab.getBrowser() == Browser.this;

            tab.setBrowser(null);
            for (TabListCallback callback : mTabListCallbacks) {
                callback.onTabRemoved(tab);
            }
            tab.onRemovedFromBrowser();
        }

        @Override
        public IRemoteFragment createMediaRouteDialogFragment() {
            StrictModeWorkaround.apply();
            return MediaRouteDialogFragment.create(mFragment);
        }

        @Override
        public void onRestoreCompleted() {
            for (BrowserRestoreCallback callback : mBrowserRestoreCallbacks) {
                callback.onRestoreCompleted();
            }
        }
    }
}
