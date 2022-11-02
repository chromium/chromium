// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.WebResourceResponse;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientPage;
import org.chromium.weblayer_private.interfaces.INavigateParams;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.NavigateParams;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.HashMap;
import java.util.Map;

/**
 * Acts as the bridge between java and the C++ implementation of of NavigationController.
 */
@JNINamespace("weblayer")
public final class NavigationControllerImpl extends INavigationController.Stub {
    private final TabImpl mTab;
    private long mNativeNavigationController;
    private INavigationControllerClient mNavigationControllerClient;

    private Map<Long, PageImpl> mPages = new HashMap<>();

    public NavigationControllerImpl(TabImpl tab, INavigationControllerClient client) {
        mTab = tab;
        mNavigationControllerClient = client;
        mNativeNavigationController =
                NavigationControllerImplJni.get().getNavigationController(tab.getNativeTab());
        NavigationControllerImplJni.get().setNavigationControllerImpl(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void navigate(String uri, NavigateParams params) {
        StrictModeWorkaround.apply();
        if (WebLayerFactoryImpl.getClientMajorVersion() < 83) {
            assert params == null;
        }
        NavigationControllerImplJni.get().navigate(mNativeNavigationController, uri,
                params == null ? false : params.mShouldReplaceCurrentEntry, false, false, false,
                false, null);
    }

    @Override
    public void navigate2(String uri, boolean shouldReplaceCurrentEntry,
            boolean disableIntentProcessing, boolean disableNetworkErrorAutoReload,
            boolean enableAutoPlay) {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().navigate(mNativeNavigationController, uri,
                shouldReplaceCurrentEntry, disableIntentProcessing,
                /*allowIntentLaunchesInBackground=*/false, disableNetworkErrorAutoReload,
                enableAutoPlay, null);
    }

    @Override
    public INavigateParams createNavigateParams() {
        StrictModeWorkaround.apply();
        return new NavigateParamsImpl();
    }

    @Override
    public void navigate3(String uri, INavigateParams iParams) {
        StrictModeWorkaround.apply();
        NavigateParamsImpl params = (NavigateParamsImpl) iParams;
        WebResourceResponseInfo responseInfo = null;
        if (params.getResponse() != null) {
            WebResourceResponse response =
                    ObjectWrapper.unwrap(params.getResponse(), WebResourceResponse.class);
            responseInfo = new WebResourceResponseInfo(response.getMimeType(),
                    response.getEncoding(), response.getData(), response.getStatusCode(),
                    response.getReasonPhrase(), response.getResponseHeaders());
        }

        NavigationControllerImplJni.get().navigate(mNativeNavigationController, uri,
                params.shouldReplaceCurrentEntry(), params.isIntentProcessingDisabled(),
                params.areIntentLaunchesAllowedInBackground(),
                params.isNetworkErrorAutoReloadDisabled(), params.isAutoPlayEnabled(),
                responseInfo);
    }

    @Override
    public void goBack() {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().goBack(mNativeNavigationController);
    }

    @Override
    public void goForward() {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().goForward(mNativeNavigationController);
    }

    @Override
    public boolean canGoBack() {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().canGoBack(mNativeNavigationController);
    }

    @Override
    public boolean canGoForward() {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().canGoForward(mNativeNavigationController);
    }

    @Override
    public void goToIndex(int index) {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().goToIndex(mNativeNavigationController, index);
    }

    @Override
    public void reload() {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().reload(mNativeNavigationController);
    }

    @Override
    public void stop() {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().stop(mNativeNavigationController);
    }

    @Override
    public int getNavigationListSize() {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().getNavigationListSize(mNativeNavigationController);
    }

    @Override
    public int getNavigationListCurrentIndex() {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().getNavigationListCurrentIndex(
                mNativeNavigationController);
    }

    @Override
    public String getNavigationEntryDisplayUri(int index) {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().getNavigationEntryDisplayUri(
                mNativeNavigationController, index);
    }

    @Override
    public String getNavigationEntryTitle(int index) {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().getNavigationEntryTitle(
                mNativeNavigationController, index);
    }

    @Override
    public boolean isNavigationEntrySkippable(int index) {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().isNavigationEntrySkippable(
                mNativeNavigationController, index);
    }

    public NavigationImpl getNavigationImplFromId(long id) {
        StrictModeWorkaround.apply();
        return NavigationControllerImplJni.get().getNavigationImplFromId(
                mNativeNavigationController, id);
    }

    public PageImpl getPage(long nativePageImpl) {
        // Ensure that each C++ object has only one Java counterpart so that the embedder sees the
        // same object for multiple navigations that have the same Page.
        PageImpl page = mPages.get(nativePageImpl);
        if (page == null) {
            IClientPage clientPage = null;
            if (WebLayerFactoryImpl.getClientMajorVersion() >= 90) {
                try {
                    clientPage = mNavigationControllerClient.createClientPage();
                } catch (RemoteException e) {
                    throw new APICallException(e);
                }
            }
            page = new PageImpl(clientPage, nativePageImpl, this);
            mPages.put(nativePageImpl, page);
        }
        return page;
    }

    public void onPageDestroyed(PageImpl page) {
        mPages.remove(page.getNativePageImpl());
        if (WebLayerFactoryImpl.getClientMajorVersion() >= 90) {
            try {
                mNavigationControllerClient.onPageDestroyed(page.getClientPage());
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
    }

    @CalledByNative
    private NavigationImpl createNavigation(long nativeNavigationImpl) {
        return new NavigationImpl(mNavigationControllerClient, nativeNavigationImpl, this);
    }

    @CalledByNative
    private void navigationStarted(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.navigationStarted(navigation.getClientNavigation());
    }

    @CalledByNative
    private void navigationRedirected(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.navigationRedirected(navigation.getClientNavigation());
    }

    @CalledByNative
    private void readyToCommitNavigation(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.readyToCommitNavigation(navigation.getClientNavigation());
    }

    @CalledByNative
    private void getOrCreatePageForNavigation(NavigationImpl navigation) throws RemoteException {
        navigation.getPage();
    }

    @CalledByNative
    private void navigationCompleted(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.navigationCompleted(navigation.getClientNavigation());
    }

    @CalledByNative
    private void navigationFailed(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.navigationFailed(navigation.getClientNavigation());
    }

    @CalledByNative
    private void loadStateChanged(boolean isLoading, boolean shouldShowLoadingUi)
            throws RemoteException {
        mNavigationControllerClient.loadStateChanged(isLoading, shouldShowLoadingUi);
    }

    @CalledByNative
    private void loadProgressChanged(double progress) throws RemoteException {
        mNavigationControllerClient.loadProgressChanged(progress);
    }

    @CalledByNative
    private void onFirstContentfulPaint() throws RemoteException {
        mNavigationControllerClient.onFirstContentfulPaint();
    }

    @CalledByNative
    private void onFirstContentfulPaint2(
            long navigationStartMs, long firstContentfulPaintDurationMs) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 88) return;

        mNavigationControllerClient.onFirstContentfulPaint2(
                navigationStartMs, firstContentfulPaintDurationMs);
    }

    @CalledByNative
    private void onLargestContentfulPaint(
            long navigationStartMs, long largestContentfulPaintDurationMs) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 88) return;

        mNavigationControllerClient.onLargestContentfulPaint(
                navigationStartMs, largestContentfulPaintDurationMs);
    }

