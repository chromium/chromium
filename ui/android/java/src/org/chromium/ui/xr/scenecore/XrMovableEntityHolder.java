// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/** Interface for an XR entity holder that supports movement. */
@NullMarked
public interface XrMovableEntityHolder {
    /** Returns the {@link XrMovableComponent} associated with this entity. */
    XrMovableComponent getMovableComponent();
}
