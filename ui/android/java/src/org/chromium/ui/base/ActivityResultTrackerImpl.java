// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

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
     * An ordered map of generated unique activity keys to the matching registered launchers.
     *
     * <p>As the registration order should be kept upon recreation to ensure in-flight activities
     * results are received correctly, the map is ordered to keep trace of the registration order,
     * so the same order can be saved and restored in the instance state bundle. See the last
     * paragraph in https://developer.android.com/training/basics/intents/result#register
     */
    private final LinkedHashMap<String, ActivityResultLauncher<Intent>> mOrderedLaunchers =
            new LinkedHashMap<>();

    /** A map of listeners to their generated unique keys. */
    private final Map<ResultListener, String> mListenersToKeys = new HashMap<>();

    /** A map of generated unique keys to their listeners. */
    private final Map<String, ResultListener> mKeysToListeners = new HashMap<>();

    /**
     * A map of generated unique keys for which an activity has been started, mapped to the attached
     * restoration key (provided by {@link ResultListener#getRestorationKey}). This is used to keep
     * track of started activities that haven't returned a result yet and will be restored after the
     * base activity's recreation.
     */
    private final LinkedHashMap<String, String> mStartedActivityKeysToRestorationKey =
            new LinkedHashMap<>();

    /**
     * A map of keys to their pending results. A result is pending if it's received while no
     * launcher is registered for its key. This can happen if the base activity is recreated after
     * an activity is started.
     */
    private final Map<String, List<ActivityResult>> mRestorationKeyToPendingResults =
            new HashMap<>();

    public ActivityResultTrackerImpl(ActivityResultTracker.Registry registry) {
        mRegistry = registry;
    }

    /**
     * This method should be called when the activity is saving its instance state. It saves the
     * keys of the activities that have been started and haven't returned a result yet, and the
     * tracker will watch for their result upon base activity recreation.
     */
    public void onSaveInstanceState(Bundle bundle) {
        if (mStartedActivityKeysToRestorationKey.isEmpty()) {
            return;
        }

        LinkedHashMap<String, String> startedActivityKeysInRegistrationOrder =
                new LinkedHashMap<>();
        Set<String> startedActivityKeys = mStartedActivityKeysToRestorationKey.keySet();
        for (String key : mOrderedLaunchers.keySet()) {
            if (startedActivityKeys.contains(key)) {
                startedActivityKeysInRegistrationOrder.put(
                        key, mStartedActivityKeysToRestorationKey.get(key));
            }
        }
        bundle.putSerializable(
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

        LinkedHashMap<String, String> keys =
                (LinkedHashMap<String, String>)
                        bundle.getSerializable(REGISTERED_ACTIVITY_RESULT_KEYS);
        if (keys == null) {
            return;
        }

        mStartedActivityKeysToRestorationKey.putAll(keys);

        for (String key : keys.keySet()) {
            ActivityResultCallback<ActivityResult> callback =
                    new ActivityResultCallback<ActivityResult>() {
                        @Override
                        public void onActivityResult(ActivityResult result) {
                            String restorationKey =
                                    assumeNonNull(mStartedActivityKeysToRestorationKey.remove(key));

                            ResultListener listener = findListenerForRestorationKey(restorationKey);
                            if (listener != null) {
                                listener.onActivityResult(result);
                            } else {
                                mRestorationKeyToPendingResults
                                        .computeIfAbsent(restorationKey, k -> new ArrayList<>())
                                        .add(result);
                            }
                            remove(key);
                        }
                    };
            // This will never be used to start an activity again, but is put to the launchers map
            // to be unregistered after use.
            ActivityResultLauncher<Intent> launcher =
                    mRegistry.register(key, new StartActivityForResult(), callback);
            mOrderedLaunchers.put(key, launcher);
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
    public void register(ResultListener listener) {
        String restorationKey = listener.getRestorationKey();

        List<ActivityResult> results = mRestorationKeyToPendingResults.get(restorationKey);
        if (results != null && !results.isEmpty()) {
            listener.onActivityResult(results.get(0));
            results.remove(0);
            if (results.isEmpty()) {
                mRestorationKeyToPendingResults.remove(restorationKey);
            }
        }

        // The same listener shouldn't be registered twice.
        assert !mListenersToKeys.containsKey(listener) : "Listener is already registered";

        // No launcher has been registered previously (with `#registerNewLauncher`) to catch
        // in-flight results after base activity recreation for this key, a new launcher needs
        // to be registered for the added callback to be called when the new activity returns a
        // result.
        String key = generateUniqueKey();
        mListenersToKeys.put(listener, key);
        mKeysToListeners.put(key, listener);

        // 2 register() methods can be used on the registry. Unfortunately only the one that
        // doesn't require a {@link LifeCyclerOwner} can be used in our case: the other API requires
        // the registration to happen before the `STARTED` state, which the Chrome architecture
        // can't guarantee.
        ActivityResultCallback<ActivityResult> callback =
                new ActivityResultCallback<ActivityResult>() {
                    @Override
                    public void onActivityResult(ActivityResult result) {
                        mStartedActivityKeysToRestorationKey.remove(key);
                        listener.onActivityResult(result);
                    }
                };
        ActivityResultLauncher<Intent> launcher =
                mRegistry.register(key, new StartActivityForResult(), callback);
        mOrderedLaunchers.put(key, launcher);
    }

    @Override
    public void startActivity(ResultListener listener, Intent intent) {
        String key = mListenersToKeys.get(listener);
        ActivityResultLauncher<Intent> launcher = mOrderedLaunchers.get(key);
        if (launcher == null) {
            throw new IllegalStateException(
                    "Callback should be registered before starting activity for result.");
        }
        mStartedActivityKeysToRestorationKey.put(key, listener.getRestorationKey());
        launcher.launch(intent);
    }

    @VisibleForTesting
    public void removeAll() {
        ArrayList<String> keysSnapshot = new ArrayList<>(mOrderedLaunchers.keySet());
        for (String key : keysSnapshot) {
            remove(key);
        }
        mRestorationKeyToPendingResults.clear();
    }

    private void remove(String key) {
        ResultListener listener = mKeysToListeners.remove(key);
        if (listener != null) {
            mListenersToKeys.remove(listener);
        }
        mStartedActivityKeysToRestorationKey.remove(key);
        ActivityResultLauncher<Intent> launcher = mOrderedLaunchers.remove(key);
        if (launcher != null) {
            launcher.unregister();
        }
    }

    private String generateUniqueKey() {
        return UUID.randomUUID().toString();
    }

    private @Nullable ResultListener findListenerForRestorationKey(String restorationKey) {
        for (ResultListener listener : mListenersToKeys.keySet()) {
            if (listener.getRestorationKey().equals(restorationKey)) {
                return listener;
            }
        }
        return null;
    }
}
