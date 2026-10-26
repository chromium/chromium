// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

@NullMarked
final class StartProbe implements Action {
    private final CdpSocket mCdpSocket;
    private final Deadline mDeadline;
    private final JSONObject mTraceConfig;
    private final long mTimeoutMillis;

    StartProbe(CdpSocket cdp, Deadline deadline, JSONObject traceConfig, long timeoutMillis) {
        mCdpSocket = cdp;
        mDeadline = deadline;
        mTraceConfig = traceConfig;
        mTimeoutMillis = timeoutMillis;
    }

    @Override
    public void run() throws Exception {
        mCdpSocket.startTrace(mTraceConfig, mDeadline.limit(mTimeoutMillis));
    }
}
