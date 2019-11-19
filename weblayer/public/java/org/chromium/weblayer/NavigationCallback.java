// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Informed of interesting events that happen during the lifetime of NavigationController. This
 * interface is only notified of main frame navigations.
 *
 * The lifecycle of a navigation:
 * 1) navigationStarted()
 * 2) 0 or more navigationRedirected()
 * 3) 0 or 1 readyToCommitNavigation()
 * 4) navigationCompleted() or navigationFailed()
 * 5) onFirstContentfulPaint
 */
public abstract class NavigationCallback {
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
     * Called when the navigation is ready to be committed in a renderer. This occurs when the
     * response code isn't 204/205 (which tell the browser that the request is successful but
     * there's no content that follows) or a download (either from a response header or based on
     * mime sniffing the response). The browser then is ready to switch rendering the new document.
     * Most observers should use NavigationCompleted or NavigationFailed instead, which happens
     * right after the navigation commits. This method is for observers that want to initialize
     * renderer-side state just before the Tab commits the navigation.
     *
     * This is the first point in time where a Tab is associated with the navigation.
     *
     * @param navigation the unique object for this navigation.
     */
    public void onReadyToCommitNavigation(@NonNull Navigation navigation) {}

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
     * @param toDifferentDocument True if the main frame is loading a different document. Only valid
     *        when |isLoading| is true.
     */
    public void onLoadStateChanged(boolean isLoading, boolean toDifferentDocument) {}

    /**
     * The progress of loading the main frame in the document has changed.
     *
     * @param progress A value in the range of 0.0-1.0.
     */
    public void onLoadProgressChanged(double progress) {}

    /**
     * This is fired after each navigation has completed to indicate that the first paint after a
     * non-empty layout has finished.
     */
    public void onFirstContentfulPaint() {}
}
