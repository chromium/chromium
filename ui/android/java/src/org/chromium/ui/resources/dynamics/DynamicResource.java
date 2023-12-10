// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import org.chromium.base.Callback;
import org.chromium.ui.resources.Resource;

/**
 * A representation of a dynamic resource. The contents may change from frame to frame. It should be
 * be able to return a {@link Resource} version of itself asynchronously. The
 * {@link DynamicResource} is in charge of tracking when it has changed and should actually be
 * returning a copy of itself.
 */
public interface DynamicResource {
    /**
     * Will be called every render frame to notify the resource. The expectation is that this call
     * will happen a lot, but only needs to be responded to when the dynamic resource has had a
     * change that would cause the resulting resource to be different in some way, typically a
     * change in the return of {@link Resource#getBitmap()}.
     */
    void onResourceRequested();

    /**
     * Adds an callback that will be invoked when the resource is ready to be used and drawn or is
     * updated.
     */
    void addOnResourceReadyCallback(Callback<Resource> onResourceReady);

    /** Removes a resource ready callback to stop listening for updates to the resource. */
    void removeOnResourceReadyCallback(Callback<Resource> onResourceReady);
}
