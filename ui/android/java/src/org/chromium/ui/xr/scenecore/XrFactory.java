// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory interface for creating XR SceneCore objects and entity holders. */
@NullMarked
public interface XrFactory {
    /** Creates a new XrVector3 with the specified components. */
    XrVector3 createVector3(float x, float y, float z);

    /** Creates a new XrQuaternion with the specified components. */
    XrQuaternion createQuaternion(float x, float y, float z, float w);

    /** Creates a new XrQuaternion from a yaw angle (rotation around the Y axis). */
    XrQuaternion createQuaternionFromYaw(float yaw);

    /** Creates a new XrPose with the specified translation and rotation. */
    XrPose createPose(XrVector3 translation, XrQuaternion rotation);

    /** Creates a new XrFloatSize3d with the specified dimensions. */
    XrFloatSize3d createFloatSize3d(float width, float height, float depth);

    /** Creates an XR surface entity holder. */
    XrSurfaceEntityHolder createSurfaceEntity(
            Object runtimeSession, @XrSurfaceEntityShape int shape);

    /** Creates an XR panel entity holder. */
    XrPanelEntityHolder createPanelEntity(Object runtimeSession, View view, String name);

    /** Holder for the static factory instance. */
    class Holder {
        private static @Nullable XrFactory sInstance;

        public static void set(XrFactory factory) {
            sInstance = factory;
        }

        public static XrFactory get() {
            assert sInstance != null;
            return sInstance;
        }
    }
}
