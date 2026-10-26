// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.UiAutomation;
import android.content.Context;

import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
final class WprBenchmarkRunner {
    private static final String TAG = "WprBenchmark";

    private final Context mContext;
    private final UiAutomation mAutomation;
    private String mLocation = "prepare_files";

    WprBenchmarkRunner(Context context, UiAutomation automation) {
        mContext = context;
        mAutomation = automation;
    }

    void run(@Nullable String resultsDirectory, long timeoutMillis) throws Exception {
        Deadline deadline = Deadline.after(timeoutMillis);
        try (RunArtifacts artifacts = new RunArtifacts(mContext, resultsDirectory)) {
            JSONObject definition = artifacts.prepareDependencies(deadline);
            JSONObject browser = definition.getJSONObject("browser");
            mLocation = "prepare_actions";
            ActionFactory factory = new ActionFactory(mAutomation, artifacts, browser, deadline);
            ActionFactory.Plan plan = factory.parse(definition);
            mLocation = "start_wpr";
            try (WprReplayController replay = new WprReplayController(plan.mReplay, artifacts);
                    BrowserController browserController =
                            new BrowserController(mContext, mAutomation, browser);
                    ActionFactory actions = factory) {
                replay.awaitReady(deadline);
                mLocation = "launch_browser";
                browserController.start(deadline);
                for (ActionFactory.Step step : plan.mActions) {
                    mLocation = step.mLocation;
                    deadline.remainingMillis();
                    Log.i(TAG, "Running " + mLocation);
                    step.mAction.run();
                }
                mLocation = "cleanup";
            }
        }
    }

    String location() {
        return mLocation;
    }
}
