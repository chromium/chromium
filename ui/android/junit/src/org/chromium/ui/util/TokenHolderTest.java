// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class TokenHolderTest {
    private final Runnable mCallback = mock(Runnable.class);
    private final TokenHolder mHolder = new TokenHolder(mCallback);

    @Test
    public void hasTokens_AfterAddingOne() {
        mHolder.acquireToken();
        assertTrue(mHolder.hasTokens());
    }

    @Test
    public void hasNoTokens_AfterRemovingTheToken() {
        int token = mHolder.acquireToken();
        mHolder.releaseToken(token);
        assertFalse(mHolder.hasTokens());
    }

    @Test
    public void hasNoTokens_AfterAddingAndRemovingTwoTokens() {
        int token1 = mHolder.acquireToken();
        int token2 = mHolder.acquireToken();
        mHolder.releaseToken(token1);
        mHolder.releaseToken(token2);
        assertFalse(mHolder.hasTokens());
    }

    @Test
    public void hasTokens_AfterTryingToRemoveInvalidToken() {
        int token1 = mHolder.acquireToken();
        mHolder.releaseToken(token1 + 1);
        assertTrue(mHolder.hasTokens());
    }

    @Test
    public void callbackIsCalled_whenTokensBecomeEmptyOrNotEmpty() {
        int token1 = mHolder.acquireToken();
        verify(mCallback).run();

        clearInvocations(mCallback);
        int token2 = mHolder.acquireToken();
        verify(mCallback, never()).run();

        clearInvocations(mCallback);
        mHolder.releaseToken(token2);
        verify(mCallback, never()).run();

        clearInvocations(mCallback);
        mHolder.releaseToken(token1);
        verify(mCallback).run();
    }
}
