// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation.transition;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link IntegerValueTransition}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IntegerValueTransitionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mView;
    @Mock private Callback<Integer> mOnUpdate;

    @Test
    public void testCreateAnimator() {
        int startValue = 10;
        int targetValue = 20;
        IntegerValueTransition transition =
                new IntegerValueTransition(mView, startValue, targetValue, mOnUpdate);
        Animator animator = transition.createAnimator(null, null, null);
        assertNotNull(animator);

        animator.start();
        ShadowLooper.runUiThreadTasks();

        ArgumentCaptor<Integer> captor = ArgumentCaptor.forClass(Integer.class);
        verify(mOnUpdate, atLeastOnce()).onResult(captor.capture());

        List<Integer> values = captor.getAllValues();
        assertEquals(startValue, (int) values.get(0));
        assertEquals(targetValue, (int) values.get(values.size() - 1));

        // Ensure values are increasing and filtered correctly.
        for (int i = 1; i < values.size(); i++) {
            // Some values may be duplicates, but the values should be monotonic.
            assert (((int) values.get(i)) >= ((int) values.get(i - 1)));
        }
    }
}
