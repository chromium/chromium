// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.graphics.drawable.Drawable;
import android.os.Looper;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TranslationAnimatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class DrawableFadeInAnimatorFactoryUnitTest {
    private static final long DURATION_MS = 10L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Drawable mDrawable;

    @Test
    public void testFadeIn() {
        ArgumentCaptor<Integer> captor = ArgumentCaptor.forClass(Integer.class);

        Animator animator = DrawableFadeInAnimatorFactory.build(mDrawable);
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        // Alpha is set to zero twice.
        verify(mDrawable, atMost(2)).setAlpha(0);

        verify(mDrawable, atLeastOnce()).setAlpha(captor.capture());
        assertEquals(
                "Drawable is not fully visible! Actual Alpha: " + captor.getValue(),
                Integer.valueOf(255),
                captor.getValue());
    }
}
