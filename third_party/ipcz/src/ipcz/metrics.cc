// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"

namespace ipcz::metrics {

namespace {

constexpr double kMetricsSubsampleRate = 0.001;

BlockAllocationSize BlockSizeToBucket(size_t block_size) {
  switch (block_size) {
    case 64:
      return BlockAllocationSize::k64B;
    case 128:
      return BlockAllocationSize::k128B;
    case 256:
      return BlockAllocationSize::k256B;
    case 512:
      return BlockAllocationSize::k512B;
    case 1024:
      return BlockAllocationSize::k1KB;
    case 2048:
      return BlockAllocationSize::k2KB;
    case 4096:
      return BlockAllocationSize::k4KB;
    case 8192:
      return BlockAllocationSize::k8KB;
    case 16384:
      return BlockAllocationSize::k16KB;
    case 32768:
      return BlockAllocationSize::k32KB;
    case 65536:
      return BlockAllocationSize::k64KB;
    case 131072:
      return BlockAllocationSize::k128KB;
    case 262144:
      return BlockAllocationSize::k256KB;
    case 524288:
      return BlockAllocationSize::k512KB;
    case 1048576:
      return BlockAllocationSize::k1MB;
    default:
      return BlockAllocationSize::kOther;
  }
}

}  // namespace

void RecordAllocateBlockResult(BlockAllocationSource source,
                               size_t block_size,
                               bool success) {
  if (!base::ShouldRecordSubsampledMetric(kMetricsSubsampleRate)) {
    return;
  }
  const BlockAllocationSize size_bucket = BlockSizeToBucket(block_size);
  switch (source) {
    case BlockAllocationSource::kBufferPool:
      base::UmaHistogramBoolean("Mojo.Ipcz.BufferPoolAllocateBlockResult",
                                success);
      base::UmaHistogramEnumeration(
          success ? "Mojo.Ipcz.BufferPoolAllocateBlockSuccessSize2"
                  : "Mojo.Ipcz.BufferPoolAllocateBlockFailureSize2",
          size_bucket);
      break;

    case BlockAllocationSource::kParcel:
      base::UmaHistogramBoolean("Mojo.Ipcz.ParcelAllocateBlockResult", success);
      base::UmaHistogramEnumeration(
          success ? "Mojo.Ipcz.ParcelAllocateBlockSuccessSize"
                  : "Mojo.Ipcz.ParcelAllocateBlockFailureSize",
          size_bucket);
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace ipcz::metrics
