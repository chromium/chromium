// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabObserver;
import org.chromium.webengine.WebEngine;

/**
 * Default observers for Test Activities.
 */
public class DefaultObservers implements TabListObserver, TabObserver, NavigationObserver {
    private static final String TAG = "WEDefaultObservers";

    // TabObserver implementation.

    @Override
    public void onVisibleUriChanged(@NonNull Tab tab, @NonNull String uri) {
        Log.i(TAG, this + "received Tab Event: 'onVisibleUriChanged(" + uri + ")'");
    }

    @Override
    public void onTitleUpdated(@NonNull Tab tab, @NonNull String title) {
        Log.i(TAG, this + "received Tab Event: 'onTitleUpdated(" + title + ")'");
    }

    @Override
    public void onRenderProcessGone(@NonNull Tab tab) {
        Log.i(TAG, this + "received Tab Event: 'onRenderProcessGone()'");
    }

    @Override
    public void onFaviconChanged(@NonNull Tab tab, @Nullable Bitmap favicon) {
        Log.i(TAG,
                this + "received Tab Event: 'onFaviconChanged("
                        + (favicon == null ? "null" : favicon.toString()) + ")'");
    }

    // NavigationObserver implementation.

    @Override
    public void onNavigationFailed(@NonNull Tab tab, @NonNull Navigation navigation) {
        Log.i(TAG, this + "received NavigationEvent: 'onNavigationFailed()';");
        Log.i(TAG,
                this + "Navigation: url:" + navigation.getUri()
                        + ", HTTP-StatusCode: " + navigation.getStatusCode()
                        + ", samePage: " + navigation.isSameDocument());
    }

    @Override
    public void onNavigationCompleted(@NonNull Tab tab, @NonNull Navigation navigation) {
        Log.i(TAG, this + "received NavigationEvent: 'onNavigationCompleted()';");
        Log.i(TAG,
                this + "Navigation: url:" + navigation.getUri()
                        + ", HTTP-StatusCode: " + navigation.getStatusCode()
                        + ", samePage: " + navigation.isSameDocument());
    }

    @Override
    public void onNavigationStarted(@NonNull Tab tab, @NonNull Navigation navigation) {
        Log.i(TAG, this + "received NavigationEvent: 'onNavigationStarted()';");
    }

    @Override
    public void onNavigationRedirected(@NonNull Tab tab, @NonNull Navigation navigation) {
        Log.i(TAG, this + "received NavigationEvent: 'onNavigationRedirected()';");
    }

    @Override
    public void onLoadProgressChanged(@NonNull Tab tab, double progress) {
        Log.i(TAG, this + "received NavigationEvent: 'onLoadProgressChanged()';");
    }

    // TabListObserver implementation.

    @Override
    public void onActiveTabChanged(@NonNull WebEngine webEngine, @Nullable Tab activeTab) {
        Log.i(TAG, this + "received TabList Event: 'onActiveTabChanged'-event");
    }

    @Override
    public void onTabAdded(@NonNull WebEngine webEngine, @NonNull Tab tab) {
        Log.i(TAG, this + "received TabList Event: 'onTabAdded'-event");
        // Recursively add tab and navigation observers to any new tab.
        tab.registerTabObserver(this);
        tab.getNavigationController().registerNavigationObserver(this);
    }

    @Override
    public void onTabRemoved(@NonNull WebEngine webEngine, @NonNull Tab tab) {
        Log.i(TAG, this + "received TabList Event: 'onTabRemoved'-event");
    }

    @Override
    public void onWillDestroyFragmentAndAllTabs(@NonNull WebEngine webEngine) {
        Log.i(TAG, this + "received TabList Event: 'onWillDestroyFragmentAndAllTabs'-event");
    }
}
