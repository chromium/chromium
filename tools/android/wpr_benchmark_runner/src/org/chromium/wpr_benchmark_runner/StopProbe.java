// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

import java.io.File;

@NullMarked
final class StopProbe implements Action {
    private final CdpSocket mCdpSocket;
    private final ActionFactory mFactory;
    private final RunArtifacts mArtifacts;
    private final Deadline mDeadline;
    private final String mOutput;
    private final long mTimeoutMillis;

    StopProbe(
            CdpSocket cdp,
            ActionFactory factory,
            RunArtifacts artifacts,
            Deadline deadline,
            String output,
            long timeoutMillis) {
        mCdpSocket = cdp;
        mFactory = factory;
        mArtifacts = artifacts;
        mDeadline = deadline;
        mOutput = output;
        mTimeoutMillis = timeoutMillis;
    }

    @Override
    public void run() throws Exception {
        File traceFile = mArtifacts.temporaryFile("trace-", ".pb");
        RunArtifacts.TraceArtifact trace =
                mCdpSocket.stopTrace(traceFile, mDeadline.limit(mTimeoutMillis));
        mFactory.putResult(mOutput, trace);
    }
}
