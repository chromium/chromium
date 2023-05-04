// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.IProfileManagerDelegate;

/**
 * Manages Profiles in a WebSandbox.
 */
public class ProfileManager {
    @NonNull
    private IProfileManagerDelegate mDelegate;

    ProfileManager(IProfileManagerDelegate delegate) {
        mDelegate = delegate;
    }

    void invalidate() {
        mDelegate = null;
    }
}
