// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/simulator_metrics_provider.h"

#include "base/time/time.h"
#include "tools/memory/simulator/memory_simulator.h"
#include "tools/memory/simulator/utils.h"

namespace memory_simulator {

SimulatorMetricsProvider::SimulatorMetricsProvider(MemorySimulator* simulator)
    : simulator_(simulator) {}

SimulatorMetricsProvider::~SimulatorMetricsProvider() = default;

std::vector<std::string> SimulatorMetricsProvider::GetMetricNames() {
  return {"simulator_allocated(gb)", "simulator_allocation_rate(mb/s)",
          "simulator_read_rate(mb/s)", "simulator_write_rate(mb/s)"};
}

std::map<std::string, double> SimulatorMetricsProvider::GetMetricValues(
    base::TimeTicks now) {
  int64_t pages_allocated = simulator_->GetPagesAllocated();
  int64_t pages_read = simulator_->GetPagesRead();
  int64_t pages_written = simulator_->GetPagesWritten();

  std::map<std::string, double> metrics;
  metrics["simulator_allocated(gb)"] = PagesToGB(pages_allocated);

  if (!prev_time_.is_null()) {
    base::TimeDelta elapsed = now - prev_time_;
    metrics["simulator_allocation_rate(mb/s)"] =
        PagesToMBPerSec(prev_pages_allocated_, pages_allocated, elapsed);
    metrics["simulator_read_rate(mb/s)"] =
        PagesToMBPerSec(prev_pages_read_, pages_read, elapsed);
    metrics["simulator_write_rate(mb/s)"] =
        PagesToMBPerSec(prev_pages_written_, pages_written, elapsed);
  }

  prev_time_ = now;
  prev_pages_allocated_ = pages_allocated;
  prev_pages_read_ = pages_read;
  prev_pages_written_ = pages_written;

  return metrics;
}

}  // namespace memory_simulator
