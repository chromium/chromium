// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.view.View;
import android.widget.EditText;
import android.widget.ProgressBar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.TabObserver;

/**
 * Default observers for Test Acivites.
 */
public class DefaultObservers {
    private static final String TAG = "WEDefaultObservers";

    static class DefaultTabObserver extends TabObserver {
        @Nullable
        private EditText mUrlBar;
        @Nullable
        private Tab mTab;
        @Nullable
        private TabManager mTabManager;

        public DefaultTabObserver(
                @Nullable EditText urlBar, @Nullable Tab tab, @Nullable TabManager tabManager) {
            mUrlBar = urlBar;
            mTab = tab;
            mTabManager = tabManager;
        }
        public DefaultTabObserver() {}

        @Override
        public void onVisibleUriChanged(@NonNull String uri) {
            Log.i(TAG, "received Tab Event: 'onVisibleUriChanged(" + uri + ")'");
            if (mUrlBar == null) {
                return;
            }
            if (mTabManager == null || mTabManager.getActiveTab() == null
                    || !mTabManager.getActiveTab().equals(mTab)) {
                return;
            }
            mUrlBar.setText(uri);
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
        @Nullable
        private ProgressBar mProgressBar;
        @Nullable
        private Tab mTab;
        @Nullable
        private TabManager mTabManager;

        public DefaultNavigationObserver(@Nullable ProgressBar progressBar, @Nullable Tab tab,
                @Nullable TabManager tabManager) {
            mProgressBar = progressBar;
            mTab = tab;
            mTabManager = tabManager;
        }
        public DefaultNavigationObserver() {}

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

        @Override
        public void onLoadProgressChanged(double progress) {
            super.onLoadProgressChanged(progress);
            int progressValue = (int) Math.rint(progress * 100);
            Log.i(TAG, "received NavigationEvent: 'onLoadProgressChanged()';");
            if (mProgressBar == null) {
                return;
            }
            if (mTabManager == null || mTabManager.getActiveTab() == null
                    || !mTabManager.getActiveTab().equals(mTab)) {
                return;
            }
            if (progressValue != mProgressBar.getMax()) {
                mProgressBar.setVisibility(View.VISIBLE);
            } else {
                mProgressBar.setVisibility(View.INVISIBLE);
            }
            mProgressBar.setProgress(progressValue);
        }
    }

    static class DefaultTabListObserver extends TabListObserver {
        @Nullable
        private ProgressBar mProgressBar;
        @Nullable
        private EditText mUrlBar;
        @Nullable
        private TabManager mTabManager;
        public DefaultTabListObserver(@Nullable ProgressBar progressBar, @Nullable EditText urlBar,
                @Nullable TabManager tabManager) {
            mProgressBar = progressBar;
            mUrlBar = urlBar;
            mTabManager = tabManager;
        }
        public DefaultTabListObserver() {}

        @Override
        public void onActiveTabChanged(@Nullable Tab activeTab) {
            Log.i(TAG, "received TabList Event: 'onActiveTabChanged'-event");
        }

        @Override
        public void onTabAdded(@NonNull Tab tab) {
            Log.i(TAG, "received TabList Event: 'onTabAdded'-event");

            // Recursively add tab and navigation observers to any new tab.
            tab.registerTabObserver(new DefaultTabObserver(mUrlBar, tab, mTabManager));
            tab.getNavigationController().registerNavigationObserver(
                    new DefaultNavigationObserver(mProgressBar, tab, mTabManager));
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
