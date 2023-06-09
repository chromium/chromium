// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PENDING_TRANSACTION_SET_
#define IPCZ_SRC_IPCZ_PENDING_TRANSACTION_SET_

#include <map>
#include <memory>

#include "ipcz/ipcz.h"
#include "ipcz/parcel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ipcz {

// PendingTransactionSet wraps a set of pending Parcel objects with special case
// for a 1-element set to use inline storage instead. This set does not provide
// facilities for iteration, only for insertion and removal.
//
// Care is taken to ensure that any Parcel owned by this set has a stable
// address throughout its lifetime, exposed as an opaque IpczTransaction value.
class PendingTransactionSet {
 public:
  PendingTransactionSet();
  PendingTransactionSet(const PendingTransactionSet&) = delete;
  PendingTransactionSet& operator=(const PendingTransactionSet&) = delete;
  ~PendingTransactionSet();

  bool empty() const { return !inline_parcel_ && other_parcels_.empty(); }

  // Adds `parcel` to this set, returning an opaque IpczTransaction value to
  // reference it.
  IpczTransaction Add(Parcel parcel);

  // Finalizes the transaction identified by `transaction`, returning its
  // underlying Parcel. Only succeeds if `transaction` is a valid transaction.
  absl::optional<Parcel> FinalizeForGet(IpczTransaction transaction);

  // Finalizes the transaction identified by `transaction`, returning its
  // underlying Parcel so that data can be committed to it and it can be put
  // into a portal. Only succeeds if `transaction` is a valid transaction and
  // `num_data_bytes` does not exceed the total capacity of the underlying
  // Parcel. Note that this does not actually commit any data to the parcel.
  absl::optional<Parcel> FinalizeForPut(IpczTransaction transaction,
                                        size_t num_data_bytes);

 private:
  // Preferred storage for a Parcel in the set.
  absl::optional<Parcel> inline_parcel_;

  // Run-off storage for other parcels when `inline_parcel_` is occupied. Note
  // that std::map is chosen for its stable iterators across insertion and
  // deletion.
  std::map<IpczTransaction, std::unique_ptr<Parcel>> other_parcels_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PENDING_TRANSACTION_SET_
