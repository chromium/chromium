// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import org.chromium.base.Callback;
import org.chromium.ui.resources.Resource;

/** A OnceCallback for observing the next {@link ViewResourceAdapter} produced {@link Resource}. */
public class DynamicResourceReadyOnceCallback implements Callback<Resource> {
    private final DynamicResource mDynamicResource;
    private final Callback<Resource> mCallback;

    /**
     * Add a callback to be invoked once the next time the {@link DynamicResource} is ready.
     * @param dynamicResource the resource to listen for an update to.
     * @param callback the callback to be invoked upon the resource being ready.
     */
    public static void onNext(DynamicResource dynamicResource, Callback<Resource> callback) {
        DynamicResourceReadyOnceCallback onceCallback =
                new DynamicResourceReadyOnceCallback(dynamicResource, callback);
        dynamicResource.addOnResourceReadyCallback(onceCallback);
    }

    private DynamicResourceReadyOnceCallback(
            DynamicResource dynamicResource, Callback<Resource> callback) {
        mDynamicResource = dynamicResource;
        mCallback = callback;
    }

    @Override
    public final void onResult(Resource resource) {
        mCallback.onResult(resource);
        mDynamicResource.removeOnResourceReadyCallback(this);
    }
}
