// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

@NullMarked
final class Js implements Action {
    private final CdpSocket mCdpSocket;
    private final ActionFactory mFactory;
    private final Deadline mDeadline;
    private final String mScript;
    private final String mOutput;
    private final long mTimeoutMillis;

    Js(
            CdpSocket cdp,
            ActionFactory factory,
            Deadline deadline,
            String script,
            String output,
            long timeoutMillis) {
        mCdpSocket = cdp;
        mFactory = factory;
        mDeadline = deadline;
        mScript = script;
        mOutput = output;
        mTimeoutMillis = timeoutMillis;
    }

    @Override
    public void run() throws Exception {
        boolean returnsResult = !mOutput.isEmpty();
        Object value =
                mCdpSocket.evaluate(mScript, returnsResult, false, mDeadline.limit(mTimeoutMillis));
        if (returnsResult) {
            mFactory.putResult(mOutput, value);
        }
    }
}
