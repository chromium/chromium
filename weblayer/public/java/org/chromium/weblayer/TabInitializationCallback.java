// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * An interface for observing events related to tab initialization on startup, either a new tab or
 * restoring previous tabs basedon on a persistence ID.
 */
abstract class TabInitializationCallback {
    /**
     * Called when WebLayer has finished the tab initialization.
     */
    public void onTabInitializationCompleted() {}
}
