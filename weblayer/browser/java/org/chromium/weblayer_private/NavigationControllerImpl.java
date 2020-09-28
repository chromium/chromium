// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.NavigateParams;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Acts as the bridge between java and the C++ implementation of of NavigationController.
 */
@JNINamespace("weblayer")
public final class NavigationControllerImpl extends INavigationController.Stub {
    private long mNativeNavigationController;
    private INavigationControllerClient mNavigationControllerClient;

    public NavigationControllerImpl(TabImpl tab, INavigationControllerClient client) {
        mNavigationControllerClient = client;
        mNativeNavigationController =
                NavigationControllerImplJni.get().getNavigationController(tab.getNativeTab());
        NavigationControllerImplJni.get().setNavigationControllerImpl(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void navigate(String uri, NavigateParams params) throws RemoteException {
        StrictModeWorkaround.apply();
        if (WebLayerFactoryImpl.getClientMajorVersion() < 83) {
            assert params == null;
        }
        navigate2(uri, params == null ? false : params.mShouldReplaceCurrentEntry, false, false,
                false);
    }

    @Override
    public void navigate2(String uri, boolean shouldReplaceCurrentEntry,
            boolean disableIntentProcessing, boolean disableNetworkErrorAutoReload,
            boolean enableAutoPlay) throws RemoteException {
        StrictModeWorkaround.apply();
        NavigationControllerImplJni.get().navigate(mNativeNavigationController, uri,
                shouldReplaceCurrentEntry, disableIntentProcessing, disableNetworkErrorAutoReload,
                enableAutoPlay);
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

    @CalledByNative
    private NavigationImpl createNavigation(long nativeNavigationImpl) {
        return new NavigationImpl(mNavigationControllerClient, nativeNavigationImpl);
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
    private void navigationCompleted(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.navigationCompleted(navigation.getClientNavigation());
    }

    @CalledByNative
    private void navigationFailed(NavigationImpl navigation) throws RemoteException {
        mNavigationControllerClient.navigationFailed(navigation.getClientNavigation());
    }

    @CalledByNative
    private void loadStateChanged(boolean isLoading, boolean toDifferentDocument)
            throws RemoteException {
        mNavigationControllerClient.loadStateChanged(isLoading, toDifferentDocument);
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
    private void onOldPageNoLongerRendered(String uri) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 85) return;
        mNavigationControllerClient.onOldPageNoLongerRendered(uri);
    }

    @NativeMethods
    interface Natives {
        void setNavigationControllerImpl(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        long getNavigationController(long tab);
        void navigate(long nativeNavigationControllerImpl, String uri,
                boolean shouldReplaceCurrentEntry, boolean disableIntentProcessing,
                boolean disableNetworkErrorAutoReload, boolean enableAutoPlay);
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
    }
}
