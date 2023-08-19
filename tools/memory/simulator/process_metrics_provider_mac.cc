// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/process_metrics_provider_mac.h"

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/time/time.h"
#include "tools/memory/simulator/utils.h"

namespace memory_simulator {

ProcessMetricsProviderMac::ProcessMetricsProviderMac() = default;
ProcessMetricsProviderMac::~ProcessMetricsProviderMac() = default;

std::vector<std::string> ProcessMetricsProviderMac::GetMetricNames() {
  return {"process_resident(gb)", "process_internal(gb)",
          "process_external(gb)", "process_compressed(gb)",
          "process_physical(gb)"};
}

std::map<std::string, double> ProcessMetricsProviderMac::GetMetricValues(
    base::TimeTicks now) {
  std::map<std::string, double> metrics;

  task_vm_info process_info;
  mach_msg_type_number_t count = TASK_VM_INFO_REV2_COUNT;
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&process_info), &count);
  CHECK_EQ(result, KERN_SUCCESS);

  metrics["process_resident(gb)"] = BytesToGB(process_info.resident_size);
  metrics["process_internal(gb)"] = BytesToGB(process_info.internal);
  metrics["process_external(gb)"] = BytesToGB(process_info.external);
  metrics["process_compressed(gb)"] = BytesToGB(process_info.compressed);
  metrics["process_physical(gb)"] = BytesToGB(process_info.phys_footprint);

  return metrics;
}

}  // namespace memory_simulator
