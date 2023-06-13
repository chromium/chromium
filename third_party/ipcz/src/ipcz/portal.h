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
#include "ipcz/pending_transaction_set.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
                 absl::Span<const IpczHandle> handles);
  IpczResult BeginPut(IpczBeginPutFlags flags,
                      volatile void** data,
                      size_t* num_bytes,
                      IpczTransaction* transaction);
  IpczResult EndPut(IpczTransaction transaction,
                    size_t num_bytes_produced,
                    absl::Span<const IpczHandle> handles,
                    IpczEndPutFlags flags);

  IpczResult Get(IpczGetFlags flags,
                 void* data,
                 size_t* num_data_bytes,
                 IpczHandle* handles,
                 size_t* num_handles,
                 IpczHandle* parcel);
  IpczResult BeginGet(IpczBeginGetFlags flags,
                      const volatile void** data,
                      size_t* num_data_bytes,
                      IpczHandle* handles,
                      size_t* num_handles,
                      IpczTransaction* transaction);
  IpczResult EndGet(IpczTransaction transaction,
                    IpczEndGetFlags flags,
                    IpczHandle* parcel);

 private:
  ~Portal() override;

  const Ref<Node> node_;
  const Ref<Router> router_;

  absl::Mutex mutex_;
  PendingTransactionSet pending_puts_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PORTAL_H_
