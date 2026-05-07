// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.Holder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link TimeoutRunnable}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TimeoutRunnableTest {
    @Test
    public void testTimeoutFires() throws Exception {
        CallbackHelper normalCallback = new CallbackHelper();
        CallbackHelper timeoutCallback = new CallbackHelper();
        TimeoutRunnable wrapped =
                new TimeoutRunnable(normalCallback::notifyCalled, timeoutCallback::notifyCalled, 1);
        wrapped.startTimeout();

        Robolectric.flushForegroundThreadScheduler();
        timeoutCallback.waitForOnly();

        wrapped.run();
        Assert.assertEquals(
                "Normal callback should not be called", 0, normalCallback.getCallCount());
        Assert.assertEquals(
                "Timeout callback should be called once", 1, timeoutCallback.getCallCount());
    }

    @Test
    public void testExplicitRunBeforeTimeout() throws Exception {
        AtomicInteger normalRunCount = new AtomicInteger(0);
        AtomicInteger timeoutRunCount = new AtomicInteger(0);
        TimeoutRunnable wrapped =
                new TimeoutRunnable(
                        normalRunCount::incrementAndGet, timeoutRunCount::incrementAndGet, 1000);
        wrapped.startTimeout();

        wrapped.run();
        Assert.assertEquals(1, normalRunCount.get());
        Assert.assertEquals(0, timeoutRunCount.get());

        Robolectric.flushForegroundThreadScheduler();
        Assert.assertEquals("Timeout should not fire", 0, timeoutRunCount.get());
        Assert.assertEquals("Normal callback should not run again", 1, normalRunCount.get());

        wrapped.run();
        Assert.assertEquals(
                "Explicit call after first run should be no-op", 1, normalRunCount.get());
    }

    @Test
    public void testCancel() throws Exception {
        CallbackHelper normalCallback = new CallbackHelper();
        CallbackHelper timeoutCallback = new CallbackHelper();
        TimeoutRunnable wrapped =
                new TimeoutRunnable(
                        normalCallback::notifyCalled, timeoutCallback::notifyCalled, 1000);
        wrapped.startTimeout();
        wrapped.cancel();

        wrapped.run();
        Assert.assertEquals(
                "Normal callback should not run after cancel", 0, normalCallback.getCallCount());

        Robolectric.flushForegroundThreadScheduler();
        Assert.assertEquals(
                "Timeout callback should not run after cancel", 0, timeoutCallback.getCallCount());
    }

    @Test
    public void testRunWithoutStartingTimeout() throws Exception {
        CallbackHelper normalCallback = new CallbackHelper();
        CallbackHelper timeoutCallback = new CallbackHelper();
        TimeoutRunnable wrapped =
                new TimeoutRunnable(
                        normalCallback::notifyCalled, timeoutCallback::notifyCalled, 1000);

        wrapped.run();
        Assert.assertEquals(1, normalCallback.getCallCount());

        wrapped.startTimeout();
        Robolectric.flushForegroundThreadScheduler();
        Assert.assertEquals(0, timeoutCallback.getCallCount());
    }

    @Test
    public void testCancelBeforeStartingTimeout() throws Exception {
        CallbackHelper normalCallback = new CallbackHelper();
        CallbackHelper timeoutCallback = new CallbackHelper();
        TimeoutRunnable wrapped =
                new TimeoutRunnable(
                        normalCallback::notifyCalled, timeoutCallback::notifyCalled, 1000);

        wrapped.cancel();
        wrapped.startTimeout();

        Robolectric.flushForegroundThreadScheduler();
        Assert.assertEquals(0, timeoutCallback.getCallCount());

        wrapped.run();
        Assert.assertEquals(0, normalCallback.getCallCount());
    }

    @Test
    public void testMultipleStartTimeoutCalls() throws Exception {
        CallbackHelper normalCallback = new CallbackHelper();
        CallbackHelper timeoutCallback = new CallbackHelper();
        TimeoutRunnable wrapped =
                new TimeoutRunnable(normalCallback::notifyCalled, timeoutCallback::notifyCalled, 1);

        wrapped.startTimeout();
        wrapped.startTimeout();

        Robolectric.flushForegroundThreadScheduler();
        timeoutCallback.waitForOnly();
        Assert.assertEquals(1, timeoutCallback.getCallCount());
    }

    @Test(expected = AssertionError.class)
    public void testThreadCheckerCrash() throws Exception {
        Holder<TimeoutRunnable> wrappedHolder = new Holder<>(null);
        Thread thread =
                new Thread(
                        () -> {
                            wrappedHolder.onResult(new TimeoutRunnable(() -> {}, () -> {}, 1000));
                        });
        thread.start();
        thread.join();

        wrappedHolder.get().run();
    }
}
