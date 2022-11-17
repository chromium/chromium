// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PORTAL_H_
#define IPCZ_SRC_IPCZ_PORTAL_H_

#include <cstdint>
#include <utility>

#include "ipcz/api_object.h"
#include "ipcz/ipcz.h"
#include "ipcz/parcel.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/ref_counted.h"

namespace ipcz {

class Node;
class Router;

// A Portal owns a terminal Router along a route. Portals are thread-safe and
// are manipulated directly by public ipcz API calls.
class Portal : public APIObjectImpl<Portal, APIObject::kPortal> {
 public:
  using Pair = std::pair<Ref<Portal>, Ref<Portal>>;

  // Creates a new portal which assumes control over `router` and which lives on
  // `node`.
  Portal(Ref<Node> node, Ref<Router> router);

  const Ref<Node>& node() const { return node_; }
  const Ref<Router>& router() const { return router_; }

  // Creates a new pair of portals which live on `node` and which are directly
  // connected to each other by a LocalRouterLink.
  static Pair CreatePair(Ref<Node> node);

  // APIObject:
  IpczResult Close() override;
  bool CanSendFrom(Portal& sender) override;

  // ipcz portal API implementation:
  IpczResult QueryStatus(IpczPortalStatus& status);
  IpczResult Merge(Portal& other);

  IpczResult Put(absl::Span<const uint8_t> data,
                 absl::Span<const IpczHandle> handles,
                 const IpczPutLimits* limits);
  IpczResult BeginPut(IpczBeginPutFlags flags,
                      const IpczPutLimits* limits,
                      size_t& num_data_bytes,
                      void** data);
  IpczResult CommitPut(size_t num_data_bytes_produced,
                       absl::Span<const IpczHandle> handles);
  IpczResult AbortPut();

  IpczResult Get(IpczGetFlags flags,
                 void* data,
                 size_t* num_data_bytes,
                 IpczHandle* handles,
                 size_t* num_handles,
                 IpczHandle* parcel);
  IpczResult BeginGet(const void** data,
                      size_t* num_data_bytes,
                      size_t* num_handles);
  IpczResult CommitGet(size_t num_data_bytes_consumed,
                       absl::Span<IpczHandle> handles);
  IpczResult AbortGet();

 private:
  ~Portal() override;

  const Ref<Node> node_;
  const Ref<Router> router_;

  absl::Mutex mutex_;

  // The parcel being built by the in-progress two-phase Put operation, if one
  // is in progress.
  absl::optional<Parcel> pending_parcel_ ABSL_GUARDED_BY(mutex_);

  bool in_two_phase_put_ ABSL_GUARDED_BY(mutex_) = false;
  bool in_two_phase_get_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PORTAL_H_
