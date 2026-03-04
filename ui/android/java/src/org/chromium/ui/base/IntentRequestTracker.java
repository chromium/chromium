// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

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
     * Responds to the intent result.
     *
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     * @return Boolean value of whether the intent.
     */
    boolean onActivityResult(int requestCode, int resultCode, @Nullable Intent data);

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

    /**
     * Show a cancelable intent.
     *
     * @param intent The intent to be shown.
     * @param callback The callback to be called after the intent is completed.
     * @param errorId The error ID used if the intent encounters an error.
     * @return int The request code for the intent.
     */
    int showCancelableIntent(Intent intent, IntentCallback callback, @Nullable Integer errorId);
}
