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
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link ActivityResultTrackerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActivityResultTrackerImplTest {
    private static final String KEY = "test_key";

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private ActivityResultTracker.Registry mRegistry;
    @Mock private ActivityResultLauncher<Intent> mLauncher;
    @Mock private ActivityResultTracker.ResultListener mListener;
    @Captor private ArgumentCaptor<ActivityResultCallback<ActivityResult>> mCallbackCaptor;
    @Captor private ArgumentCaptor<String> mUniqueKeyCaptor;

    private ActivityResultTrackerImpl mTracker;
    private final Intent mIntent = new Intent();
    private final ActivityResult mResult = new ActivityResult(0, new Intent());

    @Before
    public void setup() {
        mTracker = new ActivityResultTrackerImpl(mRegistry);
        doReturn(mLauncher).when(mRegistry).register(anyString(), notNull(), notNull());
        doReturn(KEY).when(mListener).getRestorationKey();
    }

    @Test
    public void testStartActivity_throwIfNoLauncher() {
        Assert.assertThrows(
                IllegalStateException.class,
                () -> mTracker.startActivity(mListener, mIntent, null));
    }

    @Test
    public void testStartActivity_throwIfNoRegistrationAfterRecreation() {
        mTracker.register(mListener);
        recreateTracker();
        Assert.assertThrows(
                IllegalStateException.class,
                () -> mTracker.startActivity(mListener, mIntent, null));
    }

    @Test
    public void testRegisterAndStartActivity() {
        mTracker.register(mListener);
        verify(mRegistry).register(anyString(), any(), any());

        mTracker.startActivity(mListener, mIntent, null);
        verify(mLauncher).launch(mIntent);
    }

    @Test
    public void testRegister_noPendingResult() {
        mTracker.register(mListener);
        verify(mListener, never()).onActivityResult(any(), any());
    }

    @Test
    public void testRegister_withPendingResult() {
        mTracker.register(mListener);
        mTracker.startActivity(mListener, mIntent, null);
        recreateTracker();

        // Verify that the registration happens again after recreation.
        verify(mRegistry, times(2)).register(anyString(), any(), mCallbackCaptor.capture());

        // Simulate activity result's return before new callback is registered.
        mCallbackCaptor.getValue().onActivityResult(mResult);
        mTracker.register(mListener);

        // Verify that the new callback is called immediately with the pending result.
        verify(mListener).onActivityResult(mResult, null);
    }

    @Test
    public void testRegister_withPendingResultAndSavedInstanceData() {
        Bundle savedInstanceData = new Bundle();
        savedInstanceData.putString("test_key", "test_value");

        mTracker.register(mListener);
        mTracker.startActivity(mListener, mIntent, savedInstanceData);
        recreateTracker();

        // Verify that the registration happens again after recreation.
        verify(mRegistry, times(2)).register(anyString(), any(), mCallbackCaptor.capture());

        // Simulate activity result's return before new callback is registered.
        mCallbackCaptor.getValue().onActivityResult(mResult);
        mTracker.register(mListener);

        // Verify that the new callback is called immediately with the pending result and config.
        verify(mListener).onActivityResult(eq(mResult), eq(savedInstanceData));
    }

    @Test
    public void testRegister_keyDuplicated() {
        ActivityResultTracker.ResultListener listenerWithSameKey =
                Mockito.mock(ActivityResultTracker.ResultListener.class);
        doReturn(KEY).when(listenerWithSameKey).getRestorationKey();

        mTracker.register(mListener);
        mTracker.register(listenerWithSameKey);

        // Verify that the registration happens again after recreation.
        verify(mRegistry, times(2)).register(anyString(), any(), mCallbackCaptor.capture());

        mTracker.startActivity(mListener, mIntent, null);

        // Simulate the case where activity result is returned to the second listener.
        mCallbackCaptor.getValue().onActivityResult(mResult);

        // Verify that the new callback is called immediately with the pending result.
        verify(listenerWithSameKey).onActivityResult(mResult, null);
        verify(mListener, never()).onActivityResult(any(), any());
    }

    @Test
    public void testRegisterAfterUnregister() {
        mTracker.register(mListener);
        verify(mRegistry).register(anyString(), any(), any());

        mTracker.unregister(mListener);
        verify(mLauncher).unregister();

        reset(mRegistry);
        mTracker.register(mListener);
        verify(mRegistry).register(any(), any(), any());
    }

    @Test
    public void testRecreation_inflightActivitiesRegisteredInOrder() {
        ActivityResultTracker.ResultListener listener2 =
                Mockito.mock(ActivityResultTracker.ResultListener.class);
        ActivityResultTracker.ResultListener listener3 =
                Mockito.mock(ActivityResultTracker.ResultListener.class);

        mTracker.register(mListener);
        mTracker.register(listener2);
        mTracker.register(listener3);

        verify(mRegistry, times(3)).register(mUniqueKeyCaptor.capture(), any(), any());
        List<String> uniqueKeys = mUniqueKeyCaptor.getAllValues();

        // Reset the mock to clear the history of the initial register calls.
        reset(mRegistry);

        // Start activities in reversed registration order.
        mTracker.startActivity(listener3, mIntent, null);
        mTracker.startActivity(listener2, mIntent, null);
        mTracker.startActivity(mListener, mIntent, null);

        recreateTracker();

        InOrder inOrder = inOrder(mRegistry);
        for (String key : uniqueKeys) {
            inOrder.verify(mRegistry).register(eq(key), any(), any());
        }
    }

    @Test
    public void testOnDestroy() {
        mTracker.register(mListener);
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
