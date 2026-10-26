// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

import java.io.IOException;

@NullMarked
final class WaitForCondition implements Action {
    private final CdpSocket mCdpSocket;
    private final Deadline mDeadline;
    private final String mCondition;
    private final String mFailureCondition;
    private final long mTimeoutMillis;
    private final long mPollMillis;
    private final long mCommandMillis;

    WaitForCondition(
            CdpSocket cdp,
            Deadline deadline,
            String condition,
            String failureCondition,
            long timeoutMillis,
            long pollMillis,
            long commandMillis) {
        mCdpSocket = cdp;
        mDeadline = deadline;
        mCondition = condition;
        mFailureCondition = failureCondition;
        mTimeoutMillis = timeoutMillis;
        mPollMillis = pollMillis;
        mCommandMillis = commandMillis;
    }

    @Override
    public void run() throws Exception {
        Deadline deadline = mDeadline.limit(mTimeoutMillis);
        while (true) {
            if (!mFailureCondition.isEmpty() && evaluate(mFailureCondition, deadline)) {
                throw new IOException("Configured failure condition matched");
            }
            if (evaluate(mCondition, deadline)) {
                return;
            }
            deadline.pause(mPollMillis);
        }
    }

    private boolean evaluate(String body, Deadline deadline) throws Exception {
        Deadline commandDeadline = deadline.limit(mCommandMillis);
        Object value = mCdpSocket.evaluate(body, true, true, commandDeadline);
        return Boolean.TRUE.equals(value);
    }
}
