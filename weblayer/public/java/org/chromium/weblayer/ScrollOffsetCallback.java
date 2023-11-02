// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Callback notified when the vertical location of the content of a Tab changes. The value reported
 * by the callback corresponds to the 'scrollTop' html property.
 *
 * WARNING: use of this API necessitates additional cross process ipc that impacts overall
 * performance. Only use when absolutely necessary.
 *
 * Because of WebLayer's multi-process architecture, this function can not be used to reliably
 * synchronize the painting of other Views with WebLayer's Views. It's entirely possible one will
 * render before or after the other.
 */
abstract class ScrollOffsetCallback {
    /**
     * Called when the vertical scroll location of the content of a Tab changes.
     *
     * @param scrollLocation The new vertical location. More specifically, the 'scrollTop' html
     * property.
     */
    public abstract void onVerticalScrollOffsetChanged(int scrollLocation);
}
