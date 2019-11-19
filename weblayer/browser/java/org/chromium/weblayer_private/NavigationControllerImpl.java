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

/**
 * Acts as the bridge between java and the C++ implementation of of NavigationController.
 */
@JNINamespace("weblayer")
public final class NavigationControllerImpl extends INavigationController.Stub {
    private long mNativeNavigationController;
    private TabImpl mTab;
    private INavigationControllerClient mNavigationControllerClient;

    public NavigationControllerImpl(TabImpl tab, INavigationControllerClient client) {
        mNavigationControllerClient = client;
        mTab = tab;
        mNativeNavigationController =
                NavigationControllerImplJni.get().getNavigationController(tab.getNativeTab());
        NavigationControllerImplJni.get().setNavigationControllerImpl(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void navigate(String uri) {
        NavigationControllerImplJni.get().navigate(
                mNativeNavigationController, NavigationControllerImpl.this, uri);
    }

    @Override
    public void goBack() {
        NavigationControllerImplJni.get().goBack(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void goForward() {
        NavigationControllerImplJni.get().goForward(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public boolean canGoBack() {
        return NavigationControllerImplJni.get().canGoBack(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public boolean canGoForward() {
        return NavigationControllerImplJni.get().canGoForward(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void reload() {
        NavigationControllerImplJni.get().reload(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public void stop() {
        NavigationControllerImplJni.get().stop(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public int getNavigationListSize() {
        return NavigationControllerImplJni.get().getNavigationListSize(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public int getNavigationListCurrentIndex() {
        return NavigationControllerImplJni.get().getNavigationListCurrentIndex(
                mNativeNavigationController, NavigationControllerImpl.this);
    }

    @Override
    public String getNavigationEntryDisplayUri(int index) {
        return NavigationControllerImplJni.get().getNavigationEntryDisplayUri(
                mNativeNavigationController, NavigationControllerImpl.this, index);
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

    @NativeMethods
    interface Natives {
        void setNavigationControllerImpl(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        long getNavigationController(long tab);
        void navigate(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller, String uri);
        void goBack(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        void goForward(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        boolean canGoBack(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        boolean canGoForward(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        void reload(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        void stop(long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        int getNavigationListSize(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        int getNavigationListCurrentIndex(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller);
        String getNavigationEntryDisplayUri(
                long nativeNavigationControllerImpl, NavigationControllerImpl caller, int index);
    }
}
