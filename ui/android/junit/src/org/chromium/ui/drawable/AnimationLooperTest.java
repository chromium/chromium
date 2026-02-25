// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.drawable;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.drawable.Animatable2;
import android.graphics.drawable.AnimatedVectorDrawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

/** Test AnimationLooper class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnimationLooperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AnimatedVectorDrawable mAnimatableMock;

    private AnimationLooper mAnimationLooper;

    @Before
    public void setUp() {
        mAnimationLooper = new AnimationLooper(mAnimatableMock);
    }

    @Test
    public void testAnimationNeverInvokedWhenAnimatorsDisabledAtStart() {
        AnimationLooper.setAreAnimatorsEnabledForTests(false);
        mAnimationLooper.start();
        verify(mAnimatableMock, never()).start();

        AnimationLooper.setAreAnimatorsEnabledForTests(true);
        mAnimationLooper.stop();
        verify(mAnimatableMock, never()).stop();
    }

    @Test
    public void testAnimationLoopsWhenAnimatorsEnabled() {
        final ArgumentCaptor<Animatable2.AnimationCallback> captor =
                ArgumentCaptor.forClass(Animatable2.AnimationCallback.class);
        AnimationLooper.setAreAnimatorsEnabledForTests(true);

        mAnimationLooper.start();
        verify(mAnimatableMock).start();
        verify(mAnimatableMock).registerAnimationCallback(captor.capture());
        final Animatable2.AnimationCallback callback = captor.getValue();

        callback.onAnimationEnd(mAnimatableMock);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mAnimatableMock, times(2)).start();

        mAnimationLooper.stop();
        verify(mAnimatableMock).unregisterAnimationCallback(callback);
        verify(mAnimatableMock).stop();
    }

    @Test(expected = AssertionError.class)
    public void testAnimationThrowsErrorWhenStartedTwiceConsecutively() {
        AnimationLooper.setAreAnimatorsEnabledForTests(true);
        mAnimationLooper.start();
        mAnimationLooper.start();
    }
}
