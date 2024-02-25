// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.statics;

import android.content.res.Resources;

import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.async.AsyncPreloadResourceLoader;

/** Handles loading Android resources from disk asynchronously and synchronously. */
public class StaticResourceLoader extends AsyncPreloadResourceLoader {
    /**
     * Creates a {@link StaticResourceLoader}.
     *
     * @param resourceType The resource type this loader is responsible for loading.
     * @param callback The {@link ResourceLoaderCallback} to notify when a {@link Resource} is done
     *     loading.
     * @param resources The {@link Resources} instance to load Android resources from.
     */
    public StaticResourceLoader(
            int resourceType, ResourceLoaderCallback callback, final Resources resources) {
        super(
                resourceType,
                callback,
                new ResourceCreator() {
                    @Override
                    public Resource create(int resId) {
                        return StaticResource.create(resources, resId, 0, 0);
                    }
                });
    }
}
