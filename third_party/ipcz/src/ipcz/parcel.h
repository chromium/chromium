// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PARCEL_H_
#define IPCZ_SRC_IPCZ_PARCEL_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ipcz/api_object.h"
#include "ipcz/fragment.h"
#include "ipcz/ipcz.h"
#include "ipcz/sequence_number.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class NodeLink;
class NodeLinkMemory;

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

  // Allocates `num_bytes` of storage for this parcel's data. If `memory` is
  // non-null then its fragment pool is the preferred allocation source.
  // Otherwise memory is allocated on the heap, and the data placed therein will
  // be inlined within any message that transmits this parcel.
  //
  // If `memory` is non-null and `allow_partial` is true, this may allocate less
  // memory than requested if some reasonable amount of space is still available
  // within `memory`.
  //
  // Upon return, data_view() references the allocated memory wherever it
  // resides.
  void AllocateData(size_t num_bytes,
                    bool allow_partial,
                    NodeLinkMemory* memory);

  // Configures this Parcel to adopt its data fragment from the `fragment`
  // belonging to `memory`. `fragment` must be addressable and must have a valid
  // FragmentHeader at the start describing the data which follows. Otherwise
  // this returns false.
  bool AdoptDataFragment(Ref<NodeLinkMemory> memory, const Fragment& fragment);

  void set_remote_source(Ref<NodeLink> source) {
    remote_source_ = std::move(source);
  }
  const Ref<NodeLink>& remote_source() const { return remote_source_; }

  absl::Span<uint8_t> data_view() { return data_view_; }
  absl::Span<const uint8_t> data_view() const { return data_view_; }

  size_t data_size() const { return data_view().size(); }

  const Fragment& data_fragment() const { return data_fragment_; }
  const Ref<NodeLinkMemory>& data_fragment_memory() const {
    return data_fragment_memory_;
  }

  absl::Span<Ref<APIObject>> objects_view() { return objects_view_; }
  absl::Span<const Ref<APIObject>> objects_view() const {
    return objects_view_;
  }
  size_t num_objects() const { return objects_view().size(); }

  // Commits `num_bytes` of data to this Parcel's data fragment. This MUST be
  // called after populating the Parcel's data, and it must be called by the
  // same thread that populated the data. If the parcel's data is inlined rather
  // than stored in a fragment, this is a no-op.
  void CommitData(size_t num_bytes);

  // Relinquishes ownership of this Parcel's data fragment, if applicable. This
  // prevents the fragment from being freed upon Parcel destruction.
  void ReleaseDataFragment();

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
  // When Parcel data is in a shared memory fragment, this header sits at the
  // front of the fragment.
  struct IPCZ_ALIGN(8) FragmentHeader {
    // The size in bytes of the parcel data which immediately follows this
    // header. Must not extend beyond the bounds of the fragment itself.
    //
    // Access to this atomic is also used to synchronize access to the parcel
    // data. Writes to the fragment must be finalized by calling CommitData(),
    // and any node receiving this parcel must adopt the fragment by calling
    // AdoptDataFragment().
    std::atomic<uint32_t> size;

    // Reserved padding for 8-byte parcel data alignment.
    uint32_t reserved;
  };

  SequenceNumber sequence_number_{0};

  // If this Parcel was received from a remote node, this tracks the NodeLink
  // which received it.
  Ref<NodeLink> remote_source_;

  // A copy of the parcel's data, owned by the Parcel itself. Used only if
  // `data_fragment_` is null.
  std::vector<uint8_t> inlined_data_;

  // If non-null, a shared memory fragment which contains this parcel's data.
  Fragment data_fragment_;
  Ref<NodeLinkMemory> data_fragment_memory_;

  // The set of APIObjects attached to this parcel.
  std::vector<Ref<APIObject>> objects_;

  // Views into any unconsumed data and objects.
  absl::Span<uint8_t> data_view_;
  absl::Span<Ref<APIObject>> objects_view_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PARCEL_H_
