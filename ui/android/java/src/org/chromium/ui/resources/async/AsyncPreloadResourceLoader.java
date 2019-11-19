// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.async;

import android.util.SparseArray;

import org.chromium.base.TraceEvent;
import org.chromium.base.task.AsyncTask;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceLoader;

import java.util.concurrent.ExecutionException;

/**
 * Handles loading Android resources from disk asynchronously and synchronously.
 */
public class AsyncPreloadResourceLoader extends ResourceLoader {
    /**
     * Responsible for actually creating a {@link Resource} from a specific resource id.
     */
    public interface ResourceCreator {
        /**
         * Creates a {@link Resource} from {@code resId}.  Note that this method may be called from
         * a background thread.  No assumptions can be made on which thread this will be called
         * from!
         * @param resId The id of the resource to load.
         * @return      A {@link Resource} instance that represents {@code resId} or {@code null} if
         *              none can be loaded.
         */
        Resource create(int resId);
    }

    private final SparseArray<AsyncLoadTask> mOutstandingLoads = new SparseArray<AsyncLoadTask>();
    private final ResourceCreator mCreator;

    /**
     * Creates a {@link AsyncPreloadResourceLoader}.
     * @param resourceType The resource type this loader is responsible for loading.
     * @param callback     The {@link ResourceLoaderCallback} to notify when a {@link Resource} is
     *                     done loading.
     * @param creator      A {@link ResourceCreator} instance that will be used to create the
     *                     {@link Resource}s.
     */
    public AsyncPreloadResourceLoader(int resourceType, ResourceLoaderCallback callback,
            ResourceCreator creator) {
        super(resourceType, callback);
        mCreator = creator;
    }

    /**
     * Loads a resource synchronously.  This will still call the {@link ResourceLoaderCallback} on
     * completion.  If the resource is currently being loaded asynchronously this will wait for that
     * task to complete before returning.  If the resource is queued to be read asynchronously later
     * this will cancel that request.
     * @param resId The Android resource id to load.
     */
    @Override
    public void loadResource(int resId) {
        AsyncLoadTask task = mOutstandingLoads.get(resId);

        if (task != null) {
            if (!task.cancel(false)) {
                try {
                    registerResource(task.get(), resId);
                } catch (InterruptedException e) {
                    notifyLoadFinished(resId, null);
                } catch (ExecutionException e) {
                    notifyLoadFinished(resId, null);
                }
                return;
            }
        }
        registerResource(createResource(resId), resId);
    }

    /**
     * Loads a resource asynchronously.  The load will be queued if other resources are currently
     * being loaded.  The {@link ResourceLoaderCallback} will be notified on completion.
     * @param resId The Android resource id to load.
     */
    @Override
    public void preloadResource(int resId) {
        if (mOutstandingLoads.get(resId) != null) return;
        AsyncLoadTask task = new AsyncLoadTask(resId);
        task.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        mOutstandingLoads.put(resId, task);
    }

    private Resource createResource(int resId) {
        try {
            TraceEvent.begin("AsyncPreloadResourceLoader.createResource");
            return mCreator.create(resId);
        } finally {
            TraceEvent.end("AsyncPreloadResourceLoader.createResource");
        }
    }

    private void registerResource(Resource resource, int resourceId) {
        notifyLoadFinished(resourceId, resource);
        mOutstandingLoads.remove(resourceId);
    }

    private class AsyncLoadTask extends AsyncTask<Resource> {
        private final int mResourceId;

        public AsyncLoadTask(int resourceId) {
            mResourceId = resourceId;
        }

        @Override
        protected Resource doInBackground() {
            return createResource(mResourceId);
        }

        @Override
        protected void onPostExecute(Resource resource) {
            // If we've been removed from the list of outstanding load tasks, don't broadcast the
            // callback.
            if (mOutstandingLoads.get(mResourceId) == null) return;
            registerResource(resource, mResourceId);
        }
    }
}
