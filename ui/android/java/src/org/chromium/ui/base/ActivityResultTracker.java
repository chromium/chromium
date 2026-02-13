// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Intent;
import android.os.Bundle;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContract;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Manages activity results, ensuring results are not lost if the calling activity is destroyed and
 * recreated by the system while the started activity is in the foreground.
 *
 * <p>See {@link
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/activity_result_tracker.md}
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
     * Listener for activity results.
     *
     * <p>The listener is identified by a restoration key, which is used to restore the listener
     * after the base activity's recreation.
     */
    interface ResultListener {

        /**
         * Called when an activity returns a result.
         *
         * @param result The result returned by the activity.
         * @param savedInstanceData The optional bundle containing data saved before starting the
         *     activity.
         */
        void onActivityResult(ActivityResult result, @Nullable Bundle savedInstanceData);

        /**
         * Returns a key that identifies this listener across activity recreation. It's preferable
         * to not reuse the same key for different instances that co-exist, otherwise, if in-flight
         * activity returns after the base activity is recreated, an arbitrary listener using the
         * given restoration key will be chosen to handle the returned result.
         *
         * <p>This key is used to capture and cache started activity's result after the base
         * activity's recreation.
         */
        String getRestorationKey();
    }

    /**
     * Registers a listener to handle the result of the activity started with the given listener.
     * Must be called before using starting the new activity.
     *
     * <p>The listener needs to be registered again after the base activity's recreation, to receive
     * any result that may have been returned before the activity was recreated. The callback will
     * be invoked with the given result immediately if such pending result is cached.
     *
     * @param listener The {@link ResultListener} to be registered.
     */
    void register(ResultListener listener);

    /**
     * Unregisters the listener if it has been registered previously. This should be called when
     * leaving the UI registering the listener initially (e.g. recent tabs page). This way, when the
     * UI is opened again in the same activity, a new ip-to-date listener can be registered with the
     * same key.
     *
     * <p>Note that if the UI is closed due to activity recreation, the key should have been saved
     * in the instance state earlier during onSaveInstanceState(), as MVC destruction happens during
     * onDestroy() usually, so the pending result should be caught after the base activity's
     * recreation.
     *
     * @param listener the ResultListener to be unregistered.
     */
    void unregister(ResultListener listener);

    /**
     * Starts an activity for result. The result handling callback must be registered before calling
     * this method.
     *
     * @param listener The listener to be notified when the result is returned.
     * @param intent The intent to start the activity.
     * @param savedInstanceData The optional bundle containing data to be saved and restored across
     *     activity recreation.
     */
    void startActivity(ResultListener listener, Intent intent, @Nullable Bundle savedInstanceData);
}
