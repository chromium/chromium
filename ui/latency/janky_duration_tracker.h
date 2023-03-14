// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_JANKY_DURATION_TRACKER_H_
#define UI_LATENCY_JANKY_DURATION_TRACKER_H_

namespace switches {

// Enables tracking scroll jank. Watches the given directory. On events like
// moving a file into the directory a log message is printed with the current
// jank counts.
const char kWatchDirForScrollJankReport[] = "watch-dir-for-scroll-jank-report";

}  // namespace switches

namespace ui {

// Advances one of the two counters by |count|. The
// switches::kWatchDirForScrollJankReport enables external programs to dump both
// sums to the log. Each process that called this method at least once would
// dump a log line upon such request.
void AdvanceJankyDurationForBenchmarking(bool janky, int count);

}  // namespace ui

#endif  // UI_LATENCY_JANKY_DURATION_TRACKER_H_
