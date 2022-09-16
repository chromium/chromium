// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.IClientPage;
import org.chromium.weblayer_private.interfaces.INavigation;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.LoadError;
import org.chromium.weblayer_private.interfaces.NavigationState;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.Arrays;
import java.util.List;

/**
 * Implementation of INavigation.
 */
@JNINamespace("weblayer")
public final class NavigationImpl extends INavigation.Stub {
    private final IClientNavigation mClientNavigation;
    private final NavigationControllerImpl mNavigationController;
    // WARNING: NavigationImpl may outlive the native side, in which case this member is set to 0.
    private long mNativeNavigationImpl;

    // Set to true if/when it is determined that an external intent was launched for this
    // navigation.
    private boolean mIntentLaunched;

    // Set to true if/when it is determined that this navigation result in UI being presented to the
    // user via which the user will determine whether an intent should be launched.
    private boolean mIsUserDecidingIntentLaunch;

    private PageImpl mPage;

    public NavigationImpl(INavigationControllerClient client, long nativeNavigationImpl,
            NavigationControllerImpl navigationController) {
        mNativeNavigationImpl = nativeNavigationImpl;
        mNavigationController = navigationController;
        try {
            mClientNavigation = client.createClientNavigation(this);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
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
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implTypeToJavaType(NavigationImplJni.get().getState(mNativeNavigationImpl));
    }

    @Override
    public String getUri() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getUri(mNativeNavigationImpl);
    }

    @Override
    public List<String> getRedirectChain() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return Arrays.asList(NavigationImplJni.get().getRedirectChain(mNativeNavigationImpl));
    }

    @Override
    public int getHttpStatusCode() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getHttpStatusCode(mNativeNavigationImpl);
    }

    @Override
    public List<String> getResponseHeaders() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return Arrays.asList(NavigationImplJni.get().getResponseHeaders(mNativeNavigationImpl));
    }

    @Override
    public boolean isSameDocument() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isSameDocument(mNativeNavigationImpl);
    }

    @Override
    public boolean isErrorPage() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isErrorPage(mNativeNavigationImpl);
    }

    @Override
    public int getLoadError() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implLoadErrorToLoadError(
                NavigationImplJni.get().getLoadError(mNativeNavigationImpl));
    }

    @Override
    public void setRequestHeader(String name, String value) {
        if (!NavigationImplJni.get().isValidRequestHeaderName(name)) {
            throw new IllegalArgumentException("Invalid header");
        }
        if (!NavigationImplJni.get().isValidRequestHeaderValue(value)) {
            throw new IllegalArgumentException("Invalid value");
        }
        if (!NavigationImplJni.get().setRequestHeader(mNativeNavigationImpl, name, value)) {
            throw new IllegalStateException();
        }
    }

    @Override
    public void setUserAgentString(String value) {
        if (!NavigationImplJni.get().isValidRequestHeaderValue(value)) {
            throw new IllegalArgumentException("Invalid value");
        }
        if (!NavigationImplJni.get().setUserAgentString(mNativeNavigationImpl, value)) {
            throw new IllegalStateException();
        }
    }

    @Override
    public boolean isDownload() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isDownload(mNativeNavigationImpl);
    }

    @Override
    public boolean isKnownProtocol() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isKnownProtocol(mNativeNavigationImpl);
    }

    @Override
    public boolean wasIntentLaunched() {
        return mIntentLaunched;
    }

    @Override
    public boolean isUserDecidingIntentLaunch() {
        return mIsUserDecidingIntentLaunch;
    }

    @Override
    public boolean wasStopCalled() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().wasStopCalled(mNativeNavigationImpl);
    }

    @Override
    public boolean isPageInitiated() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isPageInitiated(mNativeNavigationImpl);
    }

    @Override
    public boolean isReload() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isReload(mNativeNavigationImpl);
    }

    @Override
    public boolean isServedFromBackForwardCache() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isServedFromBackForwardCache(mNativeNavigationImpl);
    }

    @Override
    public void disableNetworkErrorAutoReload() {
        if (!NavigationImplJni.get().disableNetworkErrorAutoReload(mNativeNavigationImpl)) {
            throw new IllegalStateException();
        }
    }

    @Override
    public void disableIntentProcessing() {
        if (!NavigationImplJni.get().disableIntentProcessing(mNativeNavigationImpl)) {
            throw new IllegalStateException();
        }
    }

    @Override
    public boolean isFormSubmission() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().isFormSubmission(mNativeNavigationImpl);
    }

    @Override
    public String getReferrer() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getReferrer(mNativeNavigationImpl);
    }

    @Override
    public IClientPage getPage() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        if (mPage == null) {
            long nativePageImpl = NavigationImplJni.get().getPage(mNativeNavigationImpl);
            if (nativePageImpl == -1) {
                throw new IllegalStateException(
                        "Invoking Navigation#getPage() outside of valid calling context");
            }

            // There should always be a Page associated with the navigation within the valid
            // calling contexts for Navigation#getPage().
            assert (nativePageImpl != 0);

            mPage = mNavigationController.getPage(nativePageImpl);
        }
        return mPage.getClientPage();
    }

    @Override
    public int getNavigationEntryOffset() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getNavigationEntryOffset(mNativeNavigationImpl);
    }

    @Override
    public boolean wasFetchedFromCache() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return NavigationImplJni.get().wasFetchedFromCache(mNativeNavigationImpl);
    }

    public void setIntentLaunched() {
        mIntentLaunched = true;
    }

    public void setIsUserDecidingIntentLaunch() {
        mIsUserDecidingIntentLaunch = true;
    }

    public boolean areIntentLaunchesAllowedInBackground() {
        return NavigationImplJni.get().areIntentLaunchesAllowedInBackground(mNativeNavigationImpl);
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
            case ImplLoadError.SAFE_BROWSING_ERROR:
                return LoadError.SAFE_BROWSING_ERROR;
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
        int getState(long nativeNavigationImpl);
        String getUri(long nativeNavigationImpl);
        String[] getRedirectChain(long nativeNavigationImpl);
        int getHttpStatusCode(long nativeNavigationImpl);
        String[] getResponseHeaders(long nativeNavigationImpl);
        boolean isSameDocument(long nativeNavigationImpl);
        boolean isErrorPage(long nativeNavigationImpl);
        boolean isDownload(long nativeNavigationImpl);
        boolean isKnownProtocol(long nativeNavigationImpl);
        boolean wasStopCalled(long nativeNavigationImpl);
        int getLoadError(long nativeNavigationImpl);
        boolean setRequestHeader(long nativeNavigationImpl, String name, String value);
        boolean isValidRequestHeaderName(String name);
        boolean isValidRequestHeaderValue(String value);
        boolean setUserAgentString(long nativeNavigationImpl, String value);
        boolean isPageInitiated(long nativeNavigationImpl);
        boolean isReload(long nativeNavigationImpl);
        boolean isServedFromBackForwardCache(long nativeNavigationImpl);
        boolean disableNetworkErrorAutoReload(long nativeNavigationImpl);
        boolean disableIntentProcessing(long nativeNavigationImpl);
        boolean areIntentLaunchesAllowedInBackground(long nativeNavigationImpl);
        boolean isFormSubmission(long nativeNavigationImpl);
        String getReferrer(long nativeNavigationImpl);
        long getPage(long nativeNavigationImpl);
        int getNavigationEntryOffset(long nativeNavigationImpl);
        boolean wasFetchedFromCache(long nativeNavigationImpl);
    }
}