    @CalledByNative
    private void onOldPageNoLongerRendered(String uri) throws RemoteException {
        mNavigationControllerClient.onOldPageNoLongerRendered(uri);
    }

    @CalledByNative
    private void onPageLanguageDetermined(PageImpl page, String language) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 93) return;

        mNavigationControllerClient.onPageLanguageDetermined(page.getClientPage(), language);
    }

    private static final class NavigateParamsImpl extends INavigateParams.Stub {
        private boolean mReplaceCurrentEntry;
        private boolean mIntentProcessingDisabled;
        private boolean mIntentLaunchesAllowedInBackground;
        private boolean mNetworkErrorAutoReloadDisabled;
        private boolean mAutoPlayEnabled;
        private IObjectWrapper mResponse;

        @Override
        public void replaceCurrentEntry() {
            mReplaceCurrentEntry = true;
        }

        @Override
        public void disableIntentProcessing() {
            mIntentProcessingDisabled = true;
        }

        @Override
        public void allowIntentLaunchesInBackground() {
            mIntentLaunchesAllowedInBackground = true;
        }

        @Override
        public void disableNetworkErrorAutoReload() {
            mNetworkErrorAutoReloadDisabled = true;
        }

        @Override
        public void enableAutoPlay() {
            mAutoPlayEnabled = true;
        }

        @Override
        public void setResponse(IObjectWrapper response) {
            mResponse = response;
        }

        public boolean shouldReplaceCurrentEntry() {
            return mReplaceCurrentEntry;
        }

        public boolean isIntentProcessingDisabled() {
            return mIntentProcessingDisabled;
        }

        public boolean areIntentLaunchesAllowedInBackground() {
            return mIntentLaunchesAllowedInBackground;
        }

        public boolean isNetworkErrorAutoReloadDisabled() {
            return mNetworkErrorAutoReloadDisabled;
        }

        public boolean isAutoPlayEnabled() {
            return mAutoPlayEnabled;
        }

        IObjectWrapper getResponse() {
            return mResponse;
        }
    }

    @NativeMethods
    interface Natives {
        void setNavigationControllerImpl(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        long getNavigationController(long tab);
        void navigate(long nativeNavigationControllerImpl, String uri,
                boolean shouldReplaceCurrentEntry, boolean disableIntentProcessing,
                boolean allowIntentLaunchesInBackground, boolean disableNetworkErrorAutoReload,
                boolean enableAutoPlay, WebResourceResponseInfo response);
        void goBack(long nativeNavigationControllerImpl);
        void goForward(long nativeNavigationControllerImpl);
        boolean canGoBack(long nativeNavigationControllerImpl);
        boolean canGoForward(long nativeNavigationControllerImpl);
        void goToIndex(long nativeNavigationControllerImpl, int index);
        void reload(long nativeNavigationControllerImpl);
        void stop(long nativeNavigationControllerImpl);
        int getNavigationListSize(long nativeNavigationControllerImpl);
        int getNavigationListCurrentIndex(long nativeNavigationControllerImpl);
        String getNavigationEntryDisplayUri(long nativeNavigationControllerImpl, int index);
        String getNavigationEntryTitle(long nativeNavigationControllerImpl, int index);
        boolean isNavigationEntrySkippable(long nativeNavigationControllerImpl, int index);
        NavigationImpl getNavigationImplFromId(long nativeNavigationControllerImpl, long id);
    }
}
