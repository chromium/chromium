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

    private static class PendingResult {
        public final ActivityResult result;
        public final @Nullable Bundle savedInstanceData;

        public PendingResult(ActivityResult result, @Nullable Bundle savedInstanceData) {
            this.result = result;
            this.savedInstanceData = savedInstanceData;
        }
    }

    private static final String REGISTERED_ACTIVITY_RESULT_KEYS_KEYS =
            "REGISTERED_ACTIVITY_RESULT_KEYS_KEYS";
    private static final String REGISTERED_ACTIVITY_RESULT_KEYS_VALUES =
            "REGISTERED_ACTIVITY_RESULT_KEYS_VALUES";
    private static final String REGISTERED_ACTIVITY_RESULT_CONFIGS =
            "REGISTERED_ACTIVITY_RESULT_CONFIGS";

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
     * A map of generated unique keys for which an activity has been started, mapped to the optional
     * bundle containing data saved before starting the activity.
     */
    private final Map<String, Bundle> mKeysToSavedInstanceData = new HashMap<>();

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
    private final Map<String, List<PendingResult>> mRestorationKeyToPendingResults =
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
        Bundle configs = new Bundle();
        Set<String> startedActivityKeys = mStartedActivityKeysToRestorationKey.keySet();
        for (String key : mOrderedLaunchers.keySet()) {
            if (startedActivityKeys.contains(key)) {
                startedActivityKeysInRegistrationOrder.put(
                        key, mStartedActivityKeysToRestorationKey.get(key));
                Bundle savedConfig = mKeysToSavedInstanceData.get(key);
                if (savedConfig != null) {
                    configs.putBundle(key, savedConfig);
                }
            }
        }

        // The ordered property of a LinkedHasMap can be lost on some Android version when using
        // Bundle#getSerializable which deserializes the map to a HashMap, reason why keys and
        // values are serialized as ArrayLists here.
        // See http://b/31607484.
        ArrayList<String> keysList =
                new ArrayList<>(startedActivityKeysInRegistrationOrder.keySet());
        ArrayList<String> valuesList =
                new ArrayList<>(startedActivityKeysInRegistrationOrder.values());
        bundle.putStringArrayList(REGISTERED_ACTIVITY_RESULT_KEYS_KEYS, keysList);
        bundle.putStringArrayList(REGISTERED_ACTIVITY_RESULT_KEYS_VALUES, valuesList);
        bundle.putBundle(REGISTERED_ACTIVITY_RESULT_CONFIGS, configs);
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

        ArrayList<String> keysList =
                bundle.getStringArrayList(REGISTERED_ACTIVITY_RESULT_KEYS_KEYS);
        ArrayList<String> valuesList =
                bundle.getStringArrayList(REGISTERED_ACTIVITY_RESULT_KEYS_VALUES);
        Bundle configs = bundle.getBundle(REGISTERED_ACTIVITY_RESULT_CONFIGS);

        if (keysList == null
                || valuesList == null
                || keysList.size() != valuesList.size()
                || configs == null) {
            return;
        }

        for (int i = 0; i < keysList.size(); i++) {
            mStartedActivityKeysToRestorationKey.put(keysList.get(i), valuesList.get(i));
        }

        for (String key : mStartedActivityKeysToRestorationKey.keySet()) {
            if (configs.containsKey(key)) {
                mKeysToSavedInstanceData.put(key, assumeNonNull(configs.getBundle(key)));
            }

            ActivityResultCallback<ActivityResult> callback =
                    new ActivityResultCallback<ActivityResult>() {
                        @Override
                        public void onActivityResult(ActivityResult result) {
                            String restorationKey =
                                    assumeNonNull(mStartedActivityKeysToRestorationKey.remove(key));
                            @Nullable Bundle savedConfig = mKeysToSavedInstanceData.remove(key);

                            ResultListener listener = findListenerForRestorationKey(restorationKey);
                            if (listener != null) {
                                listener.onActivityResult(result, savedConfig);
                            } else {
                                mRestorationKeyToPendingResults
                                        .computeIfAbsent(restorationKey, k -> new ArrayList<>())
                                        .add(new PendingResult(result, savedConfig));
                            }
                            unregister(key);
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

        List<PendingResult> pendingResults = mRestorationKeyToPendingResults.get(restorationKey);
        if (pendingResults != null && !pendingResults.isEmpty()) {
            PendingResult pendingResult = pendingResults.remove(0);
            listener.onActivityResult(pendingResult.result, pendingResult.savedInstanceData);
            if (pendingResults.isEmpty()) {
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
                        @Nullable Bundle savedConfig = mKeysToSavedInstanceData.remove(key);
                        listener.onActivityResult(result, savedConfig);
                    }
                };
        ActivityResultLauncher<Intent> launcher =
                mRegistry.register(key, new StartActivityForResult(), callback);
        mOrderedLaunchers.put(key, launcher);
    }

    @Override
    public void startActivity(
            ResultListener listener, Intent intent, @Nullable Bundle savedInstanceData) {
        String key = mListenersToKeys.get(listener);
        ActivityResultLauncher<Intent> launcher = mOrderedLaunchers.get(key);
        if (launcher == null) {
            throw new IllegalStateException(
                    "Callback should be registered before starting activity for result.");
        }
        mStartedActivityKeysToRestorationKey.put(key, listener.getRestorationKey());
        if (savedInstanceData != null) {
            mKeysToSavedInstanceData.put(key, savedInstanceData);
        }
        launcher.launch(intent);
    }

    @Override
    public void unregister(ResultListener listener) {
        String key = mListenersToKeys.get(listener);
        if (key != null) {
            unregister(key);
        }
    }

    private void removeAll() {
        ArrayList<String> keysSnapshot = new ArrayList<>(mOrderedLaunchers.keySet());
        for (String key : keysSnapshot) {
            unregister(key);
        }
        mRestorationKeyToPendingResults.clear();
    }

    private void unregister(String key) {
        ResultListener listener = mKeysToListeners.remove(key);
        if (listener != null) {
            mListenersToKeys.remove(listener);
        }
        mKeysToSavedInstanceData.remove(key);
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
