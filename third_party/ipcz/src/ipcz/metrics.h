// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_METRICS_H_
#define IPCZ_SRC_IPCZ_METRICS_H_

#include <cstddef>

namespace ipcz::metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(IpczBlockAllocationSize)
enum class BlockAllocationSize {
  kOther = 0,
  k64B = 1,
  k128B = 2,
  k256B = 3,
  k512B = 4,
  k1KB = 5,
  k2KB = 6,
  k4KB = 7,
  k8KB = 8,
  k16KB = 9,
  k32KB = 10,
  k64KB = 11,
  k128KB = 12,
  k256KB = 13,
  k512KB = 14,
  k1MB = 15,
  kMaxValue = k1MB,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:IpczBlockAllocationSize)

enum class BlockAllocationSource {
  kBufferPool,
  kParcel,
};

void RecordAllocateBlockResult(BlockAllocationSource source,
                               size_t block_size,
                               bool success);

}  // namespace ipcz::metrics

#endif  // IPCZ_SRC_IPCZ_METRICS_H_
