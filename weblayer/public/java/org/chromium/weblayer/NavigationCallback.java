// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * Informed of interesting events that happen during the lifetime of NavigationController. This
 * interface is only notified of main frame navigations.
 *
 * The lifecycle of a navigation:
 * 1) navigationStarted()
 * 2) 0 or more navigationRedirected()
 * 3) navigationCompleted() or navigationFailed()
 * 4) onFirstContentfulPaint().
 */
abstract class NavigationCallback {
    /**
     * Called when a navigation started in the Tab. |navigation| is unique to a
     * specific navigation. The same |navigation| will be  provided on subsequent calls to
     * NavigationRedirected, NavigationCommitted, NavigationCompleted and NavigationFailed when
     * related to this navigation. Observers should clear any references to |navigation| in
     * NavigationCompleted or NavigationFailed, just before it is destroyed.
     *
     * Note that this is only fired by navigations in the main frame.
     *
     * Note that this is fired by same-document navigations, such as fragment navigations or
     * pushState/replaceState, which will not result in a document change. To filter these out, use
     * Navigation::IsSameDocument.
     *
     * Note that more than one navigation can be ongoing in the Tab at the same time.
     * Each will get its own Navigation object.
     *
     * Note that there is no guarantee that NavigationCompleted/NavigationFailed will be called for
     * any particular navigation before NavigationStarted is called on the next.
     *
     * @param navigation the unique object for this navigation.
     */
    public void onNavigationStarted(@NonNull Navigation navigation) {}

    /**
     * Called when a navigation encountered a server redirect.
     *
     * @param navigation the unique object for this navigation.
     */
    public void onNavigationRedirected(@NonNull Navigation navigation) {}

    /**
     * Called when a navigation completes successfully in the Tab.
     *
     * The document load will still be ongoing in the Tab. Use the document loads
     * events such as onFirstContentfulPaint and related methods to listen for continued events from
     * this Tab.
     *
     * Note that this is fired by same-document navigations, such as fragment navigations or
     * pushState/replaceState, which will not result in a document change. To filter these out, use
     * NavigationHandle::IsSameDocument.
     *
     * Note that |navigation| will be destroyed at the end of this call, so do not keep a reference
     * to it afterward.
     *
     * @param navigation the unique object for this navigation.
     */
    public void onNavigationCompleted(@NonNull Navigation navigation) {}

    /**
     * Called when a navigation aborts in the Tab.
     *
     * Note that |navigation| will be destroyed at the end of this call, so do not keep a reference
     * to it afterward.
     *
     * @param navigation the unique object for this navigation.
     */
    public void onNavigationFailed(@NonNull Navigation navigation) {}

    /**
     * The load state of the document has changed.
     *
     * @param isLoading Whether any resource is loading.
     * @param shouldShowLoadingUi True if the navigation is expected to show navigation-in-progress
     *        UI (if any exists). Only valid when |isLoading| is true.
     */
    public void onLoadStateChanged(boolean isLoading, boolean shouldShowLoadingUi) {}

    /**
     * The progress of loading the main frame in the document has changed.
     *
     * @param progress A value in the range of 0.0-1.0.
     */
    public void onLoadProgressChanged(double progress) {}

    /**
     * This is fired after each navigation has completed to indicate that the first paint after a
     * non-empty layout has finished. This is *not* called for same-document navigations or when the
     * page is loaded from the back-forward cache; see {@link
     * Navigation#isServedFromBackForwardCache}.
     */
    public void onFirstContentfulPaint() {}

    /**
     * Similar to onFirstContentfulPaint but contains timing information from the renderer process
     * to better align with the Navigation Timing API.
     *
     * @param navigationStartMs the absolute navigation start time in milliseconds since boot,
     *        not counting time spent in deep sleep. This comes from SystemClock.uptimeMillis().
     * @param firstContentfulPaintDurationMs the number of milliseconds to first contentful paint
     *        from navigation start.
     * @since 88
     */
    public void onFirstContentfulPaint(
            long navigationStartMs, long firstContentfulPaintDurationMs) {}

    /**
     * This is fired when the largest contentful paint metric is available.
     *
     * @param navigationStartMs the absolute navigation start time in milliseconds since boot,
     *        not counting time spent in deep sleep. This comes from SystemClock.uptimeMillis().
     * @param largestContentfulPaintDurationMs the number of milliseconds to largest contentful
     *         paint
     *        from navigation start.
     * @since 88
     */
    public void onLargestContentfulPaint(
            long navigationStartMs, long largestContentfulPaintDurationMs) {}

    /**
     * Called after each navigation to indicate that the old page is no longer
     * being rendered. Note this is not ordered with respect to onFirstContentfulPaint.
     * @param newNavigationUri Uri of the new navigation.
     */
    public void onOldPageNoLongerRendered(@NonNull Uri newNavigationUri) {}

    /* Called when a Page is destroyed. For the common case, this is called when the user navigates
     * away from a page to a new one or when the Tab is destroyed. However there are situations when
     * a page is alive when it's not visible, e.g. when it goes into the back-forward cache. In that
     * case this method will either be called when the back-forward cache entry is evicted or if it
     * is used then this cycle repeats.
     * @since 90
     */
    public void onPageDestroyed(@NonNull Page page) {}

    /*
     * Called when the source language for |page| has been determined to be |language|.
     * Note: |language| is an ISO 639 language code (two letters, except for Chinese where a
     * localization is necessary).
     * @since 93
     */
    public void onPageLanguageDetermined(@NonNull Page page, @NonNull String language) {}
}
