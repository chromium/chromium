// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Intent;
import android.os.Bundle;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.ActivityResultRegistry;
import androidx.activity.result.contract.ActivityResultContract;
import androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Set;

@NullMarked
public class ActivityResultTrackerImpl implements ActivityResultTracker {
    public static class RegistryImpl implements Registry {
        private final ActivityResultRegistry mRegistry;

        public RegistryImpl(ActivityResultRegistry registry) {
            mRegistry = registry;
        }

        @Override
        public ActivityResultLauncher<Intent> register(
                String key,
                ActivityResultContract<Intent, ActivityResult> contract,
                ActivityResultCallback<ActivityResult> callback) {
            return mRegistry.register(key, contract, callback);
        }
    }

    private static final String REGISTERED_ACTIVITY_RESULT_KEYS = "REGISTERED_ACTIVITY_RESULT_KEYS";

    private final ActivityResultTracker.Registry mRegistry;

    /**
     * An ordered map of keys to their registered launchers.
     *
     * <p>As the registration order should be kept upon recreation to ensure in-flight activities
     * results are received correctly, the map is ordered to keep trace of the registration order,
     * so the same order can be saved and restored in the instance state bundleSee the last
     * paragraph in https://developer.android.com/training/basics/intents/result#register
     */
    private final LinkedHashMap<String, ActivityResultLauncher<Intent>> mOrderedLaunchers =
            new LinkedHashMap<>();

    /** A map of keys to their registered callbacks. */
    private final HashMap<String, ActivityResultCallback<ActivityResult>> mCallbacks =
            new HashMap<>();

    /**
     * A set of keys for which an activity has been started. This is used to keep track of started
     * activities that haven't returned a result yet and will be restored after the base activity's
     * recreation.
     */
    private final Set<String> mStartedActivityKeys = new HashSet<>();

    /**
     * A map of keys to their pending results. A result is pending if it's received while no
     * launcher is registered for its key. This can happen if the base activity is recreated after
     * an activity is started.
     */
    private final HashMap<String, ActivityResult> mPendingResults = new HashMap<>();

    public ActivityResultTrackerImpl(ActivityResultTracker.Registry registry) {
        mRegistry = registry;
    }

    /**
     * This method should be called when the activity is saving its instance state. It saves the
     * keys of the activities that have been started and haven't returned a result yet, and the
     * tracker will watch for their result upon base activity recreation
     */
    public void onSaveInstanceState(Bundle bundle) {
        if (mStartedActivityKeys.isEmpty()) {
            return;
        }

        ArrayList<String> startedActivityKeysInRegistrationOrder = new ArrayList<>();
        for (String key : mOrderedLaunchers.keySet()) {
            if (mStartedActivityKeys.contains(key)) {
                startedActivityKeysInRegistrationOrder.add(key);
            }
        }
        bundle.putStringArrayList(
                REGISTERED_ACTIVITY_RESULT_KEYS, startedActivityKeysInRegistrationOrder);
    }

    /**
     * This method is called when the activity is restoring its instance state. It restores the keys
     * and re-registers temporary launchers for activities that were started before the base
     * activity was recreated, to receive and cache results for those activities.
     */
    public void onRestoreInstanceState(@Nullable Bundle bundle) {
        if (bundle == null) {
            return;
        }

        ArrayList<String> keys = bundle.getStringArrayList(REGISTERED_ACTIVITY_RESULT_KEYS);
        if (keys == null) {
            return;
        }

        mStartedActivityKeys.addAll(keys);
        for (String key : keys) {
            ActivityResultLauncher<Intent> launcher = mOrderedLaunchers.get(key);
            if (launcher == null) {
                registerNewLauncher(key);
            }
        }
    }

    public void onDestroy() {
        // Registering activity callbacks with {@link ActivityResultRegistry#register(String,
        // ActivityResultContract, ActivityResultCallback)} requires the launchers to be
        // unregistered manually upon onDestroy() when they are not needed anymore. See {@link
        // #registerNewLauncher}.
        removeAll();
    }

    @Override
    public void register(String key, ActivityResultCallback<ActivityResult> callback) {
        if (mCallbacks.containsKey(key)) {
            throw new IllegalStateException(
                    "Callback should be registered before starting activity for result.");
        }
        mCallbacks.put(key, callback);

        ActivityResult result = mPendingResults.remove(key);
        if (result != null) {
            callback.onActivityResult(result);
        }

        ActivityResultLauncher<Intent> oldLauncher = mOrderedLaunchers.get(key);
        if (oldLauncher == null) {
            // No launcher has been registered previously (with `#registerNewLauncher`) to catch
            // in-flight results after base activity recreation for this key, a new launcher needs
            // to be registered for the added callback to be called when the new activity returns a
            // result.
            registerNewLauncher(key);
        }
    }

    @Override
    public void startActivity(String key, Intent intent) {
        ActivityResultLauncher<Intent> launcher = mOrderedLaunchers.get(key);
        if (launcher == null || mCallbacks.get(key) == null) {
            throw new IllegalStateException(
                    "Callback should be registered before starting activity for result.");
        }
        mStartedActivityKeys.add(key);
        launcher.launch(intent);
    }

    @VisibleForTesting
    public void removeAll() {
        ArrayList<String> keysSnapshot = new ArrayList<>(mOrderedLaunchers.keySet());
        for (String key : keysSnapshot) {
            remove(key);
        }
    }

    private void remove(String key) {
        mCallbacks.remove(key);
        mPendingResults.remove(key);
        mStartedActivityKeys.remove(key);
        ActivityResultLauncher<Intent> launcher = mOrderedLaunchers.remove(key);
        if (launcher != null) {
            launcher.unregister();
        }
    }

    /**
     * Registers a launcher that will put any pending result to mPendingResults while the result
     * callback is not available, or call the registered result callback otherwise.
     */
    private void registerNewLauncher(String key) {
        // 2 register() methods can be used on the registry. Unfortunately only the one that doesn't
        // require a {@link LifeCyclerOwner} can be used in our case: the other API requires the
        // registration to happen before the `STARTED` state, which the Chrome architecture can't
        // guarantee.
        ActivityResultCallback<ActivityResult> callback =
                new ActivityResultCallback<ActivityResult>() {
                    @Override
                    public void onActivityResult(ActivityResult result) {
                        mStartedActivityKeys.remove(key);
                        ActivityResultCallback<ActivityResult> callback = mCallbacks.get(key);
                        if (callback != null) {
                            callback.onActivityResult(result);
                        } else {
                            mPendingResults.put(key, result);
                        }
                    }
                };
        ActivityResultLauncher<Intent> launcher =
                mRegistry.register(key, new StartActivityForResult(), callback);
        mOrderedLaunchers.put(key, launcher);
    }
}
