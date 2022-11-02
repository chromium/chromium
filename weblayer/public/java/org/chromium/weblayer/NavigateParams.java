// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.webkit.WebResourceResponse;

import androidx.annotation.NonNull;

/**
 * Parameters for {@link NavigationController#navigate}.
 */
class NavigateParams {
    private boolean mShouldReplaceCurrentEntry;
    private boolean mIntentProcessingDisabled;
    private boolean mIntentLaunchesAllowedInBackground;
    private boolean mNetworkErrorAutoReloadDisabled;
    private boolean mAutoPlayEnabled;
    private WebResourceResponse mResponse;

    /**
     * A Builder class to help create NavigateParams.
     */
    public static final class Builder {
        private NavigateParams mParams;

        /**
         * Constructs a new Builder.
         */
        public Builder() {
            mParams = new NavigateParams();
        }

        /**
         * Builds the NavigateParams.
         */
        @NonNull
        public NavigateParams build() {
            return mParams;
        }

        /**
         * @param replace Indicates whether the navigation should replace the current navigation
         *         entry in the history stack. False by default.
         */
        @NonNull
        public Builder setShouldReplaceCurrentEntry(boolean replace) {
            mParams.mShouldReplaceCurrentEntry = replace;
            return this;
        }

        /**
         * Disables lookup and launching of an Intent that matches the uri being navigated to. If
         * this is not called, WebLayer may look for a matching intent-filter, and if one is found,
         * create and launch an Intent. The exact heuristics of when Intent matching is performed
         * depends upon a wide range of state (such as the uri being navigated to, navigation
         * stack...).
         */
        @NonNull
        public Builder disableIntentProcessing() {
            mParams.mIntentProcessingDisabled = true;
            return this;
        }

        /**
         * Enables intent launching to occur for this navigation even if it is being executed in a
         * background (i.e., non-visible) tab (by default, intent launches are disallowed in
         * background tabs).
         *
         * @since 89
         */
        @NonNull
        public Builder allowIntentLaunchesInBackground() {
            if (WebLayer.shouldPerformVersionChecks()
                    && WebLayer.getSupportedMajorVersionInternal() < 89) {
                throw new UnsupportedOperationException();
            }

            mParams.mIntentLaunchesAllowedInBackground = true;
            return this;
        }

        /**
         * Disables auto-reload for this navigation if the network is down and comes back later.
         * Auto-reload is enabled by default. This is deprecated as of 88, instead use
         * {@link Navigation#disableNetworkErrorAutoReload} which works for both embedder-initiated
         * navigations and also user-initiated navigations (such as back or forward). Auto-reload
         * is disabled if either method is called.
         */
        @NonNull
        public Builder disableNetworkErrorAutoReload() {
            mParams.mNetworkErrorAutoReloadDisabled = true;
            return this;
        }

        /**
         * Enable auto-play for videos in this navigation. Auto-play is disabled by default.
         */
        @NonNull
        public Builder enableAutoPlay() {
            mParams.mAutoPlayEnabled = true;
            return this;
        }

        /**
         * @param response If the embedder has already fetched the data for a navigation it can
         *         supply it via a WebResourceResponse. The navigation will be committed at the
         *         Uri given to NavigationController.navigate().
         *         Caveats:
         *             -ensure proper cache headers are set if you don't want it to be reloaded.
         *                  Depending on the device available memory a back navigation might hit
         *                  the network if the headers don't indicate it's cacheable and the
         *                  page wasn't in the back-forward cache. An example to cache for 1 minute:
         *                      Cache-Control: private, max-age=60
         *             -since this isn't fetched by WebLayer it won't have the necessary certificate
         *                  information to show the security padlock or certificate data.
         */
        @NonNull
        public Builder setResponse(@NonNull WebResourceResponse response) {
            mParams.mResponse = response;
            return this;
        }
    }

    /**
     * Returns true if the current navigation will be replaced, false otherwise.
     */
    public boolean getShouldReplaceCurrentEntry() {
        return mShouldReplaceCurrentEntry;
    }

    /**
     * Returns true if intent processing is disabled.
     *
     * @return Whether intent processing is disabled.
     */
    public boolean isIntentProcessingDisabled() {
        return mIntentProcessingDisabled;
    }

    /**
     * Returns true if intent launches are allowed in the background.
     *
     * @return Whether intent launches are allowed in the background.
     *
     * @since 89
     */
    public boolean areIntentLaunchesAllowedInBackground() {
        if (WebLayer.shouldPerformVersionChecks()
                && WebLayer.getSupportedMajorVersionInternal() < 89) {
            throw new UnsupportedOperationException();
        }

        return mIntentLaunchesAllowedInBackground;
    }

    /**
     * Returns true if auto reload for network errors is disabled.
     *
     * @return Whether auto reload for network errors is disabled.
     */
    public boolean isNetworkErrorAutoReloadDisabled() {
        return mNetworkErrorAutoReloadDisabled;
    }

    /**
     * Returns true if auto play for videos is enabled.
     *
     * @return Whether auto play for videos is enabled.
     */
    public boolean isAutoPlayEnabled() {
        return mAutoPlayEnabled;
    }

    /**
     * Returns a response of the html to use.
     *
     * @return WebResourceResponse of html to use.
     */
    public WebResourceResponse getResponse() {
        return mResponse;
    }
}
