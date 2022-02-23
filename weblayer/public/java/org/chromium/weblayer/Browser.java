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
public class Browser {
    // Set to null once destroyed (or for tests).
    private IBrowser mImpl;
    // The Fragment the Browser is associated with. The value of this may change.
    @Nullable
    private Fragment mFragment;
    private final ObserverList<TabListCallback> mTabListCallbacks;
    private final UrlBarController mUrlBarController;

    private final ObserverList<BrowserControlsOffsetCallback> mBrowserControlsOffsetCallbacks;
    private final ObserverList<BrowserRestoreCallback> mBrowserRestoreCallbacks;

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
        mUrlBarController = null;
        mBrowserControlsOffsetCallbacks = null;
        mBrowserRestoreCallbacks = null;
    }

    Browser(IBrowser impl, Fragment fragment) {
        mImpl = impl;
        mFragment = fragment;
        mTabListCallbacks = new ObserverList<TabListCallback>();
        mBrowserControlsOffsetCallbacks = new ObserverList<BrowserControlsOffsetCallback>();
        mBrowserRestoreCallbacks = new ObserverList<BrowserRestoreCallback>();

        try {
            mImpl.setClient(new BrowserClientImpl());
            mUrlBarController = new UrlBarController(mImpl.getUrlBarController());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Changes the fragment. During configuration changes the fragment may change.
     */
    void setFragment(@Nullable BrowserFragment fragment) {
        mFragment = fragment;
    }

    /**
     * Returns the fragment this Browser is associated with. During configuration changes the
     * fragment may change, and be null for some amount of time.
     */
    @Nullable
    public Fragment getFragment() {
        return mFragment;
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

    /**
     * Returns true if this Browser has been destroyed.
     */
    public boolean isDestroyed() {
        ThreadCheck.ensureOnUiThread();
        return mImpl == null;
    }

    // Called prior to notifying IBrowser of destroy().
    void prepareForDestroy() {
        mFragment = null;
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
     * Adds a BrowserRestoreCallback.
     *
     * @param callback The BrowserRestoreCallback.
     */
    public void registerBrowserRestoreCallback(@NonNull BrowserRestoreCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        mBrowserRestoreCallbacks.addObserver(callback);
    }

    /**
     * Removes a BrowserRestoreCallback.
     *
     * @param callback The BrowserRestoreCallback.
     */
    public void unregisterBrowserRestoreCallback(@NonNull BrowserRestoreCallback callback) {
        ThreadCheck.ensureOnUiThread();
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
     */
    public void setTopView(@Nullable View view, int minHeight, boolean onlyExpandControlsAtPageTop,
            boolean animate) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
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
     * Registers {@link callback} to be notified when the offset of the top or bottom view changes.
     *
     * @param callback The BrowserControlsOffsetCallback to notify
     *
     * @since 88
     */
    public void registerBrowserControlsOffsetCallback(
            @NonNull BrowserControlsOffsetCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        if (mBrowserControlsOffsetCallbacks.isEmpty()) {
            try {
                mImpl.setBrowserControlsOffsetsEnabled(true);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        mBrowserControlsOffsetCallbacks.addObserver(callback);
    }

    /**
     * Removes a BrowserControlsOffsetCallback that was added using {@link
     * registerBrowserControlsOffsetCallback}.
     *
     * @param callback The BrowserControlsOffsetCallback to remove.
     *
     * @since 88
     */
    public void unregisterBrowserControlsOffsetCallback(
            @NonNull BrowserControlsOffsetCallback callback) {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        if (WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        mBrowserControlsOffsetCallbacks.removeObserver(callback);
        if (mBrowserControlsOffsetCallbacks.isEmpty()) {
            try {
                mImpl.setBrowserControlsOffsetsEnabled(false);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
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
     * Control support for embedding use cases such as animations. This should be enabled when the
     * container view of the fragment is animated in any way, needs to be rotated or blended, or
     * need to control z-order with other views or other BrowserFragmentImpls. Note embedder should
     * keep WebLayer in the default non-embedding mode when user is interacting with the web
     * content. Embedding mode does not support encrypted video.
     * Deprecated in 90. Use setEmbeddabilityMode instead.
     *
     * @param enable Whether to support embedding
     * @param callback {@link Callback} to be called with a boolean indicating whether request
     * succeeded. A request might fail if it is subsumed by a subsequent request, or if this object
     * is destroyed.
     */
    @Deprecated
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
     * See BrowserEmbeddabilityMode for details. The default mode is UNSUPPORTED.
     * @param mode the requested embedding mode.
     * @param callback {@link Callback} to be called with a boolean indicating whether request
     * succeeded. A request might fail if it is subsumed by a subsequent request, or if this object
     * is destroyed.
     * @since 90
     */
    public void setEmbeddabilityMode(
            @BrowserEmbeddabilityMode int mode, @NonNull Callback<Boolean> callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 90) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        try {
            mImpl.setEmbeddabilityMode(
                    mode, ObjectWrapper.wrap((ValueCallback<Boolean>) callback::onResult));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Set the minimum surface size of this Browser instance.
     * Setting this avoids expensive surface resize for a fragment view resize that is within the
     * minimum size. The trade off is the additional memory and power needed for the larger
     * surface. For example, for a browser use case, it's likely worthwhile to set the minimum
     * surface size to the screen size to avoid surface resize when entering and exiting fullscreen.
     * It is safe to call this before Views are initialized.
     * Note Android does have a max size limit on Surfaces which applies here as well; this
     * generally should not be larger than the device screen size.
     * Note the surface size is increased to the layout size only if both the width and height are
     * no larger than the minimum surface size. No adjustment is made if the surface size is larger
     * than the minimum size in one dimension and smaller in the other dimension.
     * @since 89
     */
    public void setMinimumSurfaceSize(int width, int height) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        try {
            mImpl.setMinimumSurfaceSize(width, height);
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

    /**
     * Returns the UrlBarController.
     */
    @NonNull
    public UrlBarController getUrlBarController() {
        ThreadCheck.ensureOnUiThread();
        throwIfDestroyed();
        return mUrlBarController;
    }

    /**
     * Normally when the Browser is detached the visibility of the page is set to hidden. When the
     * visibility is hidden video may stop, or other side effects may result. At certain times,
     * such as fullscreen or rotation, it may be necessary to transiently detach the Browser.
     * Calling this method with a value of false results in WebLayer not hiding the page on the next
     * detach. Once the Browser is reattached, the value is implicitly reset to true. Calling this
     * method when the Browser is already detached does nothing.
     *
     * @param changeVisibility Whether WebLayer should change visibility as the result of a detach.
     *
     * @since 91
     */
    public void setChangeVisibilityOnNextDetach(boolean changeVisibility) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 91) {
            throw new UnsupportedOperationException();
        }
        throwIfDestroyed();
        try {
            mImpl.setChangeVisibilityOnNextDetach(changeVisibility);
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
            return MediaRouteDialogFragment.create(mFragment);
        }

        @Override
        public void onBrowserControlsOffsetsChanged(boolean isTop, int offset) {
            for (BrowserControlsOffsetCallback callback : mBrowserControlsOffsetCallbacks) {
                if (isTop) {
                    callback.onTopViewOffsetChanged(offset);
                } else {
                    callback.onBottomViewOffsetChanged(offset);
                }
            }
        }

        @Override
        public void onRestoreCompleted() {
            for (BrowserRestoreCallback callback : mBrowserRestoreCallbacks) {
                callback.onRestoreCompleted();
            }
        }
    }
}
