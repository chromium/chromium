// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/win/chromeexts/commands/task_trace_utils.h"

#include <algorithm>

namespace tools::win::chromeexts {

namespace {

// Magic markers placed by base::TaskAnnotator::RunTaskImpl() on the stack
// to delimit the task backtrace snapshot. See
// base/task/common/task_annotator.cc for the layout.
constexpr uint64_t kTaskTraceBeginMarker = 0xc001c0ded017d00d;
constexpr uint64_t kTaskTraceEndMarker = 0x0d00d1d1d178119;

// From base/pending_task.h: PendingTask::kTaskBacktraceLength.
constexpr int kTaskBacktraceLength = 4;

// Total snapshot size: begin_marker + posted_from + backtrace[4] + ipc_hash +
// end_marker.
constexpr int kStackSnapshotSize = kTaskBacktraceLength + 4;

// Maximum stack memory to scan per thread (2 MB).
constexpr uint64_t kMaxStackScanSize = 2 * 1024 * 1024;

}  // namespace

TaskTraceEntry::TaskTraceEntry() = default;
TaskTraceEntry::TaskTraceEntry(const TaskTraceEntry&) = default;
TaskTraceEntry& TaskTraceEntry::operator=(const TaskTraceEntry&) = default;
TaskTraceEntry::TaskTraceEntry(TaskTraceEntry&&) = default;
TaskTraceEntry& TaskTraceEntry::operator=(TaskTraceEntry&&) = default;
TaskTraceEntry::~TaskTraceEntry() = default;

HRESULT ScanMemoryRegionForTaskTraces(IDebugDataSpaces4* debug_data,
                                      uint64_t start,
                                      uint64_t size,
                                      std::vector<TaskTraceEntry>* traces) {
  // Read the stack memory in chunks with overlap to avoid missing markers
  // that straddle a chunk boundary.
  constexpr size_t kChunkSize = 64 * 1024;  // 64KB chunks.
  constexpr size_t kOverlapBytes = sizeof(uint64_t) * (kStackSnapshotSize - 1);
  std::vector<uint8_t> buffer(kChunkSize + kOverlapBytes);

  for (uint64_t offset = 0; offset < size;) {
    uint64_t read_addr = start + offset;
    ULONG to_read = static_cast<ULONG>(
        std::min(static_cast<uint64_t>(kChunkSize), size - offset));
    ULONG bytes_read = 0;

    HRESULT hr =
        debug_data->ReadVirtual(read_addr, buffer.data(), to_read, &bytes_read);
    if (FAILED(hr) || bytes_read < sizeof(uint64_t) * kStackSnapshotSize) {
      offset += kChunkSize;
      continue;
    }

    // Scan for the begin marker, pointer-aligned.
    const uint64_t* qwords = reinterpret_cast<const uint64_t*>(buffer.data());
    size_t num_qwords = bytes_read / sizeof(uint64_t);

    for (size_t i = 0; i + kStackSnapshotSize <= num_qwords; i++) {
      if (qwords[i] != kTaskTraceBeginMarker) {
        continue;
      }
      if (qwords[i + kStackSnapshotSize - 1] != kTaskTraceEndMarker) {
        continue;
      }

      // Found a valid marker pair. Extract the trace.
      TaskTraceEntry entry;
      entry.stack_location = read_addr + i * sizeof(uint64_t);
      entry.posted_from_pc = qwords[i + 1];

      for (int j = 0; j < kTaskBacktraceLength; j++) {
        uint64_t pc = qwords[i + 2 + j];
        if (pc == 0) {
          break;
        }
        entry.parent_pcs.push_back(pc);
      }

      entry.ipc_hash = qwords[i + kStackSnapshotSize - 2];
      traces->push_back(entry);

      // Skip past this snapshot to avoid double-counting.
      i += kStackSnapshotSize - 1;
    }

    // Advance by chunk size minus overlap so the next read covers the
    // boundary region.
    offset += (bytes_read > kOverlapBytes) ? (bytes_read - kOverlapBytes)
                                           : bytes_read;
  }

  return S_OK;
}

HRESULT ScanForTaskTraces(IDebugDataSpaces4* debug_data,
                          IDebugSystemObjects4* debug_system,
                          IDebugRegisters* debug_registers,
                          bool scan_all,
                          std::vector<TaskTraceEntry>* traces,
                          ULONG* out_threads_scanned,
                          ULONG* out_threads_skipped,
                          uint64_t* out_total_bytes_scanned) {
  HRESULT hr;

  // Save current thread to restore later.
  ULONG original_thread_id;
  debug_system->GetCurrentThreadId(&original_thread_id);

  ULONG threads_scanned = 0;
  ULONG threads_skipped = 0;
  uint64_t total_bytes_scanned = 0;

  ULONG num_threads = 1;

  if (scan_all) {
    hr = debug_system->GetNumberThreads(&num_threads);
    if (FAILED(hr)) {
      return hr;
    }
  }

  for (ULONG i = 0; i < num_threads; i++) {
    ULONG thread_id;
    ULONG thread_sys_id;

    if (scan_all) {
      hr = debug_system->GetThreadIdsByIndex(i, 1, &thread_id, &thread_sys_id);
      if (FAILED(hr)) {
        threads_skipped++;
        continue;
      }

      hr = debug_system->SetCurrentThreadId(thread_id);
      if (FAILED(hr)) {
        threads_skipped++;
        continue;
      }
    } else {
      thread_id = original_thread_id;
      debug_system->GetCurrentThreadSystemId(&thread_sys_id);
    }

    // Get the TEB (Thread Environment Block) for stack limits.
    ULONG64 teb_addr = 0;
    hr = debug_system->GetCurrentThreadTeb(&teb_addr);
    if (FAILED(hr) || teb_addr == 0) {
      threads_skipped++;
      continue;
    }

    // Read StackBase and StackLimit from the TEB.
    // TEB layout (x64): StackBase at offset 0x08, StackLimit at offset 0x10.
    ULONG64 stack_base = 0;
    ULONG64 stack_limit = 0;
    ULONG bytes_read = 0;
    hr = debug_data->ReadVirtual(teb_addr + 0x08, &stack_base,
                                 sizeof(stack_base), &bytes_read);
    if (FAILED(hr)) {
      threads_skipped++;
      continue;
    }
    hr = debug_data->ReadVirtual(teb_addr + 0x10, &stack_limit,
                                 sizeof(stack_limit), &bytes_read);
    if (FAILED(hr)) {
      threads_skipped++;
      continue;
    }

    if (stack_base <= stack_limit || stack_base == 0) {
      threads_skipped++;
      continue;
    }

    // Use the current RSP as scan start (more accurate than StackLimit for
    // finding live local variables), and scan up to StackBase.
    ULONG64 stack_offset = 0;
    if (debug_registers) {
      debug_registers->GetStackOffset(&stack_offset);
    }

    // Fall back to StackLimit if we can't get RSP.
    uint64_t scan_start = (stack_offset != 0 && stack_offset >= stack_limit &&
                           stack_offset < stack_base)
                              ? stack_offset
                              : stack_limit;
    uint64_t scan_size = std::min(stack_base - scan_start, kMaxStackScanSize);

    threads_scanned++;
    total_bytes_scanned += scan_size;

    size_t traces_before = traces->size();
    ScanMemoryRegionForTaskTraces(debug_data, scan_start, scan_size, traces);

    // Tag newly found traces with thread info.
    for (size_t t = traces_before; t < traces->size(); t++) {
      (*traces)[t].thread_id = thread_id;
      (*traces)[t].thread_sys_id = thread_sys_id;
    }
  }

  // Restore the original thread context.
  debug_system->SetCurrentThreadId(original_thread_id);

  if (out_threads_scanned) {
    *out_threads_scanned = threads_scanned;
  }
  if (out_threads_skipped) {
    *out_threads_skipped = threads_skipped;
  }
  if (out_total_bytes_scanned) {
    *out_total_bytes_scanned = total_bytes_scanned;
  }

  return S_OK;
}

}  // namespace tools::win::chromeexts
