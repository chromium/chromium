// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Defines the spaces where an entity can be posed or queried. */
@IntDef({XrSpace.ACTIVITY, XrSpace.PARENT})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface XrSpace {
    /** The space of the activity. */
    int ACTIVITY = 0;

    /** The space of the parent entity. */
    int PARENT = 1;
}
