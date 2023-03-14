// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;

/**
 * An interface for observing changes to a Tab.
 */
public interface NavigationObserver {
    /**
     * Called when a navigation aborts in the Tab.
     *
     * Note that |navigation| is a snapshot of the current navigation, content might change over the
     * course of the navigation.
     *
     * @param tab the tab associated with this event.
     * @param navigation the unique object for this navigation.
     */
    public default void onNavigationFailed(@NonNull Tab tab, @NonNull Navigation navigation) {}

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
     * Note that |navigation| is a snapshot of the current navigation, content might change over the
     * course of the navigation.
     *
     * @param tab the tab associated with this event.
     * @param navigation the unique object for this navigation.
     */
    public default void onNavigationCompleted(@NonNull Tab tab, @NonNull Navigation navigation) {}

    /**
     * Called when a navigation started in the Tab. |navigation| is unique to a
     * specific navigation.
     *
     * Note that this is only fired by navigations in the main frame.
     *
     * Note that this is fired by same-document navigations, such as fragment navigations or
     * pushState/replaceState, which will not result in a document change. To filter these out, use
     * Navigation::IsSameDocument.
     *
     * Note that |navigation| is a snapshot of the current navigation, content might change over the
     * course of the navigation.
     *
     * Note that there is no guarantee that NavigationCompleted/NavigationFailed will be called for
     * any particular navigation before NavigationStarted is called on the next.
     *
     * @param tab the tab associated with this event.
     * @param navigation the unique object for this navigation.
     */
    public default void onNavigationStarted(@NonNull Tab tab, @NonNull Navigation navigation) {}

    /**
     * Called when a navigation encountered a server redirect.
     *
     * @param tab the tab associated with this event.
     * @param navigation the unique object for this navigation.
     */
    public default void onNavigationRedirected(@NonNull Tab tab, @NonNull Navigation navigation) {}

    /**
     * The load state of the document has changed.
     *
     * @param tab the tab associated with this event.
     * @param progress The loading progress.
     */
    public default void onLoadProgressChanged(@NonNull Tab tab, double progress) {}
}