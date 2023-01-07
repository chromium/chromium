// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/fragment_ref.h"

#include <algorithm>
#include <utility>

#include "ipcz/fragment.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/ref_counted_fragment.h"
#include "util/ref_counted.h"

namespace ipcz::internal {

GenericFragmentRef::GenericFragmentRef() = default;

GenericFragmentRef::GenericFragmentRef(Ref<NodeLinkMemory> memory,
                                       const Fragment& fragment)
    : memory_(std::move(memory)), fragment_(fragment) {}

GenericFragmentRef::~GenericFragmentRef() {
  reset();
}

void GenericFragmentRef::reset() {
  Ref<NodeLinkMemory> memory = std::move(memory_);
  if (fragment_.is_null()) {
    return;
  }

  Fragment fragment;
  std::swap(fragment, fragment_);
  if (!fragment.is_addressable()) {
    return;
  }

  auto* ref_counted = static_cast<RefCountedFragment*>(fragment.address());
  if (ref_counted->ReleaseRef() > 1 || !memory) {
    return;
  }

  memory->FreeFragment(fragment);
}

Fragment GenericFragmentRef::release() {
  Fragment fragment;
  std::swap(fragment_, fragment);
  memory_.reset();
  return fragment;
}

}  // namespace ipcz::internal
