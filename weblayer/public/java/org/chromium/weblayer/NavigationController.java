// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.IClientPage;
import org.chromium.weblayer_private.interfaces.INavigateParams;
import org.chromium.weblayer_private.interfaces.INavigation;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Provides methods to control navigation, along with maintaining the current list of navigations.
 */
class NavigationController {
    private INavigationController mNavigationController;
    private final ObserverList<NavigationCallback> mCallbacks;

    static NavigationController create(ITab tab) {
        NavigationController navigationController = new NavigationController();
        try {
            navigationController.mNavigationController = tab.createNavigationController(
                    navigationController.new NavigationControllerClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return navigationController;
    }

    // Constructor protected for test mocking.
    protected NavigationController() {
        mCallbacks = new ObserverList<NavigationCallback>();
    }

    public void navigate(@NonNull Uri uri) {
        navigate(uri, null);
    }

    /**
     * Navigates to the given URI, with optional settings.
     *
     * @param uri the destination URI.
     * @param params extra parameters for the navigation.
     *
     */
    public void navigate(@NonNull Uri uri, @Nullable NavigateParams params) {
        ThreadCheck.ensureOnUiThread();
        try {
            INavigateParams iparams = mNavigationController.createNavigateParams();
            if (params != null) {
                if (params.getShouldReplaceCurrentEntry()) iparams.replaceCurrentEntry();
                if (params.isIntentProcessingDisabled()) iparams.disableIntentProcessing();
                if (WebLayer.getSupportedMajorVersionInternal() >= 89
                        && params.areIntentLaunchesAllowedInBackground()) {
                    iparams.allowIntentLaunchesInBackground();
                }
                if (params.isNetworkErrorAutoReloadDisabled()) {
                    iparams.disableNetworkErrorAutoReload();
                }
                if (params.isAutoPlayEnabled()) iparams.enableAutoPlay();
                if (params.getResponse() != null) {
                    iparams.setResponse(ObjectWrapper.wrap(params.getResponse()));
                }
            }
            mNavigationController.navigate3(uri.toString(), iparams);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Navigates to the previous navigation.
     *
     * Note: this may go back more than a single navigation entry, see {@link
     * isNavigationEntrySkippable} for more details.
     *
     * @throws IndexOutOfBoundsException If {@link #canGoBack} returns false.
     */
    public void goBack() throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        if (!canGoBack()) {
            throw new IndexOutOfBoundsException();
        }
        try {
            mNavigationController.goBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Navigates to the next navigation.
     *
     * @throws IndexOutOfBoundsException If {@link #canGoForward} returns false.
     */
    public void goForward() throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        if (!canGoForward()) {
            throw new IndexOutOfBoundsException();
        }
        try {
            mNavigationController.goForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if there is a navigation before the current one.
     *
     * Note: this may return false even if the current index is not 0, see {@link
     * isNavigationEntrySkippable} for more details.
     *
     * @return Whether there is a navigation before the current one.
     */
    public boolean canGoBack() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.canGoBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if there is a navigation after the current one.
     *
     * @return Whether there is a navigation after the current one.
     */
    public boolean canGoForward() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.canGoForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Navigates to the entry at {@link index}.
     *
     * @throws IndexOutOfBoundsException If index is negative or is not less than {@link
     *         getNavigationListSize}.
     */
    public void goToIndex(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        checkNavigationIndex(index);
        try {
            mNavigationController.goToIndex(index);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Reloads the current entry. Does nothing if there are no navigations.
     */
    public void reload() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.reload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Stops in progress loading. Does nothing if not in the process of loading.
     */
    public void stop() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.stop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the number of navigations entries.
     *
     * @return The number of navigation entries, 0 if empty.
     */
    public int getNavigationListSize() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.getNavigationListSize();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the index of the current navigation, -1 if there are no navigations.
     *
     * @return The index of the current navigation.
     */
    public int getNavigationListCurrentIndex() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.getNavigationListCurrentIndex();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the uri to display for the navigation at index.
     *
     * @param index The index of the navigation.
     * @throws IndexOutOfBoundsException If index is negative or is not less than {@link
     *         getNavigationListSize}.
     */
    @NonNull
    public Uri getNavigationEntryDisplayUri(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        checkNavigationIndex(index);
        try {
            return Uri.parse(mNavigationController.getNavigationEntryDisplayUri(index));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the title of the navigation entry at the supplied index.
     *
     * @throws IndexOutOfBoundsException If index is negative or is not less than {@link
     *         getNavigationListSize}.
     */
    @NonNull
    public String getNavigationEntryTitle(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        checkNavigationIndex(index);
        try {
            return mNavigationController.getNavigationEntryTitle(index);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns whether this entry will be skipped on a call to {@link goBack} or {@link goForward}.
     * This will be true for certain navigations, such as certain client side redirects and
     * history.pushState navigations done without user interaction.
     *
     * @throws IndexOutOfBoundsException If index is negative or is not less than {@link
     *         getNavigationListSize}.
     */
    public boolean isNavigationEntrySkippable(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        checkNavigationIndex(index);
        try {
            return mNavigationController.isNavigationEntrySkippable(index);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void registerNavigationCallback(@NonNull NavigationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    public void unregisterNavigationCallback(@NonNull NavigationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    private void checkNavigationIndex(int index) throws IndexOutOfBoundsException {
        if (index < 0 || index >= getNavigationListSize()) {
            throw new IndexOutOfBoundsException();
        }
    }

    private final class NavigationControllerClientImpl extends INavigationControllerClient.Stub {
        @Override
        public IClientNavigation createClientNavigation(INavigation navigationImpl) {
            StrictModeWorkaround.apply();
            return new Navigation(navigationImpl);
        }

        @Override
        public void navigationStarted(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationStarted((Navigation) navigation);
            }
        }

        @Override
        public void navigationRedirected(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationRedirected((Navigation) navigation);
            }
        }

        @Override
        public void readyToCommitNavigation(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            // Functionality removed from NavigationCallback in M90. See crbug.com/1174193
        }

        @Override
        public void navigationCompleted(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationCompleted((Navigation) navigation);
            }
        }

        @Override
        public void navigationFailed(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationFailed((Navigation) navigation);
            }
        }

        @Override
        public void loadStateChanged(boolean isLoading, boolean shouldShowLoadingUi) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onLoadStateChanged(isLoading, shouldShowLoadingUi);
            }
        }

        @Override
        public void loadProgressChanged(double progress) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onLoadProgressChanged(progress);
            }
        }

        @Override
        public void onFirstContentfulPaint() {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onFirstContentfulPaint();
            }
        }

        @Override
        public void onFirstContentfulPaint2(
                long navigationStartMs, long firstContentfulPaintDurationMs) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onFirstContentfulPaint(navigationStartMs, firstContentfulPaintDurationMs);
            }
        }

        @Override
        public void onLargestContentfulPaint(
                long navigationStartMs, long largestContentfulPaintDurationMs) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onLargestContentfulPaint(
                        navigationStartMs, largestContentfulPaintDurationMs);
            }
        }

        @Override
        public void onOldPageNoLongerRendered(String uri) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onOldPageNoLongerRendered(Uri.parse(uri));
            }
        }

        @Override
        public IClientPage createClientPage() {
            StrictModeWorkaround.apply();
            return new Page();
        }

        @Override
        public void onPageDestroyed(IClientPage page) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onPageDestroyed((Page) page);
            }
        }

        @Override
        public void onPageLanguageDetermined(IClientPage page, String language) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onPageLanguageDetermined((Page) page, language);
            }
        }
    }
}
