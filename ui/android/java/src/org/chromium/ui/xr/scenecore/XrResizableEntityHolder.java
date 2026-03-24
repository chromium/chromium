// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.util.SizeF;

import org.chromium.build.annotations.NullMarked;

/** Interface for an XR entity holder that supports resizing. */
@NullMarked
public interface XrResizableEntityHolder {
    /** Returns the {@link XrResizableComponent} associated with this entity. */
    XrResizableComponent getResizableComponent();

    /** Returns the current size of the entity. */
    SizeF getEntitySize();

    /**
     * Sets the size of the entity.
     *
     * @param width The new width of the entity.
     * @param height The new height of the entity.
     */
    void setEntitySize(float width, float height);
}
