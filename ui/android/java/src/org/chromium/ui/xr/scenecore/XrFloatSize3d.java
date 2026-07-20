// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/** A lightweight representation of a 3D size or extent (width, height, depth). */
@NullMarked
public interface XrFloatSize3d {
    /** Returns the width. */
    float getWidth();

    /** Returns the height. */
    float getHeight();

    /** Returns the depth. */
    float getDepth();

    /** Creates a new XrFloatSize3d with the specified dimensions. */
    static XrFloatSize3d create(float width, float height, float depth) {
        return XrFactory.Holder.get().createFloatSize3d(width, height, depth);
    }
}
