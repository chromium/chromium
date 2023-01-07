// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PARCEL_QUEUE_H_
#define IPCZ_SRC_IPCZ_PARCEL_QUEUE_H_

#include <cstddef>
#include <cstdint>

#include "ipcz/ipcz.h"
#include "ipcz/parcel.h"
#include "ipcz/sequence_number.h"
#include "ipcz/sequenced_queue.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {

struct ParcelQueueTraits {
  static size_t GetElementSize(const Parcel& parcel) {
    return parcel.data_view().size();
  }
};

// A ParcelQueue is a SequencedQueue of Parcel objects which also tracks the
// total data size (in bytes) of available parcels at the head of the queue and
// allows for proper accounting of a partially consumed parcel at the head of
// the queue.
class ParcelQueue : public SequencedQueue<Parcel, ParcelQueueTraits> {
 public:
  // Fully or partially consumes the next parcel in the queue. Returns true iff
  // there was in fact a parcel at the head of the queue.
  bool Consume(size_t num_bytes_consumed, absl::Span<IpczHandle> handles);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PARCEL_QUEUE_H_
