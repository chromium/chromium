// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;

/**
 * This interface must be implemented by Activity. See {@link XrSceneCoreSessionManager} for
 * details.
 */
@NullMarked
public interface XrSceneCoreSessionManagerProvider {
    /** Return instance of XrSceneCoreSessionManager on Android XR platform or null otherwise. */
    @Nullable
    XrSceneCoreSessionManager getXrSceneCoreSessionManager();
}
