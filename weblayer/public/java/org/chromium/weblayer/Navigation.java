// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.INavigation;

import java.util.ArrayList;
import java.util.List;

/**
 * Information about a navigation.
 */
public final class Navigation extends IClientNavigation.Stub {
    private final INavigation mNavigationImpl;

    Navigation(INavigation impl) {
        mNavigationImpl = impl;
    }

    @NavigationState
    public int getState() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.getState();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * The uri the main frame is navigating to. This may change during the navigation when
     * encountering a server redirect.
     */
    @NonNull
    public Uri getUri() {
        ThreadCheck.ensureOnUiThread();
        try {
            return Uri.parse(mNavigationImpl.getUri());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the redirects that occurred on the way to the current page. The current page is the
     * last one in the list (so even when there's no redirect, there will be one entry in the list).
     */
    @NonNull
    public List<Uri> getRedirectChain() {
        ThreadCheck.ensureOnUiThread();
        try {
            List<Uri> redirects = new ArrayList<Uri>();
            for (String r : mNavigationImpl.getRedirectChain()) redirects.add(Uri.parse(r));
            return redirects;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the status code of the navigation. Returns 0 if the navigation  hasn't completed yet
     * or if a response wasn't received.
     */
    public int getHttpStatusCode() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.getHttpStatusCode();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether the navigation happened without changing document. Examples of same document
     * navigations are:
     *  - reference fragment navigations
     *  - pushState/replaceState
     *  - same page history navigation
     */
    public boolean isSameDocument() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.isSameDocument();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether the navigation resulted in an error page (e.g. interstitial). Note that if an error
     * page reloads, this will return true even though GetNetErrorCode will be kNoError.
     */
    public boolean isErrorPage() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.isErrorPage();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Return information about the error, if any, that was encountered while loading the page.
     */
    @LoadError
    public int getLoadError() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.getLoadError();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
