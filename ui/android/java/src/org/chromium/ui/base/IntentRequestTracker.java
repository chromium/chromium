// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.lang.ref.WeakReference;

/**
 * The interface for a helper class that keeps track of the intent requests for an Activity. Its
 * implementation should be hidden in ui/base. No implementation should be made outside of ui/base.
 */
@NullMarked
public interface IntentRequestTracker {
    /**
     * Show a cancelable intent.
     *
     * @param intent The PendingIntent to be shown.
     * @param onIntentCompletion The onIntentCompletion to be called after the intent is completed.
     * @param errorId The ID of the error string shown when the intent result can't be handled (e.g.
     *     result returned after the activity starting the intent was recreated, erasing the initial
     *     callback.
     * @return int The request code for the intent.
     */
    int showCancelableIntent(
            PendingIntent intent,
            @Nullable IntentCallback onIntentCompletion,
            @Nullable @StringRes Integer errorId);

    /**
     * Show a cancelable intent.
     *
     * @param intent The intent to be shown.
     * @param callback The callback to be called after the intent is completed.
     * @param errorId The ID of the error string shown when the intent result can't be handled (e.g.
     *     result returned after the activity starting the intent was recreated, erasing the initial
     *     callback.
     * @return int The request code for the intent.
     */
    int showCancelableIntent(
            @Nullable Intent intent, @Nullable IntentCallback callback, @Nullable Integer errorId);

    /**
     * Force finish an activity that you had previously started with showCancelableIntent.
     *
     * @param requestCode The request code returned from showCancelableIntent.
     */
    void cancelIntent(int requestCode);

    /**
     * Removes a callback from the list of pending intents, so that nothing happens if/when the
     * result for that intent is received.
     *
     * @param callback The object that should have received the results
     * @return True if the callback was removed, false if it was not found.
     */
    boolean removeIntentCallback(IntentCallback callback);

    /**
     * Responds to the intent result.
     *
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     * @return Boolean value of whether the intent.
     */
    boolean onActivityResult(int requestCode, int resultCode, @Nullable Intent data);

    /** Returns the {@link Activity} associated with this tracker. */
    WeakReference<Activity> getActivity();

    /**
     * Saves the error messages that should be shown if any pending intents would return
     * after the application has been put onPause.
     * @param bundle The bundle to save the information in onPause
     */
    void saveInstanceState(Bundle bundle);

    /**
     * Restores the error messages that should be shown if any pending intents would return after
     * the application has been put onPause.
     *
     * @param bundle The bundle to restore the information from onResume
     */
    void restoreInstanceState(@Nullable Bundle bundle);
}
