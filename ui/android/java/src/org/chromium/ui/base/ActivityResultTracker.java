// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Intent;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContract;

import org.chromium.build.annotations.NullMarked;

/**
 * Manages activity results, ensuring results are not lost if the calling activity is destroyed and
 * recreated by the system while the started activity is in the foreground.
 *
 * <p><b>Problem:</b> Chrome primarily uses {@code IntentRequestTracker}, which has a limitation: if
 * the base Activity is killed by the system and recreated (e.g., due to memory pressure), any
 * pending activity result would be lost because the callback in the component is not persisted.
 *
 * <p>The standard Android {@link androidx.activity.result.ActivityResultRegistry} can handle
 * Activity recreation, but it requires callbacks to be registered unconditionally during activity
 * initialization or {@code onCreate}. This doesn't suit Chrome's component-based architecture,
 * where components requesting results might be created much later in the Activity's lifecycle, or
 * not at all on some code paths.
 *
 * <p><b>Solution:</b> This tracker uses the underlying {@link
 * androidx.activity.result.ActivityResultRegistry} to cache results, even across Activity
 * recreations. Crucially, if a result arrives after the base Activity is recreated but *before* the
 * specific Chrome component has re-registered its callback, the tracker caches the result. When the
 * component is recreated and registers its callback again with the same key, any cached result is
 * immediately delivered.
 *
 * <p><b>Example of flow:</b>
 *
 * <ol>
 *   <li>When Chrome launches an activity for a result and is then killed in the background, the
 *       base activity's {@link ActivityResultTrackerImpl} saves a record of the launched activity's
 *       key in its {@link #onSaveInstanceState} method to the saved instance state bundle.
 *   <li>Upon the activities recreation, the tracker's {@link #onRestoreInstanceState} method
 *       registers a launcher for these previously launched activities that catches and caches any
 *       pending results that the {@link ActivityResultRegistry} has held onto.
 *   <li>When the original component that initiated the activity is recreated and re-registers its
 *       own callback, the stored pending result is immediately delivered to the newly registered
 *       callback and the launcher registered during {@link #onRestoreInstanceState} will switch to
 *       use the new registered callback instead of caching the returned activity result.
 * </ol>
 *
 * <p>The Activity hosting this tracker is responsible for forwarding lifecycle events like {@code
 * onSaveInstanceState}, {@code onRestoreInstanceState}, and {@code onDestroy} to the tracker's
 * implementation (e.g., {@code ActivityResultTrackerImpl}).
 */
@NullMarked
public interface ActivityResultTracker {

    /**
     * Wrapper of {@link ActivityResultRegistry}, necessary for this Kotlin class to be mockable in
     * Java tests.
     */
    interface Registry {

        /**
         * See (@link ActivityResultRegistry#register(String, ActivityResultContract,
         * ActivityResultCallback)}.)
         */
        ActivityResultLauncher<Intent> register(
                String key,
                ActivityResultContract<Intent, ActivityResult> contract,
                ActivityResultCallback<ActivityResult> callback);
    }

    /**
     * Registers a callback to handle the result of the activity started with the given {@String
     * key}. Must be called before starting the new activity. A same key should not be used twice to
     * register different callbacks.
     *
     * <p>The callback needs to be registered again after the base activity's recreation, to receive
     * any result that may have been returned before the activity was recreated. The callback will
     * be invoked with the given result immediately if such pending result exists.
     *
     * @param key A key to identify the activity to be started.
     * @param callback The callback to be invoked with the result returned by the started activity.
     */
    void register(String key, ActivityResultCallback<ActivityResult> callback);

    /**
     * Starts an activity for result. The result handling callback must be registered before calling
     * this method.
     *
     * @param key The key that was used to register the launcher.
     * @param intent The intent to start the activity.
     */
    void startActivity(String key, Intent intent);
}
