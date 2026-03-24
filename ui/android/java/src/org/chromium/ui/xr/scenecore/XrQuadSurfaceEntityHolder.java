// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for a class that holds a quad-shaped XR surface entity. It supports movement and
 * resizing.
 *
 * @param <EntityType> The type of the underlying XR entity.
 */
@NullMarked
public interface XrQuadSurfaceEntityHolder<EntityType>
        extends XrSurfaceEntityHolder<EntityType>, XrMovableEntityHolder, XrResizableEntityHolder {}
