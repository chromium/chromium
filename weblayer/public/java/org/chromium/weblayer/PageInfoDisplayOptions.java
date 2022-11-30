// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.UrlBarOptionsKeys;

/**
 * Builds the options used to configure how the url bar shows page info.
 *
 * @since 95
 */
final class PageInfoDisplayOptions {
    public static Builder builder() {
        return new Builder();
    }

    private Bundle mOptions;

    /**
     * A Builder class to help create PageInfoDisplayOptions.
     */
    public static final class Builder {
        private Bundle mOptions;

        private Builder() {
            mOptions = new Bundle();
        }

        Bundle getBundle() {
            return mOptions;
        }

        /**
         * Specifies whether the publisher URL is shown.
         */
        @NonNull
        public Builder showPublisherUrl() {
            mOptions.putBoolean(UrlBarOptionsKeys.SHOW_PUBLISHER_URL, true);
            return this;
        }

        /**
         * Builds a PageInfoDisplayOptions object.
         */
        @NonNull
        public PageInfoDisplayOptions build() {
            return new PageInfoDisplayOptions(mOptions);
        }
    }

    private PageInfoDisplayOptions(Bundle options) {
        mOptions = options;
    }

    /**
     * Returns the page info display options as a Bundle.
     */
    Bundle getBundle() {
        return mOptions;
    }

    /**
     * Returns whether the publisher URL is shown.
     */
    public boolean getShowPublisherUrl() {
        return mOptions.getBoolean(UrlBarOptionsKeys.SHOW_PUBLISHER_URL, false);
    }
}
