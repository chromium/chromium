// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.webengine.interfaces.IFragmentParams;

/**
 * Parameters for {@link WebSandbox#createFragment}.
 */
public class FragmentParams {
    @Nullable
    private String mProfileName;

    @Nullable
    private String mPersistenceId;

    private boolean mIsIncognito;

    IFragmentParams getParcelable() {
        IFragmentParams params = new IFragmentParams();
        params.profileName = mProfileName;
        params.persistenceId = mPersistenceId;
        params.isIncognito = mIsIncognito;
        return params;
    }

    /**
     * A Builder class to help create FragmentParams.
     */
    public static final class Builder {
        private FragmentParams mParams = new FragmentParams();

        public FragmentParams build() {
            return mParams;
        }

        /**
         * Sets the name of the profile. Null or empty string implicitly creates an incognito
         * profile. If {@code profile} must only contain alphanumeric and underscore characters
         * since it will be used as a directory name in the file system.
         *
         * @param name The name of the profile.
         */
        @NonNull
        public Builder setProfileName(@Nullable String profileName) {
            mParams.mProfileName = profileName;
            return this;
        }

        /**
         * Sets the persistence id, which uniquely identifies the Fragment for saving the set of
         * tabs and navigations. A value of null does not save/restore any state. A non-null value
         * results in asynchronously restoring the tabs and navigations. Supplying a non-null value
         * means the Fragment initially has no tabs (until restore is complete).
         *
         * @param persistenceId The id for persistence.
         */
        @NonNull
        public Builder setPersistenceId(@Nullable String persistenceId) {
            mParams.mPersistenceId = persistenceId;
            return this;
        }

        /**
         * Sets whether the profile is incognito.
         * @param incognito Whether the profile should be incognito.
         */
        @NonNull
        public Builder setIsIncognito(boolean isIncognito) {
            mParams.mIsIncognito = isIncognito;
            return this;
        }
    }
}
