// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;

import java.lang.ref.WeakReference;

/**
 * The interface for a helper class that keeps track of the intent requests for an Activity. Its
 * implementation should be hidden in ui/base. No implementation should be made outside of ui/base.
 */
public interface IntentRequestTracker {
    /** A delegate of this class's intent sending. */
    interface Delegate {
        /**
         * Starts an activity for the provided intent.
         * @see Activity#startActivityForResult
         */
        boolean startActivityForResult(Intent intent, int requestCode);

        /**
         * Uses the provided intent sender to start the intent.
         * @see Activity#startIntentSenderForResult
         */
        boolean startIntentSenderForResult(IntentSender intentSender, int requestCode);

        /**
         * Force finish an activity that you had previously started with startActivityForResult or
         * startIntentSenderForResult.
         * @param requestCode The request code returned from showCancelableIntent.
         */
        void finishActivity(int requestCode);

        /**
         * @return A reference to owning Activity.  The returned WeakReference will never be null,
         *         but the contained Activity can be null (if it has been garbage collected). The
         *         returned WeakReference is immutable and calling clear will throw an exception.
         */
        WeakReference<Activity> getActivity();

        /**
         * Invoked from {@link #onActivityResult} when an intent's callback is not found.
         * @param error The error message that was set when "showing an intent" was requested.
         * @return True if the error has been handled. Otherwise, the caller needs to handle the
         *         unhandled error.
         */
        default boolean onCallbackNotFoundError(String error) {
            return false;
        }
    }

    /**
     * Creates an instance of the class from a given delegate.
     * @param delegate A delegate that will be responsible for intent sending.
     * @return an instance of the class.
     */
    static IntentRequestTracker createFromDelegate(Delegate delegate) {
        return new IntentRequestTrackerImpl(delegate);
    }

    /**
     * Creates an instance of the class from a given activity. This is a pure convenience method to
     * create with an ActivityIntentRequestTrackerDelegate. Clients that needs to customize the
     * Delegate should use {@link #createFromDelegate} instead.
     * @param activity The activity who will be used as the intent sender and whose intent requests
     *        to be tracked.
     * @return an instance of the class.
     */
    static IntentRequestTracker createFromActivity(Activity activity) {
        return new IntentRequestTrackerImpl(new ActivityIntentRequestTrackerDelegate(activity));
    }

    /**
     * Responds to the intent result.
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     * @return Boolean value of whether the intent.
     */
    boolean onActivityResult(int requestCode, int resultCode, Intent data);

    WeakReference<Activity> getActivity();

    /**
     * Saves the error messages that should be shown if any pending intents would return
     * after the application has been put onPause.
     * @param bundle The bundle to save the information in onPause
     */
    void saveInstanceState(Bundle bundle);

    /**
     * Restores the error messages that should be shown if any pending intents would return
     * after the application has been put onPause.
     * @param bundle The bundle to restore the information from onResume
     */
    void restoreInstanceState(Bundle bundle);
}
