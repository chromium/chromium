// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabObserver;

/**
 * Default observers for Test Acivites.
 */
public class DefaultObservers {
    private static final String TAG = "WEDefaultObservers";

    static class DefaultTabObserver extends TabObserver {
        @Override
        public void onVisibleUriChanged(@NonNull String uri) {
            Log.i(TAG, "received Tab Event: 'onVisibleUriChanged(" + uri + ")'");
        }

        @Override
        public void onTitleUpdated(@NonNull String title) {
            Log.i(TAG, "received Tab Event: 'onTitleUpdated(" + title + ")'");
        }

        @Override
        public void onRenderProcessGone() {
            Log.i(TAG, "received Tab Event: 'onRenderProcessGone()'");
        }
    }

    static class DefaultNavigationObserver extends NavigationObserver {
        @Override
        public void onNavigationFailed(@NonNull Navigation navigation) {
            Log.i(TAG, "received NavigationEvent: 'onNavigationFailed()';");
            Log.i(TAG,
                    "Navigation: url:" + navigation.getUri()
                            + ", HTTP-StatusCode: " + navigation.getStatusCode()
                            + ", samePage: " + navigation.isSameDocument());
        }

        @Override
        public void onNavigationCompleted(@NonNull Navigation navigation) {
            Log.i(TAG, "received NavigationEvent: 'onNavigationCompleted()';");
            Log.i(TAG,
                    "Navigation: url:" + navigation.getUri()
                            + ", HTTP-StatusCode: " + navigation.getStatusCode()
                            + ", samePage: " + navigation.isSameDocument());
        }
    }

    static class DefaultTabListObserver extends TabListObserver {
        @Override
        public void onActiveTabChanged(@Nullable Tab activeTab) {
            Log.i(TAG, "received TabList Event: 'onActiveTabChanged'-event");
        }

        @Override
        public void onTabAdded(@NonNull Tab tab) {
            Log.i(TAG, "received TabList Event: 'onTabAdded'-event");

            // Recursively add tab and navigation observers to any new tab.
            tab.registerTabObserver(new DefaultTabObserver());
            tab.getNavigationController().registerNavigationObserver(
                    new DefaultNavigationObserver());
        }

        @Override
        public void onTabRemoved(@NonNull Tab tab) {
            Log.i(TAG, "received TabList Event: 'onTabRemoved'-event");
        }

        @Override
        public void onWillDestroyFragmentAndAllTabs() {
            Log.i(TAG, "received TabList Event: 'onWillDestroyFragmentAndAllTabs'-event");
        }
    }
}