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

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link ShrinkTransition}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ShrinkTransitionUnitTest {
    private static final float DELTA = 0.0001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mView;

    @Test
    public void testOnAppear() {
        float goneScale = 0.6f;
        float visibleScale = 1.0f;
        ShrinkTransition transition = new ShrinkTransition(goneScale, visibleScale);
        Animator animator = transition.onAppear(null, mView, null, null);
        assertNotNull(animator);

        animator.start();
        ShadowLooper.runUiThreadTasks();

        ArgumentCaptor<Float> captorX = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> captorY = ArgumentCaptor.forClass(Float.class);

        verify(mView, atLeastOnce()).setScaleX(captorX.capture());
        verify(mView, atLeastOnce()).setScaleY(captorY.capture());

        List<Float> valuesX = captorX.getAllValues();
        List<Float> valuesY = captorY.getAllValues();

        assertEquals(goneScale, valuesX.get(0), DELTA);
        assertEquals(goneScale, valuesY.get(0), DELTA);
        assertEquals(visibleScale, valuesX.get(valuesX.size() - 1), DELTA);
        assertEquals(visibleScale, valuesY.get(valuesY.size() - 1), DELTA);
    }

    @Test
    public void testOnDisappear() {
        float goneScale = 0.6f;
        float visibleScale = 1.0f;
        ShrinkTransition transition = new ShrinkTransition(goneScale, visibleScale);
        Animator animator = transition.onDisappear(null, mView, null, null);
        assertNotNull(animator);

        animator.start();
        ShadowLooper.runUiThreadTasks();

        ArgumentCaptor<Float> captorX = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> captorY = ArgumentCaptor.forClass(Float.class);

        verify(mView, atLeastOnce()).setScaleX(captorX.capture());
        verify(mView, atLeastOnce()).setScaleY(captorY.capture());

        List<Float> valuesX = captorX.getAllValues();
        List<Float> valuesY = captorY.getAllValues();

        assertEquals(visibleScale, valuesX.get(0), DELTA);
        assertEquals(visibleScale, valuesY.get(0), DELTA);

        // The penultimate value should be the goneScale (end of animation).
        assertEquals(goneScale, valuesX.get(valuesX.size() - 2), DELTA);
        assertEquals(goneScale, valuesY.get(valuesY.size() - 2), DELTA);

        // The last value should be the visibleScale because of the reset in onAnimationEnd.
        assertEquals(visibleScale, valuesX.get(valuesX.size() - 1), DELTA);
        assertEquals(visibleScale, valuesY.get(valuesY.size() - 1), DELTA);
    }
}
