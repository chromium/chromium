// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * A lightweight, immutable representation of a 3D pose, combining translation and rotation.
 *
 * <p>Note: This interface must only define the API surface. Do not add default method
 * implementations or calculations to this interface. All implementations must reside in the
 * concrete subclass (e.g. in the internal package of the xr module).
 */
@NullMarked
public interface XrPose {
    /** Returns the translation vector of the pose. */
    XrVector3 getTranslation();

    /** Returns the rotation quaternion of the pose. */
    XrQuaternion getRotation();

    /** Transforms a 3D point (rotates by the pose's rotation and adds the translation). */
    XrVector3 transformPoint(XrVector3 point);

    /** Creates a new XrPose with the specified translation and identity rotation. */
    static XrPose create(XrVector3 translation) {
        return create(translation, XrQuaternion.getIdentity());
    }

    /** Creates a new XrPose with the specified translation and rotation. */
    static XrPose create(XrVector3 translation, XrQuaternion rotation) {
        return XrFactory.Holder.get().createPose(translation, rotation);
    }

    /** Returns the identity pose representing no translation and no rotation. */
    static XrPose getIdentity() {
        return create(XrVector3.getZero(), XrQuaternion.getIdentity());
    }
}
