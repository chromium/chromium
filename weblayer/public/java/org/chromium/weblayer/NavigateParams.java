// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.webkit.WebResourceResponse;

import androidx.annotation.NonNull;

/**
 * Parameters for {@link NavigationController#navigate}.
 *
 * @since 83
 */
public class NavigateParams {
    private org.chromium.weblayer_private.interfaces.NavigateParams mInterfaceParams =
            new org.chromium.weblayer_private.interfaces.NavigateParams();
    private boolean mIntentProcessingDisabled;
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
            mParams.mInterfaceParams.mShouldReplaceCurrentEntry = replace;
            return this;
        }

        /**
         * Disables lookup and launching of an Intent that matches the uri being navigated to. If
         * this is not called, WebLayer may look for a matching intent-filter, and if one is found,
         * create and launch an Intent. The exact heuristics of when Intent matching is performed
         * depends upon a wide range of state (such as the uri being navigated to, navigation
         * stack...).
         *
         * @since 86
         */
        @NonNull
        public Builder disableIntentProcessing() {
            if (WebLayer.shouldPerformVersionChecks()
                    && WebLayer.getSupportedMajorVersionInternal() < 86) {
                throw new UnsupportedOperationException();
            }
            mParams.mIntentProcessingDisabled = true;
            return this;
        }

        /**
         * Disables auto-reload for this navigation if the network is down and comes back later.
         *          Auto-reload is enabled by default.
         *
         * @since 86
         */
        @NonNull
        public Builder disableNetworkErrorAutoReload() {
            if (WebLayer.shouldPerformVersionChecks()
                    && WebLayer.getSupportedMajorVersionInternal() < 86) {
                throw new UnsupportedOperationException();
            }
            mParams.mNetworkErrorAutoReloadDisabled = true;
            return this;
        }

        /**
         * Enable auto-play for videos in this navigation. Auto-play is disabled by default.
         *
         * @since 86
         */
        @NonNull
        public Builder enableAutoPlay() {
            if (WebLayer.shouldPerformVersionChecks()
                    && WebLayer.getSupportedMajorVersionInternal() < 86) {
                throw new UnsupportedOperationException();
            }
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
         *                  information to show the security padlock or certificate data. As such an
         *                  exception is thrown if this is set when a View from UrlBarController is
         *                  attached to a window.
         *
         * @since 87
         */
        @NonNull
        public Builder setResponse(@NonNull WebResourceResponse response) {
            if (WebLayer.getSupportedMajorVersionInternal() < 87) {
                throw new UnsupportedOperationException();
            }
            mParams.mResponse = response;
            return this;
        }
    }

    org.chromium.weblayer_private.interfaces.NavigateParams toInterfaceParams() {
        return mInterfaceParams;
    }

    /**
     * Returns true if the current navigation will be replaced, false otherwise.
     *
     * @since 83
     */
    public boolean getShouldReplaceCurrentEntry() {
        return mInterfaceParams.mShouldReplaceCurrentEntry;
    }

    /**
     * Returns true if intent processing is disabled.
     *
     * @return Whether intent processing is disabled.
     *
     * @since 86
     */
    public boolean isIntentProcessingDisabled() {
        if (WebLayer.shouldPerformVersionChecks()
                && WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        return mIntentProcessingDisabled;
    }

    /**
     * Returns true if auto reload for network errors is disabled.
     *
     * @return Whether auto reload for network errors is disabled.
     *
     * @since 86
     */
    public boolean isNetworkErrorAutoReloadDisabled() {
        if (WebLayer.shouldPerformVersionChecks()
                && WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        return mNetworkErrorAutoReloadDisabled;
    }

    /**
     * Returns true if auto play for videos is enabled.
     *
     * @return Whether auto play for videos is enabled.
     *
     * @since 86
     */
    public boolean isAutoPlayEnabled() {
        if (WebLayer.shouldPerformVersionChecks()
                && WebLayer.getSupportedMajorVersionInternal() < 86) {
            throw new UnsupportedOperationException();
        }
        return mAutoPlayEnabled;
    }

    /**
     * Returns a response of the html to use.
     *
     * @return WebResourceResponse of html to use.
     *
     * @since 87
     */
    WebResourceResponse getResponse() {
        return mResponse;
    }
}
