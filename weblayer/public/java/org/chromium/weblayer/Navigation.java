// Copyright 2019 The Chromium Authors
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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Information about a navigation. Each time there is a new navigation, a new
 * Navigation object will be created and that same object will be used in all
 * of the NavigationCallback methods.
 */
class Navigation extends IClientNavigation.Stub {
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
     * Returns the status code of the navigation. Returns 0 if the navigation hasn't completed yet
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

    /*
     * Returns the HTTP response headers. Returns an empty map if the navigation hasn't completed
     * yet or if a response wasn't received.
     *
     * @since 91
     */
    public Map<String, String> getResponseHeaders() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 91) {
            throw new UnsupportedOperationException();
        }
        try {
            Map<String, String> headers = new HashMap<String, String>();
            List<String> array = mNavigationImpl.getResponseHeaders();
            for (int i = 0; i < array.size(); i += 2) {
                headers.put(array.get(i), array.get(i + 1));
            }

            return headers;
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
     * Whether the target URL can be handled by the browser's internal protocol handlers, i.e., has
     * a scheme that the browser knows how to process internally. Examples of such URLs are
     * http(s) URLs, data URLs, and file URLs. A typical example of a URL for which there is no
     * internal protocol handler (and for which this method would return also) is an intent:// URL.
     *
     * @return Whether the target URL of the navigation has a known protocol.
     *
     * @since 89
     */
    public boolean isKnownProtocol() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.isKnownProtocol();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether this navigation resulted in an external intent being launched. Returns false if this
     * navigation did not do so, or if that status is not yet known for this navigation.  This
     * status is determined for a navigation when processing final (post redirect) HTTP response
     * headers. This means the only time the embedder can know if the navigation resulted in an
     * external intent being launched is in NavigationCallback.onNavigationFailed.
     *
     * @return Whether an intent was launched for the navigation.
     *
     * @since 89
     */
    public boolean wasIntentLaunched() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.wasIntentLaunched();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether this navigation resulted in the user deciding whether an external intent should be
     * launched (e.g., via a dialog). Returns false if this navigation did not resolve to such a
     * user decision, or if that status is not yet known for this navigation.  This status is
     * determined for a navigation when processing final (post redirect) HTTP response headers. This
     * means the only time the embedder can know this status definitively is in
     * NavigationCallback.onNavigationFailed.
     *
     * @return Whether this navigation resulted in a user decision guarding external intent launch.
     *
     * @since 89
     */
    public boolean isUserDecidingIntentLaunch() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.isUserDecidingIntentLaunch();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether this navigation was stopped before it could complete because
     * NavigationController.stop() was called.
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
     * Note that any headers that are set here won't be sent again if the frame html is fetched
     * again due to a user reloading the page, navigating back and forth etc... when this fetch
     * couldn't be cached (either in the disk cache or in the back-forward cache).
     *
     * @param name The name of the header. The name must be rfc 2616 compliant.
     * @param value The value of the header. The value must not contain '\0', '\n' or '\r'.
     *
     * @throws IllegalArgumentException If supplied invalid values.
     * @throws IllegalStateException If not called during start or a redirect.
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
     * Disables auto-reload for this navigation if the network is down and comes back later.
     * Auto-reload is enabled by default. This method may only be called from
     * {@link NavigationCallback.onNavigationStarted}.
     *
     * @throws IllegalStateException If not called during start.
     *
     * @since 88
     */
    public void disableNetworkErrorAutoReload() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.shouldPerformVersionChecks()
                && WebLayer.getSupportedMajorVersionInternal() < 88) {
            throw new UnsupportedOperationException();
        }
        try {
            mNavigationImpl.disableNetworkErrorAutoReload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Disables intent processing for the lifetime of this navigation (including following
     * redirects). This method may only be called from
     * {@link NavigationCallback.onNavigationStarted}.
     *
     * @throws IllegalStateException If not called during start.
     *
     * @since 97
     */
    public void disableIntentProcessing() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.shouldPerformVersionChecks()
                && WebLayer.getSupportedMajorVersionInternal() < 97) {
            throw new UnsupportedOperationException();
        }
        try {
            mNavigationImpl.disableIntentProcessing();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Sets the user-agent string that applies to the current navigation. This user-agent is not
     * sticky, it applies to this navigation only (and any redirects or resources that are loaded).
     * This method may only be called from {@link NavigationCallback.onNavigationStarted}.  Setting
     * this to a non empty string will cause will cause the User-Agent Client Hint header values and
     * the values returned by `navigator.userAgentData` to be empty for requests this override is
     * applied to.
     *
     * Note that this user agent won't be sent again if the frame html is fetched again due to a
     * user reloading the page, navigating back and forth etc... when this fetch couldn't be cached
     * (either in the disk cache or in the back-forward cache).
     *
     * @param value The user-agent string. The value must not contain '\0', '\n' or '\r'. An empty
     * string results in the default user-agent string.
     *
     * @throws IllegalArgumentException If supplied an invalid value.
     * @throws IllegalStateException If not called during start or if {@link
     *         Tab.setDesktopUserAgent} was called with a value of true.
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
     * * window.history.forward() or window.history.back()
     *
     * This method returns false for navigations initiated by the WebLayer API.
     *
     * @return Whether the navigation was initiated by the page.
     */
    public boolean isPageInitiated() {
        ThreadCheck.ensureOnUiThread();
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
     */
    public boolean isReload() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationImpl.isReload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether the navigation is restoring a page from back-forward cache (see
     * https://web.dev/bfcache/). Since a previously loaded page is being reused, there are some
     * things embedders have to keep in mind such as:
     *   * there will be no NavigationObserver::onFirstContentfulPaint callbacks
     *   * if an embedder injects code using Tab::ExecuteScript there is no need to reinject scripts
     *
     * @since 89
     */
    public boolean isServedFromBackForwardCache() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.isServedFromBackForwardCache();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if this navigation was initiated by a form submission.
     *
     * @since 89
     */
    public boolean isFormSubmission() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89
                || WebLayer.getVersion().equals("89.0.4389.69")
                || WebLayer.getVersion().equals("89.0.4389.72")) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.isFormSubmission();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the referrer for this request.
     *
     * @since 89
     */
    @NonNull
    public Uri getReferrer() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 89
                || WebLayer.getVersion().equals("89.0.4389.69")
                || WebLayer.getVersion().equals("89.0.4389.72")) {
            throw new UnsupportedOperationException();
        }
        try {
            return Uri.parse(mNavigationImpl.getReferrer());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the Page object this navigation is occurring for.
     * This method may only be called in (1) {@link NavigationCallback.onNavigationCompleted} or
     * (2) {@link NavigationCallback.onNavigationFailed} when {@link Navigation#isErrorPage}
     * returns true. It will return a non-null object in these cases.
     *
     * @throws IllegalStateException if called outside the above circumstances.
     *
     * @since 90
     */
    @NonNull
    public Page getPage() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 90) {
            throw new UnsupportedOperationException();
        }
        try {
            return (Page) mNavigationImpl.getPage();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the offset between the indices of the previous last committed and the newly committed
     * navigation entries, for example -1 for back navigations, 0 for reloads, 1 for forward
     * navigations or new navigations. Note that the return value can be less than -1 or greater
     * than 1 if the navigation goes back/forward multiple entries. This may not cover all corner
     * cases, and can be incorrect in cases like main frame client redirects.
     *
     * @since 92
     */
    public int getNavigationEntryOffset() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 92) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.getNavigationEntryOffset();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if the navigation response was fetched from the cache.
     *
     * @since 102
     */
    public boolean wasFetchedFromCache() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 102) {
            throw new UnsupportedOperationException();
        }
        try {
            return mNavigationImpl.wasFetchedFromCache();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
