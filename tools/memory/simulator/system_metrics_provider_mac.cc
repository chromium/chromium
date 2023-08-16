// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/system_metrics_provider_mac.h"

#include <mach/mach.h>
#include <mach/mach_time.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/time/time.h"
#include "tools/memory/simulator/utils.h"

namespace memory_simulator {

SystemMetricsProviderMac::SystemMetricsProviderMac() = default;
SystemMetricsProviderMac::~SystemMetricsProviderMac() = default;

std::vector<std::string> SystemMetricsProviderMac::GetMetricNames() {
  return {"total(gb)",
          "free(gb)",
          "active(gb)",
          "inactive(gb)",
          "wired(gb)",
          "purgeable_count(gb)",
          "speculative(gb)",
          "compressor(gb)",
          "throttled(gb)",
          "file_backed(gb)",
          "anonymous(gb)",
          "uncompressed_in_compressor(gb)",
          "reactivations(mb/s)",
          "pageins(mb/s)",
          "pageouts(mb/s)",
          "faults(mb/s)",
          "cow_faults(mb/s)",
          "cache_lookups(mb/s)",
          "cache_hits(mb/s)",
          "purges(mb/s)",
          "decompressions(mb/s)",
          "compressions(mb/s)",
          "swapins(mb/s)",
          "swapouts(mb/s)"};
}

std::map<std::string, double> SystemMetricsProviderMac::GetMetricValues(
    base::TimeTicks now) {
  std::map<std::string, double> metrics;

  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  base::apple::ScopedMachSendRight host(mach_host_self());
  int result = host_info(host.get(), HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo), &count);
  CHECK_EQ(result, KERN_SUCCESS);

  CHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  metrics["total(gb)"] = BytesToGB(hostinfo.max_mem);

  vm_statistics64_data_t vm_info;
  count = HOST_VM_INFO64_COUNT;
  CHECK_EQ(host_statistics64(host.get(), HOST_VM_INFO64,
                             reinterpret_cast<host_info64_t>(&vm_info), &count),
           KERN_SUCCESS);
  CHECK_EQ(HOST_VM_INFO64_COUNT, count);

  // Free memory includes speculative memory.
  metrics["free(gb)"] = PagesToGB(vm_info.free_count);
  metrics["active(gb)"] = PagesToGB(vm_info.active_count);
  metrics["inactive(gb)"] = PagesToGB(vm_info.inactive_count);
  metrics["wired(gb)"] = PagesToGB(vm_info.wire_count);
  metrics["purgeable_count(gb)"] = PagesToGB(vm_info.purgeable_count);
  metrics["speculative(gb)"] = PagesToGB(vm_info.speculative_count);
  metrics["compressor(gb)"] = PagesToGB(vm_info.compressor_page_count);
  metrics["throttled(gb)"] = PagesToGB(vm_info.throttled_count);
  metrics["file_backed(gb)"] = PagesToGB(vm_info.external_page_count);
  metrics["anonymous(gb)"] = PagesToGB(vm_info.internal_page_count);
  metrics["uncompressed_in_compressor(gb)"] =
      PagesToGB(vm_info.total_uncompressed_pages_in_compressor);

  if (!prev_time_.is_null()) {
    const base::TimeDelta elapsed = now - prev_time_;

    metrics["reactivations(mb/s)"] = PagesToMBPerSec(
        prev_vm_info_.reactivations, vm_info.reactivations, elapsed);
    metrics["pageins(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.pageins, vm_info.pageins, elapsed);
    metrics["pageouts(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.pageouts, vm_info.pageouts, elapsed);
    metrics["faults(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.faults, vm_info.faults, elapsed);
    metrics["cow_faults(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.cow_faults, vm_info.cow_faults, elapsed);
    metrics["cache_lookups(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.lookups, vm_info.lookups, elapsed);
    metrics["cache_hits(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.hits, vm_info.hits, elapsed);
    metrics["purges(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.purges, vm_info.purges, elapsed);
    metrics["decompressions(mb/s)"] = PagesToMBPerSec(
        prev_vm_info_.decompressions, vm_info.decompressions, elapsed);
    metrics["compressions(mb/s)"] = PagesToMBPerSec(
        prev_vm_info_.compressions, vm_info.compressions, elapsed);
    metrics["swapins(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.swapins, vm_info.swapins, elapsed);
    metrics["swapouts(mb/s)"] =
        PagesToMBPerSec(prev_vm_info_.swapouts, vm_info.swapouts, elapsed);
  }

  prev_time_ = now;
  prev_vm_info_ = vm_info;

  return metrics;
}

}  // namespace memory_simulator
