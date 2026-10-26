// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

import java.net.SocketTimeoutException;

@NullMarked
final class Wait implements Action {
    private final Deadline mDeadline;
    private final long mDurationMillis;

    Wait(Deadline deadline, long durationMillis) {
        mDeadline = deadline;
        mDurationMillis = durationMillis;
    }

    @Override
    public void run() throws Exception {
        if (mDurationMillis > mDeadline.remainingMillis()) {
            throw new SocketTimeoutException("Wait exceeds the remaining run budget");
        }
        Thread.sleep(mDurationMillis);
    }
}
