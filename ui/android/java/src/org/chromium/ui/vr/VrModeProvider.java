// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.vr;

/**
 * Provides information about VR mode.
 */
public interface VrModeProvider {
    /**
     * @return Whether VR mode is currently active.
     */
    boolean isInVr();

    /**
     * Registers the given {@link VrModeObserver}.
     *
     * @param observer The VrModeObserver to register.
     */
    void registerVrModeObserver(VrModeObserver observer);

    /**
     * Unregisters the given {@link VrModeObserver}.
     *
     * @param observer The VrModeObserver to remove.
     */
    void unregisterVrModeObserver(VrModeObserver observer);
}
