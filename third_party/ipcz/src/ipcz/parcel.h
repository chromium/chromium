// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PARCEL_H_
#define IPCZ_SRC_IPCZ_PARCEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ipcz/api_object.h"
#include "ipcz/ipcz.h"
#include "ipcz/sequence_number.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

// Represents a parcel queued within a portal, either for inbound retrieval or
// outgoing transfer.
class Parcel {
 public:
  Parcel();
  explicit Parcel(SequenceNumber sequence_number);
  Parcel(Parcel&& other);
  Parcel& operator=(Parcel&& other);
  ~Parcel();

  void set_sequence_number(SequenceNumber n) { sequence_number_ = n; }
  SequenceNumber sequence_number() const { return sequence_number_; }

  // Indicates whether this Parcel is empty, meaning its data and objects have
  // been fully consumed.
  bool empty() const { return data_view_.empty() && objects_view_.empty(); }

  void SetInlinedData(std::vector<uint8_t> data);
  void SetObjects(std::vector<Ref<APIObject>> objects);

  absl::Span<uint8_t> data_view() { return data_view_; }
  absl::Span<const uint8_t> data_view() const { return data_view_; }

  size_t data_size() const { return data_view().size(); }

  absl::Span<Ref<APIObject>> objects_view() { return objects_view_; }
  absl::Span<const Ref<APIObject>> objects_view() const {
    return objects_view_;
  }
  size_t num_objects() const { return objects_view().size(); }

  // Partially consumes the contents of this Parcel, advancing the front of
  // data_view() by `num_bytes` and filling `out_handles` (of size N) with
  // handles to the first N APIObjects in objects_view(). The front of
  // objects_view() is also advanced by N.
  //
  // Note that `num_bytes` must not be larger than the size of data_view(), and
  // the size of `out_handles` must not be larger than the size of
  // objects_view().
  void Consume(size_t num_bytes, absl::Span<IpczHandle> out_handles);

  // Produces a log-friendly description of the Parcel, useful for various
  // debugging log messages.
  std::string Describe() const;

 private:
  SequenceNumber sequence_number_{0};

  // A copy of the parcel's data, owned by the Parcel itself.
  // TODO(rockot): Also support data within a referenced shared memory fragment
  // rather than always retaining a separate copy.
  std::vector<uint8_t> inlined_data_;

  // The set of APIObjects attached to this parcel.
  std::vector<Ref<APIObject>> objects_;

  // Views into any unconsumed data and objects.
  absl::Span<uint8_t> data_view_;
  absl::Span<Ref<APIObject>> objects_view_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PARCEL_H_
