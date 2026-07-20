// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * A lightweight representation of a 3D vector.
 *
 * <p>Note: This interface must only define the API surface. Do not add default method
 * implementations or calculations to this interface. All implementations must reside in the
 * concrete subclass (e.g. in the internal package of the xr module).
 */
@NullMarked
public interface XrVector3 {
    /** Returns the X component of the vector. */
    float getX();

    /** Returns the Y component of the vector. */
    float getY();

    /** Returns the Z component of the vector. */
    float getZ();

    /** Adds two vectors and returns the result as a new XrVector3. */
    XrVector3 plus(XrVector3 other);

    /** Multiplies the vector by a scalar and returns the result as a new XrVector3. */
    XrVector3 times(float scale);

    /** Calculates the dot product of this vector and another. */
    float dot(XrVector3 other);

    /** Returns the length of the vector. */
    float getLength();

    /** Returns a normalized unit-length vector pointing in the same direction. */
    XrVector3 toNormalized();

    /** Creates a new XrVector3 with the specified components. */
    static XrVector3 create(float x, float y, float z) {
        return XrFactory.Holder.get().createVector3(x, y, z);
    }

    /** Returns the identity zero vector. */
    static XrVector3 getZero() {
        return create(0f, 0f, 0f);
    }
}
