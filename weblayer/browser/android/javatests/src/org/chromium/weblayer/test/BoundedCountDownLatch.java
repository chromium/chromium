// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A CountDownLatch with a default timeout.
 */
public class BoundedCountDownLatch extends CountDownLatch {
    BoundedCountDownLatch(int count) {
        super(count);
    }

    /**
     * This fails more quickly and gracefully than {@link CountDownLatch#await()}, which has no
     * timeout. It gives useful error output, whereas a test that times out in {@link await()} may
     * leave no stack.
     */
    public void timedAwait() {
        try {
            Assert.assertTrue(super.await(CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Assert.fail(e.toString());
        }
    }
}
