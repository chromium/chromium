// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

import java.io.File;

@NullMarked
final class RunTraceProcessor implements Action {
    private final ActionFactory mFactory;
    private final TraceProcessor mProcessor;
    private final Deadline mDeadline;
    private final String mInput;
    private final String mOutput;
    private final File mQuery;
    private final long mTimeoutMillis;

    RunTraceProcessor(
            ActionFactory factory,
            TraceProcessor processor,
            Deadline deadline,
            String input,
            String output,
            File query,
            long timeoutMillis) {
        mFactory = factory;
        mProcessor = processor;
        mDeadline = deadline;
        mInput = input;
        mOutput = output;
        mQuery = query;
        mTimeoutMillis = timeoutMillis;
    }

    @Override
    public void run() throws Exception {
        RunArtifacts.TraceArtifact trace =
                mFactory.requireResult(mInput, RunArtifacts.TraceArtifact.class);
        TraceProcessor.QueryResult result =
                mProcessor.query(trace, mQuery, mDeadline.limit(mTimeoutMillis));
        mFactory.putResult(mOutput, result);
    }
}
