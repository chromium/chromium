// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.INavigationParams;

/**
 * {@link Navigation} contains information about the current Tab Navigation.
 */
public class Navigation {
    private INavigationParams mNavigationParams;

    Navigation(@NonNull INavigationParams navigationParams) {
        mNavigationParams = navigationParams;
    }

    /**
     * The uri the main frame is navigating to. This may change during the navigation when
     * encountering a server redirect.
     */
    @NonNull
    public Uri getUri() {
        return mNavigationParams.uri;
    }

    /**
     * Returns the status code of the navigation. Returns 0 if the navigation hasn't completed yet
     * or if a response wasn't received.
     */
    public int getStatusCode() {
        return mNavigationParams.statusCode;
    }

    /**
     * Whether the navigation happened without changing document. Examples of same document
     * navigations are:
     *  - reference fragment navigations
     *  - pushState/replaceState
     *  - same page history navigation
     */
    public boolean isSameDocument() {
        return mNavigationParams.isSameDocument;
    }
}