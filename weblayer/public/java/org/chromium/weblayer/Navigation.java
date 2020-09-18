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
public class Navigation extends IClientNavigation.Stub {
    private final INavigation mNavigationImpl;

    // Constructor for test mocking.
    protected Navigation() {
        mNavigationImpl = null;
    }

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

    /**
     * Whether this navigation resulted in a download. Returns false if this navigation did not
     * result in a download, or if download status is not yet known for this navigation.  Download
     * status is determined for a navigation when processing final (post redirect) HTTP response
     * headers. This means the only time the embedder can know if it's a download is in
     * NavigationCallback.onNavigationFailed.
     *
     * @since 84
     */
    public boolean isDownload() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.isDownload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether this navigation was stopped before it could complete because
     * NavigationController.stop() was called.
     *
     * @since 84
     */
    public boolean wasStopCalled() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.wasStopCalled();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets a header for a network request. If a header with the specified name exists it is
     * overwritten. This method can only be called at two times, from
     * {@link NavigationCallback.onNavigationStarted} and {@link
     * NavigationCallback.onNavigationRedirected}. When called during start, the header applies to
     * both the initial network request as well as redirects.
     *
     * This method may be used to set the referer. If the referer is set in navigation start, it is
     * reset during the redirect. In other words, if you need to set a referer that applies to
     * redirects, then this must be called from {@link onNavigationRedirected}.
     *
     * @param name The name of the header. The name must be rfc 2616 compliant.
     * @param value The value of the header. The value must not contain '\0', '\n' or '\r'.
     *
     * @throws IllegalArgumentException If supplied invalid values.
     * @throws IllegalStateException If not called during start or a redirect.
     *
     * @since 83
     */
    public void setRequestHeader(@NonNull String name, @NonNull String value) {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationImpl.setRequestHeader(name, value);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets the user-agent string that applies to the current navigation. This user-agent is not
     * sticky, it applies to this navigation only (and any redirects or resources that are loaded).
     * This method may only be called from {@link NavigationCallback.onNavigationStarted}.
     *
     * @param value The user-agent string. The value must not contain '\0', '\n' or '\r'. An empty
     * string results in the default user-agent string.
     *
     * @throws IllegalArgumentException If supplied an invalid value.
     * @throws IllegalStateException If not called during start.
     *
     * @since 84
     */
    public void setUserAgentString(@NonNull String value) {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationImpl.setUserAgentString(value);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns whether the navigation was initiated by the page. Examples of page-initiated
     * navigations:
     * * Clicking <a> links.
     * * changing window.location.href
     * * redirect via the <meta http-equiv="refresh"> tag
     * * using window.history.pushState
     *
     * This method returns false for navigations initiated by the WebLayer API, including using
     *  window.history.forward() or window.history.back().
     *
     * @return Whether the navigation was initiated by the page.
     *
     * @since 86
     */
    public boolean isPageInitiated() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.isPageInitiated();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether the navigation is a reload. Examples of reloads include:
     * * embedder-specified through NavigationController::Reload
     * * page-initiated reloads, e.g. location.reload()
     * * reloads when the network interface is reconnected
     *
     * @since 86
     */
    public boolean isReload() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.isReload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
