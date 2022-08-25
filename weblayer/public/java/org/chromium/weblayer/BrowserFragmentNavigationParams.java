// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.browserfragment.interfaces.INavigationParams;

/**
 * This class is a helper class to create {@link INavigationParams}-parcelable.
 */
class BrowserFragmentNavigationParams {
    private BrowserFragmentNavigationParams() {}

    public static INavigationParams create(Navigation navigation) {
        INavigationParams params = new INavigationParams();
        params.uri = navigation.getUri();
        params.statusCode = navigation.getHttpStatusCode();
        params.isSameDocument = navigation.isSameDocument();
        return params;
    }
}