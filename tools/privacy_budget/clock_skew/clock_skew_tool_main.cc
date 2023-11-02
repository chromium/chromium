// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "mojo/core/embedder/embedder.h"
#include "tools/privacy_budget/clock_skew/clock_skew_tool.h"

namespace {
using clock_skew::ClockSkewTool;
using network_time::NetworkTimeTracker;

base::StringPiece NetworkTimeResultToString(
    NetworkTimeTracker::NetworkTimeResult result) {
  switch (result) {
    case NetworkTimeTracker::NETWORK_TIME_AVAILABLE:
      return "NETWORK_TIME_AVAILABLE";
    case NetworkTimeTracker::NETWORK_TIME_SYNC_LOST:
      return "NETWORK_TIME_SYNC_LOST";
    case NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT:
      return "NETWORK_TIME_NO_SYNC_ATTEMPT";
    case NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC:
      return "NETWORK_TIME_NO_SUCCESSFUL_SYNC";
    case NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING:
      return "NETWORK_TIME_FIRST_SYNC_PENDING";
    case NetworkTimeTracker::NETWORK_TIME_SUBSEQUENT_SYNC_PENDING:
      return "NETWORK_TIME_SUBSEQUENT_SYNC_PENDING";
  }
  NOTREACHED();
}

std::string GetHistogramReport() {
  std::string histogram_plot = "Report:\n";
  base::StatisticsRecorder::WriteGraph("NetworkTimeTracker", &histogram_plot);
  base::StatisticsRecorder::WriteGraph("PrivacyBudget.ClockSkew",
                                       &histogram_plot);
  base::StatisticsRecorder::WriteGraph("PrivacyBudget.ClockDrift",
                                       &histogram_plot);
  return histogram_plot;
}

constexpr int kNumFetches = 1000;
}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line.GetArgs();

  ClockSkewTool tool;

  for (int i = 0; i < kNumFetches; i++) {
    // Technically, this loop could get out of sync with NetworkTimeTracker's
    // background fetches.
    tool.tracker()->WaitForFetch();

    base::Time network_time;
    base::TimeDelta uncertainty;
    NetworkTimeTracker::NetworkTimeResult time_result =
        tool.tracker()->GetNetworkTime(&network_time, &uncertainty);

    if (time_result != NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
      LOG(ERROR) << "Time fetch failed: "
                 << NetworkTimeResultToString(time_result);
      LOG(INFO) << GetHistogramReport();
      continue;
    }

    LOG(INFO) << "Got network_time: " << network_time
              << " with uncertainty: " << uncertainty;
    LOG(INFO) << GetHistogramReport();
  }

  return 0;
}
