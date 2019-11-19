// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;

import androidx.annotation.CallSuper;

import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceLoader.ResourceLoaderCallback;

/**
 * A representation of a dynamic resource.  The contents of the resource might change from frame to
 * frame.
 */
public abstract class DynamicResource implements Resource {
    /**
     * {@link DynamicResourceLoader#loadResource(int)} only notifies {@link ResourceLoaderCallback}
     * if the resource is dirty. Therefore, if the resource is not dirty, this should not be called.
     * @return null. This method should be overridden, and ignore the return value here.
     */
    @Override
    @CallSuper
    public Bitmap getBitmap() {
        assert isDirty() : "getBitmap() should not be called when not dirty";
        return null;
    }

    /**
     * Note that this is called for every access to the resource during a frame.  If a resource is
     * dirty, it should not be dirty again during the same looper call.
     * {@link DynamicResourceLoader#loadResource(int)} only notifies
     * {@link ResourceLoaderCallback#onResourceLoaded} if the resource is dirty.
     * Therefore, if the resource is not dirty, {@link #getBitmap()} doesn't get called.
     *
     * TODO(dtrainor): Add checks so that a dynamic resource **can't** be built more than once each
     * frame.
     *
     * @return Whether or not this resource is dirty and the CC component should be rebuilt.
     */
    abstract boolean isDirty();
}
