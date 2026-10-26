// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import org.chromium.build.annotations.NullMarked;

@NullMarked
final class DumpToSyslog implements Action {
    private final ActionFactory mFactory;
    private final Deadline mDeadline;
    private final String mInput;

    DumpToSyslog(ActionFactory factory, Deadline deadline, String input) {
        mFactory = factory;
        mDeadline = deadline;
        mInput = input;
    }

    @Override
    public void run() throws Exception {
        Object result = mFactory.requireResult(mInput, Object.class);
        RunArtifacts.logJson(mInput, result, mDeadline);
    }
}
