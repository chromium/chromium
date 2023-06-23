// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/pending_transaction_set.h"

#include <utility>

#include "ipcz/parcel.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

PendingTransactionSet::PendingTransactionSet() = default;

PendingTransactionSet::~PendingTransactionSet() = default;

IpczTransaction PendingTransactionSet::Add(Parcel parcel) {
  if (has_retained_empty_transaction()) {
    first().set_parcel(std::move(parcel));
    return AsIpczTransaction(first());
  }

  auto new_transaction = std::make_unique<Transaction>(std::move(parcel));
  IpczTransaction transaction = AsIpczTransaction(*new_transaction);
  transactions_.insert(std::move(new_transaction));
  return transaction;
}

absl::optional<Parcel> PendingTransactionSet::FinalizeForGet(
    IpczTransaction transaction) {
  auto it = transactions_.find(reinterpret_cast<Transaction*>(transaction));
  if (it != transactions_.end() && (*it)->has_parcel()) {
    return Remove(it);
  }

  return absl::nullopt;
}

absl::optional<Parcel> PendingTransactionSet::FinalizeForPut(
    IpczTransaction transaction,
    size_t num_data_bytes) {
  auto it = transactions_.find(reinterpret_cast<Transaction*>(transaction));
  if (it != transactions_.end() && (*it)->CanPut(num_data_bytes)) {
    return Remove(it);
  }

  return absl::nullopt;
}

Parcel PendingTransactionSet::Remove(TransactionSet::iterator it) {
  Parcel parcel = (*it)->TakeParcel();
  if (transactions_.size() == 1) {
    // If this was the only Transaction left, leave it around for future reuse.
    ABSL_HARDENING_ASSERT(it == transactions_.begin());
  } else {
    transactions_.erase(it);
  }
  return parcel;
}

}  // namespace ipcz
