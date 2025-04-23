// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package tools.metrics.histograms.test_data;

private class AllowlistExample {

    private String ignoredMethod() {
        return "histograms_allowlist_check should ignore this code";
    }

    private String[] getAllowlist() {
        String[] histogramsAllowlist =
                new String[] {
                    // histograms_allowlist_check START_PARSING
                    "UMA.FileMetricsProvider.InitialAccessResult",
                    "UMA.FileMetricsProvider.AccessResult",
                    // histograms_allowlist_check END_PARSING
                };
        return histogramsAllowlist;
    }

    private String anotherIgnoredMethod() {
        String ignored = "This is all ignored.";
        return ignored;
    }
}
