// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.os.Bundle;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ActivityResultTrackerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActivityResultTrackerImplTest {
    private static final String KEY = "test_key";

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private ActivityResultTracker.Registry mRegistry;
    @Mock private ActivityResultLauncher<Intent> mLauncher;
    @Mock private ActivityResultCallback<ActivityResult> mCallback;
    @Captor private ArgumentCaptor<ActivityResultCallback<ActivityResult>> mCallbackCaptor;

    private ActivityResultTrackerImpl mTracker;
    private final Intent mIntent = new Intent();
    private final ActivityResult mResult = new ActivityResult(0, new Intent());

    @Before
    public void setup() {
        mTracker = new ActivityResultTrackerImpl(mRegistry);
        doReturn(mLauncher).when(mRegistry).register(anyString(), notNull(), notNull());
    }

    @Test
    public void testStartActivity_throwIfNoLauncher() {
        Assert.assertThrows(
                IllegalStateException.class, () -> mTracker.startActivity(KEY, mIntent));
    }

    @Test
    public void testStartActivity_throwIfNoRegistrationAfterRecreation() {
        mTracker.register(KEY, mCallback);
        recreateTracker();
        Assert.assertThrows(
                IllegalStateException.class, () -> mTracker.startActivity(KEY, mIntent));
    }

    @Test
    public void testRegisterAndStartActivity() {
        mTracker.register(KEY, mCallback);
        verify(mRegistry).register(anyString(), any(), any());

        mTracker.startActivity(KEY, mIntent);
        verify(mLauncher).launch(mIntent);
    }

    @Test
    public void testRegister_noPendingResult() {
        mTracker.register(KEY, mCallback);
        verify(mCallback, never()).onActivityResult(any());
    }

    @Test
    public void testRegister_withPendingResult() {
        mTracker.register(KEY, mCallback);
        mTracker.startActivity(KEY, mIntent);
        recreateTracker();

        // Verify that the registration happens again after recreation.
        verify(mRegistry, times(2)).register(anyString(), any(), mCallbackCaptor.capture());

        // Simulate activity result's return before new callback is registered.
        mCallbackCaptor.getValue().onActivityResult(mResult);
        mTracker.register(KEY, mCallback);

        // Verify that the new callback is called immediately with the pending result.
        verify(mCallback).onActivityResult(mResult);
    }

    @Test
    public void testRecreation_inflightActivitiesRegisteredInOrder() {
        // Keys are not in natural alphabetical order on purpose to avoid false positives. (e.g.
        // avoid interferences with default ordering rules of collections used in the tracker's
        // implementation)
        String[] keys = new String[] {"key0", "key3", "key1"};
        for (String key : keys) {
            mTracker.register(key, mCallback);
        }

        // Reset the mock to clear the history of the initial register calls.
        reset(mRegistry);

        // Start activities in reversed registration order.
        for (int i = keys.length - 1; i > -1; i--) {
            mTracker.startActivity(keys[i], mIntent);
        }

        recreateTracker();

        InOrder inOrder = inOrder(mRegistry);
        for (String key : keys) {
            inOrder.verify(mRegistry).register(eq(key), any(), any());
        }
    }

    @Test
    public void testOnDestroy() {
        mTracker.register(KEY, mCallback);
        mTracker.onDestroy();
        verify(mLauncher).unregister();
    }

    private void recreateTracker() {
        Bundle bundle = new Bundle();
        mTracker.onSaveInstanceState(bundle);
        mTracker = new ActivityResultTrackerImpl(mRegistry);
        mTracker.onRestoreInstanceState(bundle);
    }
}
