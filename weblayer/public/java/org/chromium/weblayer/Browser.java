// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IMediaRouteDialogFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.Set;

/**
 * Browser contains any number of Tabs, with one active Tab. The active Tab is visible to the user,
 * all other Tabs are hidden.
 *
 * Newly created Browsers have a single active Tab.
 *
 * Browser provides for two distinct ways to save state, which impacts the state of the Browser at
 * various points in the lifecycle.
 *
 * Asynchronously to the file system. This is used if a {@link persistenceId} was supplied when the
 * Browser was created. The {@link persistenceId} uniquely identifies the Browser for saving the
 * set of tabs and navigations. This is intended for long term persistence.
 *
 * For Browsers created with a {@link persistenceId}, restore happens asynchronously. As a result,
 * the Browser will not have any tabs until restore completes (which may be after the Fragment has
 * started).
 *
 * If a {@link persistenceId} is not supplied, then a minimal amount of state is saved to the
 * fragment (instance state). During recreation, if instance state is available, the state is
 * restored in {@link onStart}. Restore happens during start so that callbacks can be attached. As
 *  a result of this, the Browser has no tabs until the Fragment is started.
 */
class Browser {
    // Set to null once destroyed (or for tests).
    private IBrowser mImpl;
    private final ObserverList<TabListCallback> mTabListCallbacks;

    private final ObserverList<TabInitializationCallback> mTabInitializationCallbacks;

    private static int sMaxNavigationsPerTabForInstanceState;

    /**
     * Sets the maximum number of navigations saved when persisting a Browsers instance state. The
     * max applies to each Tab in the Browser. For example, if a value of 6 is supplied and the
     * Browser has 4 tabs, then up to 24 navigation entries may be saved. The supplied value is
     * a recommendation, for various reasons it may not be honored. A value of 0 results in
     * using the default.
     *
     * @param value The maximum number of navigations to persist.
     *
     * @throws IllegalArgumentException If {@code value} is less than 0.
     *
     * @since 98
     */
    public static void setMaxNavigationsPerTabForInstanceState(int value) {
        ThreadCheck.ensureOnUiThread();
        if (value < 0) throw new IllegalArgumentException("Max must be >= 0");
        sMaxNavigationsPerTabForInstanceState = value;
    }

    static int getMaxNavigationsPerTabForInstanceState() {
        return sMaxNavigationsPerTabForInstanceState;
    }

    // Constructor for test mocking.
    protected Browser() {
        mImpl = null;
        mTabListCallbacks = null;
        mTabInitializationCallbacks = null;
    }

    // Constructor for browserfragment to inject the {@code tabListCallback} on startup.
    Browser(IBrowser impl) {
        mImpl = impl;
        mTabListCallbacks = new ObserverList<TabListCallback>();
        mTabInitializationCallbacks = new ObserverList<TabInitializationCallback>();
    }

    void initializeState() {
        try {
            mImpl.setClient(new BrowserClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void throwIfDestroyed() {
        if (mImpl == null) {
            throw new IllegalStateException("Browser can not be used once destroyed");
        }
    }

    IBrowser getIBrowser() {
        return mImpl;
    }

    /**
     * Returns remote counterpart for the BrowserFragment: an {@link IRemoteFragment}.
     */
    IRemoteFragment connectFragment() {
        try {
            return mImpl.createBrowserFragmentImpl();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the remote counterpart of MediaRouteDialogFragment.
     */
    /* package */ IMediaRouteDialogFragment createMediaRouteDialogFragment() {
        try {
            return mImpl.createMediaRouteDialogFragmentImpl();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if this Browser has been destroyed.
     */
    public boolean isDestroyed() {
        ThreadCheck.ensureOnUiThread();
        return mImpl == null;
    }

    // Called prior to notifying IBrowser of destroy().
    void prepareForDestroy() {
        for (TabListCallback callback : mTabListCallbacks) {
            callback.onWillDestroyBrowserAndAllTabs();
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
     * Returns a List of Tabs as saved in the native Browser.
     *
     * @return The Tabs.
     */
    @NonNull
    private int[] getTabIds() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mImpl.getTabIds();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
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
     * Navigates to the previous navigation across all tabs according to tabs in native Browser.
     */
    void tryNavigateBack(@NonNull Callback<Boolean> callback) {
        Tab activeTab = getActiveTab();
        if (activeTab == null) {
            callback.onResult(false);
            return;
        }
        if (activeTab.dismissTransientUi()) {
            callback.onResult(true);
            return;
        }
        NavigationController controller = activeTab.getNavigationController();
        if (controller.canGoBack()) {
            controller.goBack();
            callback.onResult(true);
            return;
        }
        int[] tabIds = getTabIds();
        if (tabIds.length > 1) {
            Tab previousTab = null;
            int activeTabId = activeTab.getId();
            int prevId = -1;
            for (int id : tabIds) {
                if (id == activeTabId) {
                    previousTab = Tab.getTabById(prevId);
                    break;
                }
                prevId = id;
            }
            if (previousTab != null) {
                activeTab.dispatchBeforeUnloadAndClose();
                setActiveTab(previousTab);
                callback.onResult(true);
                return;
            }
        }
        callback.onResult(false);
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
     */
    public boolean isRestoringPreviousState() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        try {
            return mImpl.isRestoringPreviousState();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a TabInitializationCallback.
     *
     * @param callback The TabInitializationCallback.
     */
    public void registerTabInitializationCallback(@NonNull TabInitializationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mTabInitializationCallbacks.addObserver(callback);
    }

    /**
     * Removes a TabInitializationCallback.
     *
     * @param callback The TabInitializationCallback.
     */
    public void unregisterTabInitializationCallback(@NonNull TabInitializationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mTabInitializationCallbacks.removeObserver(callback);
    }

    /**
     * Creates a new tab attached to this browser. This will call {@link TabListCallback#onTabAdded}
     * with the new tab.
     */
    public @NonNull Tab createTab() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
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
     * Controls how sites are themed when WebLayer is in dark mode. WebLayer considers itself to be
     * in dark mode if the UI_MODE_NIGHT_YES flag of its Resources' Configuration's uiMode field is
     * set, which is typically controlled with AppCompatDelegate#setDefaultNightMode. By default
     * pages will only be rendered in dark mode if WebLayer is in dark mode and they provide a dark
     * theme in CSS. See DarkModeStrategy for other possible configurations.
     *
     * @see DarkModeStrategy
     * @param strategy See {@link DarkModeStrategy}.
     *
     * @since 90
     */
    public void setDarkModeStrategy(@DarkModeStrategy int strategy) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        try {
            mImpl.setDarkModeStrategy(strategy);
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

    public void shutdown() {
        try {
            mImpl.shutdown();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
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
        }

        @Override
        public IRemoteFragment createMediaRouteDialogFragment() {
            StrictModeWorkaround.apply();
            return new MediaRouteDialogFragmentEventHandler().getRemoteFragment();
        }

        @Override
        public void onTabInitializationCompleted() {
            for (TabInitializationCallback callback : mTabInitializationCallbacks) {
                callback.onTabInitializationCompleted();
            }
        }
    }
}
