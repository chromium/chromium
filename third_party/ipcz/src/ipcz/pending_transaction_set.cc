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

IpczTransaction PendingTransactionSet::Add(std::unique_ptr<Parcel> parcel) {
  IpczTransaction transaction = AsIpczTransaction(*parcel);
  transactions_.insert(std::move(parcel));
  return transaction;
}

std::unique_ptr<Parcel> PendingTransactionSet::FinalizeForGet(
    IpczTransaction transaction) {
  Parcel* parcel = reinterpret_cast<Parcel*>(transaction);
  auto it = transactions_.find(parcel);
  if (it != transactions_.end() && parcel) {
    return Extract(it);
  }

  return nullptr;
}

std::unique_ptr<Parcel> PendingTransactionSet::FinalizeForPut(
    IpczTransaction transaction,
    size_t num_data_bytes) {
  Parcel* parcel = reinterpret_cast<Parcel*>(transaction);
  auto it = transactions_.find(parcel);
  if (it != transactions_.end() && parcel &&
      parcel->data_view().size() >= num_data_bytes) {
    return Extract(it);
  }

  return nullptr;
}

std::unique_ptr<Parcel> PendingTransactionSet::Extract(
    TransactionSet::iterator it) {
  return std::move(transactions_.extract(it).value());
}

}  // namespace ipcz
