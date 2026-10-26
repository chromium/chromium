// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.Activity;
import android.app.Instrumentation;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
public final class WprBenchmarkInstrumentation extends Instrumentation {
    private static final String TAG = "WprBenchmark";

    private Bundle mArguments = new Bundle();

    @Override
    public void onCreate(@Nullable Bundle arguments) {
        super.onCreate(arguments);
        if (arguments != null) {
            mArguments = new Bundle(arguments);
        }
        start();
    }

    @Override
    public void onStart() {
        super.onStart();
        @Nullable WprBenchmarkRunner runner = null;
        try {
            long timeoutMillis = Long.parseLong(mArguments.getString("timeout-ms", "1800000"));
            if (timeoutMillis <= 0) {
                throw new IllegalArgumentException("timeout-ms must be positive");
            }
            String directory = mArguments.getString("results-dir", "").trim();
            @Nullable String resultsDirectory = null;
            if (!directory.isEmpty()) {
                resultsDirectory = directory;
            }
            runner = new WprBenchmarkRunner(getTargetContext(), getUiAutomation());
            runner.run(resultsDirectory, timeoutMillis);
            report(true, "Benchmark completed");
        } catch (Exception error) {
            if (error instanceof InterruptedException) {
                Thread.currentThread().interrupt();
            }
            String location = "startup";
            if (runner != null) {
                location = runner.location();
            }
            Log.e(TAG, "Benchmark failed at " + location, error);
            report(false, location + ": " + error);
        }
    }

    private void report(boolean success, String message) {
        Bundle result = new Bundle();
        result.putBoolean("success", success);
        String summary = message.substring(0, Math.min(2048, message.length()));
        result.putString(REPORT_KEY_STREAMRESULT, summary + "\n");
        int resultCode = Activity.RESULT_CANCELED;
        if (success) {
            resultCode = Activity.RESULT_OK;
        }
        finish(resultCode, result);
    }
}
