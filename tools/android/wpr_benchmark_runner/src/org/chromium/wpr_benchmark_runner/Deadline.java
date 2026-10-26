// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.os.SystemClock;

import org.chromium.build.annotations.NullMarked;

import java.net.SocketTimeoutException;

@NullMarked
final class Deadline {
    private final long mEndMillis;

    private Deadline(long endMillis) {
        mEndMillis = endMillis;
    }

    static Deadline after(long timeoutMillis) {
        long endMillis = Math.addExact(SystemClock.elapsedRealtime(), timeoutMillis);
        return new Deadline(endMillis);
    }

    Deadline limit(long timeoutMillis) {
        long requestedEndMillis = Math.addExact(SystemClock.elapsedRealtime(), timeoutMillis);
        return new Deadline(Math.min(mEndMillis, requestedEndMillis));
    }

    long remainingMillis() throws SocketTimeoutException {
        long remaining = mEndMillis - SystemClock.elapsedRealtime();
        if (remaining <= 0) {
            throw new SocketTimeoutException("Operation timed out");
        }
        return remaining;
    }

    void pause(long intervalMillis) throws InterruptedException, SocketTimeoutException {
        long sleepMillis = Math.min(intervalMillis, remainingMillis());
        Thread.sleep(sleepMillis);
    }
}
