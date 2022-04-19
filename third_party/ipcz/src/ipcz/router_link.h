// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_LINK_H_
#define IPCZ_SRC_IPCZ_ROUTER_LINK_H_

#include "ipcz/link_type.h"
#include "ipcz/sequence_number.h"
#include "util/ref_counted.h"

namespace ipcz {

// A RouterLink represents one endpoint of a link between two Routers. All
// subclasses must be thread-safe.
class RouterLink : public RefCounted {
 public:
  using Pair = std::pair<Ref<RouterLink>, Ref<RouterLink>>;

  // Indicates what type of link this is. See LinkType documentation.
  virtual LinkType GetType() const = 0;

 protected:
  ~RouterLink() override = default;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_LINK_H_
