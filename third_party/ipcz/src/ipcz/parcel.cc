// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/parcel.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {

Parcel::Parcel() = default;

Parcel::Parcel(SequenceNumber sequence_number)
    : sequence_number_(sequence_number) {}

// Note: We do not use default move construction or assignment because we want
// to explicitly clear the data and object views of the moved-from Parcel.
Parcel::Parcel(Parcel&& other)
    : sequence_number_(other.sequence_number_),
      remote_source_(std::move(other.remote_source_)),
      inlined_data_(std::move(other.inlined_data_)),
      data_fragment_(std::exchange(other.data_fragment_, {})),
      data_fragment_memory_(
          std::exchange(other.data_fragment_memory_, nullptr)),
      objects_(std::move(other.objects_)),
      data_view_(std::exchange(other.data_view_, {})),
      objects_view_(std::exchange(other.objects_view_, {})) {}

Parcel& Parcel::operator=(Parcel&& other) {
  sequence_number_ = other.sequence_number_;
  remote_source_ = std::move(other.remote_source_);
  inlined_data_ = std::move(other.inlined_data_);
  data_fragment_ = std::exchange(other.data_fragment_, {});
  data_fragment_memory_ = std::move(other.data_fragment_memory_);
  objects_ = std::move(other.objects_);
  data_view_ = std::exchange(other.data_view_, {});
  objects_view_ = std::exchange(other.objects_view_, {});
  return *this;
}

Parcel::~Parcel() {
  for (Ref<APIObject>& object : objects_) {
    if (object) {
      object->Close();
    }
  }

  if (!data_fragment_.is_null()) {
    ABSL_ASSERT(data_fragment_memory_);
    data_fragment_memory_->FreeFragment(data_fragment_);
  }
}

void Parcel::SetInlinedData(std::vector<uint8_t> data) {
  inlined_data_ = std::move(data);
  data_view_ = absl::MakeSpan(inlined_data_);
}

void Parcel::AllocateData(size_t num_bytes,
                          bool allow_partial,
                          NodeLinkMemory* memory) {
  // This should never be called on a Parcel that already has data.
  ABSL_ASSERT(inlined_data_.empty());
  ABSL_ASSERT(data_fragment_.is_null());
  ABSL_ASSERT(data_view_.empty());

  Fragment fragment;
  if (memory && num_bytes > 0) {
    const size_t requested_fragment_size = num_bytes + sizeof(FragmentHeader);
    if (allow_partial) {
      fragment = memory->AllocateFragmentBestEffort(requested_fragment_size);
    } else {
      fragment = memory->AllocateFragment(requested_fragment_size);
    }
  }

  if (fragment.is_null()) {
    inlined_data_.resize(num_bytes);
    data_view_ = absl::MakeSpan(inlined_data_);
    return;
  }

  // The smallest possible Fragment we could allocate above is still
  // subsantially larger than a FragmentHeader.
  ABSL_ASSERT(fragment.size() > sizeof(FragmentHeader));

  // Leave room for a FragmentHeader at the start of the fragment. This header
  // is not written until CommitData().
  const size_t data_size =
      std::min(num_bytes, fragment.size() - sizeof(FragmentHeader));
  data_fragment_ = fragment;
  data_fragment_memory_ = WrapRefCounted(memory);
  data_view_ =
      fragment.mutable_bytes().subspan(sizeof(FragmentHeader), data_size);
}

bool Parcel::AdoptDataFragment(Ref<NodeLinkMemory> memory,
                               const Fragment& fragment) {
  // This should never be called on a Parcel that already has data.
  ABSL_ASSERT(inlined_data_.empty());
  ABSL_ASSERT(data_fragment_.is_null());
  ABSL_ASSERT(data_view_.empty());

  if (!fragment.is_addressable() || fragment.size() <= sizeof(FragmentHeader)) {
    return false;
  }

  // This load-acquire is balanced by a store-release in CommitData() by the
  // producer of this data.
  const auto& header =
      *reinterpret_cast<const FragmentHeader*>(fragment.bytes().data());
  const uint32_t data_size = header.size.load(std::memory_order_acquire);
  const size_t max_data_size = fragment.size() - sizeof(FragmentHeader);
  if (data_size > max_data_size) {
    return false;
  }

  data_fragment_ = fragment;
  data_fragment_memory_ = std::move(memory);
  data_view_ =
      fragment.mutable_bytes().subspan(sizeof(FragmentHeader), data_size);
  return true;
}

void Parcel::SetObjects(std::vector<Ref<APIObject>> objects) {
  objects_ = std::move(objects);
  objects_view_ = absl::MakeSpan(objects_);
}

void Parcel::CommitData(size_t num_bytes) {
  data_view_ = data_view_.first(num_bytes);
  if (data_fragment_.is_null()) {
    return;
  }

  ABSL_ASSERT(data_fragment_.is_addressable());
  ABSL_ASSERT(num_bytes <= data_fragment_.size() + sizeof(FragmentHeader));
  auto& header =
      *reinterpret_cast<FragmentHeader*>(data_fragment_.mutable_bytes().data());
  header.reserved = 0;

  // This store-release is balanced by the load-acquire in AdoptDataFragment()
  // by the eventual consumer of this data.
  //
  // A static_cast is fine here: we do not allocate 4 GB+ fragments.
  header.size.store(static_cast<uint32_t>(num_bytes),
                    std::memory_order_release);
}

void Parcel::ReleaseDataFragment() {
  ABSL_ASSERT(!data_fragment_.is_null());
  data_fragment_ = {};
  data_fragment_memory_.reset();
  data_view_ = {};
}

void Parcel::Consume(size_t num_bytes, absl::Span<IpczHandle> out_handles) {
  auto data = data_view();
  auto objects = objects_view();
  ABSL_ASSERT(num_bytes <= data.size());
  ABSL_ASSERT(out_handles.size() <= objects.size());
  for (size_t i = 0; i < out_handles.size(); ++i) {
    out_handles[i] = APIObject::ReleaseAsHandle(std::move(objects[i]));
  }

  data_view_.remove_prefix(num_bytes);
  objects_view_.remove_prefix(out_handles.size());
}

std::string Parcel::Describe() const {
  std::stringstream ss;
  ss << "parcel " << sequence_number() << " (";
  if (!data_view().empty()) {
    // Cheesy heuristic: if the first character is an ASCII letter or number,
    // assume the parcel data is human-readable and print a few characters.
    if (std::isalnum(data_view()[0])) {
      const absl::Span<const uint8_t> preview = data_view().subspan(0, 8);
      ss << "\"" << std::string(preview.begin(), preview.end());
      if (preview.size() < data_size()) {
        ss << "...\", " << data_size() << " bytes";
      } else {
        ss << '"';
      }
    }
  } else {
    ss << "no data";
  }
  if (!objects_view().empty()) {
    ss << ", " << num_objects() << " handles";
  }
  ss << ")";
  return ss.str();
}

}  // namespace ipcz
