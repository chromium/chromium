// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.webengine.interfaces.IWebEngineParams;

import java.util.ArrayList;

/**
 * Parameters for {@link WebSandbox#createFragment}.
 */
public class WebEngineParams {
    @Nullable
    private String mProfileName;

    @Nullable
    private String mPersistenceId;

    private boolean mIsIncognito;

    private boolean mIsExternalIntentsEnabled = true;

    @Nullable
    private ArrayList<String> mAllowedOrigins;

    IWebEngineParams getParcelable() {
        IWebEngineParams params = new IWebEngineParams();
        params.profileName = mProfileName;
        params.persistenceId = mPersistenceId;
        params.isIncognito = mIsIncognito;
        params.isExternalIntentsEnabled = mIsExternalIntentsEnabled;
        params.allowedOrigins = mAllowedOrigins;
        return params;
    }

    /**
     * A Builder class to help create WebEngineParams.
     */
    public static final class Builder {
        private WebEngineParams mParams = new WebEngineParams();

        /**
         * Returns the WebEngineParam instance asscoaiated with this Builder.
         */
        @NonNull
        public WebEngineParams build() {
            return mParams;
        }

        /**
         * Sets the name of the profile. Null or empty string implicitly creates an incognito
         * profile. If {@code profile} must only contain alphanumeric and underscore characters
         * since it will be used as a directory name in the file system.
         *
         * @param profileName The name of the profile.
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
         * @param isIncognito Whether the profile should be incognito.
         */
        @NonNull
        public Builder setIsIncognito(boolean isIncognito) {
            mParams.mIsIncognito = isIncognito;
            return this;
        }

        /**
         * Sets whether pages will be able to open native intents.
         * @param isExternalIntentsEnabled Whether all pages will have the ability to open intent
         *         urls.
         */
        @NonNull
        public Builder setIsExternalIntentsEnabled(boolean isExternalIntentsEnabled) {
            mParams.mIsExternalIntentsEnabled = isExternalIntentsEnabled;
            return this;
        }

        /**
         * Sets whether a list of origins are allowed to be navigated to. If this is not set all
         * origins will be allowed.
         * @param allowedOrigins An ArrayList of origins the WebEngine can navigate to.
         *         This does not support wild cards; full host strings must be provided.
         *
         * <pre>
         * {@code
         * ArrayList<String> allowList = new ArrayList<>();
         * allowList.add("https://www.example.com");
         * allowList.add("http://foo.com");
         *
         * new WebEngineParams.Builder()
         * .setAllowedOrigins(allowList)
         * .build();
         * }
         * </pre>
         */
        @NonNull
        public Builder setAllowedOrigins(@Nullable ArrayList<String> allowedOrigins) {
            mParams.mAllowedOrigins = allowedOrigins;
            return this;
        }
    }
}
