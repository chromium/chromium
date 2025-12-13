// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link TranslationAnimatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class CommonAnimationsFactoryUnitTest {
    private static final long DURATION_MS = 10L;
    private static final float DELTA = 0.0001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View mView;

    @Test
    public void testFadeIn() {
        AtomicReference<Float> alpha = new AtomicReference<>(-1f);
        doAnswer(
                        invocation -> {
                            alpha.set(invocation.getArgument(0));
                            return null;
                        })
                .when(mView)
                .setAlpha(anyFloat());

        Animator animator = CommonAnimationsFactory.createFadeInAnimation(mView);
        animator.setDuration(DURATION_MS);
        animator.start();

        assertEquals(0f, alpha.get(), DELTA);
        verify(mView).setVisibility(View.VISIBLE);

        ShadowLooper.runUiThreadTasks();
        assertEquals(1f, alpha.get(), DELTA);
    }

    @Test
    public void testFadeOut() {
        List<Float> alphas = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            alphas.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mView)
                .setAlpha(anyFloat());

        Animator animator = CommonAnimationsFactory.createFadeOutAnimation(mView);
        animator.setDuration(DURATION_MS);
        animator.start();

        assertEquals(1f, alphas.get(0), DELTA);

        ShadowLooper.runUiThreadTasks();
        for (int i = 1; i < alphas.size() - 2; i++) {
            assertTrue(alphas.get(i) <= alphas.get(i - 1));
        }
        assertEquals(1f, alphas.get(alphas.size() - 1), DELTA);
        verify(mView).setVisibility(View.GONE);
    }
}
