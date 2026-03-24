// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/**
 * This is the view that provides access to the underlying XR surface entity. It only exists to
 * satisfy the requirement of the CompositorView as it needs to be a View to be added to the view
 * hierarchy.
 */
@NullMarked
public abstract class XrSurfaceEntityView extends View {
    public XrSurfaceEntityView(Context context) {
        super(context);
    }

    /** Returns the holder that provides access to the underlying XR surface. */
    public abstract XrSurfaceEntityHolder getHolder();
}
