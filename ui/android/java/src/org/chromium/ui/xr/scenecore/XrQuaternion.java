// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * A lightweight representation of a Quaternion for 3D rotation.
 *
 * <p>Note: This interface must only define the API surface. Do not add default method
 * implementations or calculations to this interface. All implementations must reside in the
 * concrete subclass (e.g. in the internal package of the xr module).
 */
@NullMarked
public interface XrQuaternion {
    /** Returns the X component of the quaternion. */
    float getX();

    /** Returns the Y component of the quaternion. */
    float getY();

    /** Returns the Z component of the quaternion. */
    float getZ();

    /** Returns the W component of the quaternion. */
    float getW();

    /** Rotates a 3D vector by this quaternion and returns the result as a new XrVector3. */
    XrVector3 rotate(XrVector3 v);

    /** Returns the yaw angle (rotation around the Y axis) of this quaternion in radians. */
    float getYaw();

    /** Creates a new XrQuaternion with the specified components. */
    static XrQuaternion create(float x, float y, float z, float w) {
        return XrFactory.Holder.get().createQuaternion(x, y, z, w);
    }

    /** Creates a quaternion from a yaw angle (rotation around the Y axis). */
    static XrQuaternion fromYaw(float yaw) {
        return XrFactory.Holder.get().createQuaternionFromYaw(yaw);
    }

    /** Returns the identity quaternion representing no rotation. */
    static XrQuaternion getIdentity() {
        return create(0f, 0f, 0f, 1f);
    }
}
