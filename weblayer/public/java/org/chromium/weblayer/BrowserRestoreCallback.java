// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * An interface for observing events related to restoring the previous state of a Browser.
 *
 * @since 88
 */
abstract class BrowserRestoreCallback {
    /**
     * Called when WebLayer has finished restoring the previous state.
     */
    public void onRestoreCompleted() {}
}
