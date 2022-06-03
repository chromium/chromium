// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.vr;

/**
 * Used to observe changes to whether Chrome is currently being viewed in VR.
 */
public interface VrModeObserver {
    /**
     * Called when Chrome enters VR rendering mode.
     */
    void onEnterVr();

    /**
     * Called when Chrome exits VR rendering mode.
     */
    void onExitVr();
}