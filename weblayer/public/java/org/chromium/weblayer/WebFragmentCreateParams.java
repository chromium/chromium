// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

class WebFragmentCreateParams {
    private boolean mIncognito;
    @Nullable
    private String mProfileName;
    @Nullable
    private String mPersistenceId;
    private boolean mUseViewModel;

    /**
     * A Builder class to help create WebFragmentCreateParams.
     */
    public static final class Builder {
        @NonNull
        private WebFragmentCreateParams mParams;

        /**
         * Constructs a new Builder.
         */
        public Builder() {
            mParams = new WebFragmentCreateParams();
        }

        /**
         * Builds the WebFragmentCreateParams.
         */
        @NonNull
        public WebFragmentCreateParams build() {
            return mParams;
        }

        /**
         * Sets whether the profile is incognito.
         *
         * Support for incognito fragments with a non-null and non-empty profile name was added
         * in 88. Attempting to use a fragment with a non-null and non-empty profile name earlier
         * than 88 will result in an exception.
         *
         * @param incognito Whether the profile should be incognito.
         */
        @NonNull
        public Builder setIsIncognito(boolean incognito) {
            mParams.mIncognito = incognito;
            return this;
        }

        /**
         *
         * Sets the name of the profile. Null or empty string implicitly creates an incognito
         * profile. If {@code profile} must only contain alphanumeric and underscore characters
         * since it will be used as a directory name in the file system.
         *
         * @param The name of the profile.
         */
        @NonNull
        public Builder setProfileName(@Nullable String name) {
            mParams.mProfileName = name;
            return this;
        }

        /**
         * Sets the persistence id, which uniquely identifies the Browser for saving the set of tabs
         * and navigations. A value of null does not save/restore any state. A non-null value
         * results in asynchronously restoring the tabs and navigations. Supplying a non-null value
         * means the Browser initially has no tabs (until restore is complete).
         *
         * @param id The id for persistence.
         */
        @NonNull
        public Builder setPersistenceId(@Nullable String id) {
            mParams.mPersistenceId = id;
            return this;
        }

        /**
         * Sets whether the Browser should be stored in a ViewModel owned by the Fragment. A value
         * of true results in the Browser not being recreated during configuration changes. This is
         * a replacement for {@code Fragment#setRetainInstance()}.
         *
         * @param useViewModel Whether a ViewModel should be used.
         */
        @NonNull
        public Builder setUseViewModel(boolean useViewModel) {
            mParams.mUseViewModel = useViewModel;
            return this;
        }
    }

    /**
     * Returns whether the Browser is incognito.
     *
     * @return True if the profile is incognito.
     */
    public boolean isIncognito() {
        return mIncognito;
    }

    /**
     * Returns the name of a profile. Null or empty is implicitly mapped to incognito.
     *
     * @return The profile name.
     */
    @Nullable
    public String getProfileName() {
        return mProfileName;
    }

    /**
     * Returns the persisted id for the browser.
     *
     * @return The persistence id.
     */
    @Nullable
    public String getPersistenceId() {
        return mPersistenceId;
    }

    /**
     * Whether ViewModel should be used.
     */
    public boolean getUseViewModel() {
        return mUseViewModel;
    }
}
