// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

@NullMarked
final class DumpToFile implements Action {
    private final ActionFactory mFactory;
    private final RunArtifacts mArtifacts;
    private final Deadline mDeadline;
    private final String mInput;
    private final String mFilename;

    DumpToFile(
            ActionFactory factory,
            RunArtifacts artifacts,
            Deadline deadline,
            String input,
            String filename) {
        mFactory = factory;
        mArtifacts = artifacts;
        mDeadline = deadline;
        mInput = input;
        mFilename = filename;
    }

    @Override
    public void run() throws Exception {
        if (!mArtifacts.hasResultsDirectory()) {
            return;
        }
        Object result = mFactory.requireResult(mInput, Object.class);
        mArtifacts.writeOutput(mFilename, result, mDeadline);
    }
}
