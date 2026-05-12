// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_WIN_CHROMEEXTS_COMMANDS_TASK_TRACE_UTILS_H_
#define TOOLS_WIN_CHROMEEXTS_COMMANDS_TASK_TRACE_UTILS_H_

#include <windows.h>

#include <dbgeng.h>

#include <cstdint>
#include <vector>

namespace tools::win::chromeexts {

// Represents a single task trace entry found on a thread's stack.
struct TaskTraceEntry {
  TaskTraceEntry();
  TaskTraceEntry(const TaskTraceEntry&);
  TaskTraceEntry& operator=(const TaskTraceEntry&);
  TaskTraceEntry(TaskTraceEntry&&);
  TaskTraceEntry& operator=(TaskTraceEntry&&);
  ~TaskTraceEntry();

  uint64_t posted_from_pc = 0;
  std::vector<uint64_t> parent_pcs;
  uint64_t ipc_hash = 0;
  uint64_t stack_location = 0;
  ULONG thread_id = 0;
  ULONG thread_sys_id = 0;
};

// Scans a contiguous memory region for task backtrace marker pairs and
// populates `traces` with any entries found. `start` is the virtual
// address corresponding to the start of the region.
HRESULT ScanMemoryRegionForTaskTraces(IDebugDataSpaces4* debug_data,
                                      uint64_t start,
                                      uint64_t size,
                                      std::vector<TaskTraceEntry>* traces);

// Scans thread stacks for task backtrace markers.
// If `scan_all` is false, only the current thread is scanned.
// Optional output parameters report scanning statistics.
HRESULT ScanForTaskTraces(IDebugDataSpaces4* debug_data,
                          IDebugSystemObjects4* debug_system,
                          IDebugRegisters* debug_registers,
                          bool scan_all,
                          std::vector<TaskTraceEntry>* traces,
                          ULONG* out_threads_scanned = nullptr,
                          ULONG* out_threads_skipped = nullptr,
                          uint64_t* out_total_bytes_scanned = nullptr);

}  // namespace tools::win::chromeexts

#endif  // TOOLS_WIN_CHROMEEXTS_COMMANDS_TASK_TRACE_UTILS_H_
