// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Bundle;
import android.util.AndroidRuntimeException;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.UrlBarOptionsKeys;

/**
 * Class containing options to tweak the URL bar.
 */
final class UrlBarOptions {
    public static Builder builder() {
        return new Builder();
    }

    private Bundle mOptions;
    private OnClickListener mTextClickListener;
    private OnLongClickListener mTextLongClickListener;

    /**
     * A Builder class to help create UrlBarOptions.
     */
    public static final class Builder {
        private Bundle mOptions;
        private OnClickListener mTextClickListener;
        private OnLongClickListener mTextLongClickListener;

        private Builder() {
            mOptions = new Bundle();
        }

        Bundle getBundle() {
            return mOptions;
        }

        OnClickListener getTextClickListener() {
            return mTextClickListener;
        }

        OnLongClickListener getTextLongClickListener() {
            return mTextLongClickListener;
        }

        /**
         * Sets the text size of the URL bar.
         *
         * @param textSize The desired size of the URL bar text in scalable pixels.
         * The default is 14.0F and the minimum allowed size is 5.0F.
         */
        @NonNull
        public Builder setTextSizeSP(float textSize) {
            mOptions.putFloat(UrlBarOptionsKeys.URL_TEXT_SIZE, textSize);
            return this;
        }

        /**
         * Specifies whether the URL text in the URL bar should also show Page Info UI on click.
         * By default, only the security status icon does so.
         */
        @NonNull
        public Builder showPageInfoWhenTextIsClicked() {
            mOptions.putBoolean(UrlBarOptionsKeys.SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED, true);
            return this;
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
         * Sets the color of the URL bar text.
         *
         * @param textColor The color for the Url bar text.
         */
        @NonNull
        public Builder setTextColor(@ColorRes int textColor) {
            mOptions.putInt(UrlBarOptionsKeys.URL_TEXT_COLOR, textColor);
            return this;
        }

        /**
         * Sets the color of the URL bar security status icon.
         *
         * @param iconColor The color for the Url bar icon.
         */
        @NonNull
        public Builder setIconColor(@ColorRes int iconColor) {
            mOptions.putInt(UrlBarOptionsKeys.URL_ICON_COLOR, iconColor);
            return this;
        }

        @NonNull
        public Builder setTextClickListener(@NonNull OnClickListener clickListener) {
            mTextClickListener = clickListener;
            return this;
        }

        @NonNull
        public Builder setTextLongClickListener(@NonNull OnLongClickListener longClickListener) {
            mTextLongClickListener = longClickListener;
            return this;
        }

        /**
         * Builds a UrlBarOptions object.
         */
        @NonNull
        public UrlBarOptions build() {
            boolean showPageInfoWhenUrlTextClicked = mOptions.getBoolean(
                    UrlBarOptionsKeys.SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED, /*default= */ false);
            if (mTextClickListener != null && showPageInfoWhenUrlTextClicked) {
                throw new AndroidRuntimeException("Text click listener cannot be set when "
                        + "SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED is true.");
            }
            return new UrlBarOptions(this);
        }
    }

    private UrlBarOptions(Builder builder) {
        mOptions = builder.getBundle();
        mTextClickListener = builder.getTextClickListener();
        mTextLongClickListener = builder.getTextLongClickListener();
    }

    /**
     * Gets the URL bar options as a Bundle.
     */
    Bundle getBundle() {
        return mOptions;
    }

    /**
     * Gets the text size of the URL bar text in scalable pixels.
     */
    public float getTextSizeSP() {
        return mOptions.getFloat(UrlBarOptionsKeys.URL_TEXT_SIZE);
    }

    /**
     * Gets the ClickListener.
     */
    OnClickListener getTextClickListener() {
        return mTextClickListener;
    }

    /**
     * Gets the LongClickListener.
     */
    OnLongClickListener getTextLongClickListener() {
        return mTextLongClickListener;
    }
}
