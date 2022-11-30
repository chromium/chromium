// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.util.SparseArray;

import org.chromium.ui.resources.ResourceLoader;

/**
 * Handles managing dynamic resources. Because {@link DynamicResource} decide when they are dirty
 * and should return a loaded resource, this class mostly just passes through notifications when
 * render frames are happening, and hands the captured {@link org.chromium.ui.resources.Resource}
 * back in our {@link ResourceLoaderCallback}.
 */
public class DynamicResourceLoader extends ResourceLoader {
    private final SparseArray<DynamicResource> mDynamicResources =
            new SparseArray<DynamicResource>();

    /**
     * Builds a {@link DynamicResourceLoader} instance.
     * @param resourceType The resource type this loader is responsible for loading.
     * @param callback     A {@link ResourceLoaderCallback} to be notified when the dynamic resource
     *                     has changed.  The callback will only be notified if
     *                     {@link #loadResource(int)} is called.
     */
    public DynamicResourceLoader(int resourceType, ResourceLoaderCallback callback) {
        super(resourceType, callback);
    }

    /**
     * Registers a {@link DynamicResource} to be tracked and exposed by this class.
     * @param resId The Android id to use.  This should be an actual Android id (R.id.some_id).
     * @param asyncDynamicResource The {@link DynamicResource} to track and expose.
     */
    public void registerResource(int resId, DynamicResource asyncDynamicResource) {
        assert mDynamicResources.get(resId) == null;
        mDynamicResources.put(resId, asyncDynamicResource);
        asyncDynamicResource.setOnResourceReadyCallback(
                (resource) -> notifyLoadFinished(resId, resource));
    }

    /**
     * Unregisters a {@link DynamicResource} specified by {@code resId}.
     * @param resId The Android id representing the {@link DynamicResource}.
     */
    public void unregisterResource(int resId) {
        DynamicResource dynamicResource = mDynamicResources.get(resId);
        if (dynamicResource == null) return;
        mDynamicResources.remove(resId);
        dynamicResource.setOnResourceReadyCallback(null);
        notifyResourceUnregistered(resId);
    }

    /**
     * Called when a {@link DynamicResource} was requested.  This will notify the
     * {@link ResourceLoaderCallback} if the resource has new contents.
     * @param resId The Android id representing the {@link DynamicResource}.
     */
    @Override
    public void loadResource(int resId) {
        DynamicResource dynamicResource = mDynamicResources.get(resId);
        if (dynamicResource == null) return;
        dynamicResource.onResourceRequested();
    }

    /**
     * Since this class relies solely on registration it does not support preloading resources.
     */
    @Override
    public void preloadResource(int resId) {
        // Not implemented.
        assert false : "Preloading dynamic resources isn't supported.";
    }
}
