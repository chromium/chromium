// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

/**
 * This is XR scene core session initialize interface. It's used by activities to control initial XR
 * space mode at launch. See implementation in {@link
 * org.chromium.chrome.browser.xr.scenecore.XrSceneCoreSessionInitializerImpl}.
 */
@NullMarked
public interface XrSceneCoreSessionInitializer extends Destroyable {
    /**
     * Initialize the activity in specified XR space mode.
     *
     * @param isFullSpaceMode - True, if the activity should launch in Full Space mode by default,
     *     False otherwise.
     */
    void initialize(boolean isFullSpaceMode);
}
