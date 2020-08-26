// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

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
         * @since 87
         */
        @NonNull
        public Builder disableIntentProcessing() {
            if (WebLayer.getSupportedMajorVersionInternal() < 87) {
                throw new UnsupportedOperationException();
            }
            mParams.mIntentProcessingDisabled = true;
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
     * @return Whether intent process is disabled.
     *
     * @since 87
     */
    public boolean isIntentProcessingDisabled() {
        if (WebLayer.getSupportedMajorVersionInternal() < 87) {
            throw new UnsupportedOperationException();
        }
        return mIntentProcessingDisabled;
    }
}
