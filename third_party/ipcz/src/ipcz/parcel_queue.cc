// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/parcel_queue.h"

namespace ipcz {

bool ParcelQueue::Consume(size_t num_bytes_consumed,
                          absl::Span<IpczHandle> handles) {
  if (!HasNextElement()) {
    return false;
  }

  Parcel& p = NextElement();
  ABSL_ASSERT(p.data_size() >= num_bytes_consumed);
  ABSL_ASSERT(p.num_objects() >= handles.size());
  p.Consume(num_bytes_consumed, handles);
  PartiallyConsumeNextElement(num_bytes_consumed);
  if (p.empty()) {
    Parcel discarded;
    const bool ok = Pop(discarded);
    ABSL_ASSERT(ok);
  }

  return true;
}

}  // namespace ipcz
