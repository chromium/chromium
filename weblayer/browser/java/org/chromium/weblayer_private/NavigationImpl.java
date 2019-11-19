// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.INavigation;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.LoadError;
import org.chromium.weblayer_private.interfaces.NavigationState;

import java.util.Arrays;
import java.util.List;

/**
 * Implementation of INavigation.
 */
@JNINamespace("weblayer")
public final class NavigationImpl extends INavigation.Stub {
    private final IClientNavigation mClientNavigation;
    // WARNING: NavigationImpl may outlive the native side, in which case this member is set to 0.
    private long mNativeNavigationImpl;

    public NavigationImpl(INavigationControllerClient client, long nativeNavigationImpl) {
        mNativeNavigationImpl = nativeNavigationImpl;
        try {
            mClientNavigation = client.createClientNavigation(this);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        NavigationImplJni.get().setJavaNavigation(mNativeNavigationImpl, NavigationImpl.this);
    }

    public IClientNavigation getClientNavigation() {
        return mClientNavigation;
    }

    @NavigationState
    private static int implTypeToJavaType(@ImplNavigationState int type) {
        switch (type) {
            case ImplNavigationState.WAITING_RESPONSE:
                return NavigationState.WAITING_RESPONSE;
            case ImplNavigationState.RECEIVING_BYTES:
                return NavigationState.RECEIVING_BYTES;
            case ImplNavigationState.COMPLETE:
                return NavigationState.COMPLETE;
            case ImplNavigationState.FAILED:
                return NavigationState.FAILED;
        }
        assert false;
        return NavigationState.FAILED;
    }

    @Override
    @NavigationState
    public int getState() {
        throwIfNativeDestroyed();
        return implTypeToJavaType(
                NavigationImplJni.get().getState(mNativeNavigationImpl, NavigationImpl.this));
    }

    @Override
    public String getUri() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getUri(mNativeNavigationImpl, NavigationImpl.this);
    }

    @Override
    public List<String> getRedirectChain() {
        throwIfNativeDestroyed();
        return Arrays.asList(NavigationImplJni.get().getRedirectChain(
                mNativeNavigationImpl, NavigationImpl.this));
    }

    @Override
    public int getHttpStatusCode() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getHttpStatusCode(
                mNativeNavigationImpl, NavigationImpl.this);
    }

    @Override
    public boolean isSameDocument() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isSameDocument(mNativeNavigationImpl, NavigationImpl.this);
    }

    @Override
    public boolean isErrorPage() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isErrorPage(mNativeNavigationImpl, NavigationImpl.this);
    }

    @Override
    public int getLoadError() {
        throwIfNativeDestroyed();
        return implLoadErrorToLoadError(
                NavigationImplJni.get().getLoadError(mNativeNavigationImpl, NavigationImpl.this));
    }

    private void throwIfNativeDestroyed() {
        if (mNativeNavigationImpl == 0) {
            throw new IllegalStateException("Using Navigation after native destroyed");
        }
    }

    @LoadError
    private static int implLoadErrorToLoadError(@ImplLoadError int loadError) {
        switch (loadError) {
            case ImplLoadError.NO_ERROR:
                return LoadError.NO_ERROR;
            case ImplLoadError.HTTP_CLIENT_ERROR:
                return LoadError.HTTP_CLIENT_ERROR;
            case ImplLoadError.HTTP_SERVER_ERROR:
                return LoadError.HTTP_SERVER_ERROR;
            case ImplLoadError.SSL_ERROR:
                return LoadError.SSL_ERROR;
            case ImplLoadError.CONNECTIVITY_ERROR:
                return LoadError.CONNECTIVITY_ERROR;
            case ImplLoadError.OTHER_ERROR:
                return LoadError.OTHER_ERROR;
            default:
                throw new IllegalArgumentException("Unexpected load error " + loadError);
        }
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeNavigationImpl = 0;
        // TODO: this should likely notify delegate in some way.
    }

    @NativeMethods
    interface Natives {
        void setJavaNavigation(long nativeNavigationImpl, NavigationImpl caller);
        int getState(long nativeNavigationImpl, NavigationImpl caller);
        String getUri(long nativeNavigationImpl, NavigationImpl caller);
        String[] getRedirectChain(long nativeNavigationImpl, NavigationImpl caller);
        int getHttpStatusCode(long nativeNavigationImpl, NavigationImpl caller);
        boolean isSameDocument(long nativeNavigationImpl, NavigationImpl caller);
        boolean isErrorPage(long nativeNavigationImpl, NavigationImpl caller);
        int getLoadError(long nativeNavigationImpl, NavigationImpl caller);
    }
}
