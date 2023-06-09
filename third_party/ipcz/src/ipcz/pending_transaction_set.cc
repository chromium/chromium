// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/pending_transaction_set.h"

#include <utility>

#include "ipcz/parcel.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

namespace {

IpczTransaction AsTransaction(Parcel& parcel) {
  return reinterpret_cast<IpczTransaction>(&parcel);
}

}  // namespace

PendingTransactionSet::PendingTransactionSet() = default;

PendingTransactionSet::~PendingTransactionSet() = default;

IpczTransaction PendingTransactionSet::Add(Parcel parcel) {
  if (!inline_parcel_) {
    return AsTransaction(inline_parcel_.emplace(std::move(parcel)));
  }

  auto new_parcel = std::make_unique<Parcel>(std::move(parcel));
  IpczTransaction transaction = AsTransaction(*new_parcel);
  other_parcels_[transaction] = std::move(new_parcel);
  return transaction;
}

absl::optional<Parcel> PendingTransactionSet::FinalizeForGet(
    IpczTransaction transaction) {
  if (inline_parcel_ && AsTransaction(*inline_parcel_) == transaction) {
    Parcel parcel = std::move(*inline_parcel_);
    inline_parcel_.reset();
    return parcel;
  }

  auto it = other_parcels_.find(transaction);
  if (it != other_parcels_.end()) {
    Parcel parcel = std::move(*it->second);
    other_parcels_.erase(it);
    return parcel;
  }

  return absl::nullopt;
}

absl::optional<Parcel> PendingTransactionSet::FinalizeForPut(
    IpczTransaction transaction,
    size_t num_data_bytes) {
  if (inline_parcel_ && AsTransaction(*inline_parcel_) == transaction &&
      inline_parcel_->data_view().size() >= num_data_bytes) {
    Parcel parcel = std::move(*inline_parcel_);
    inline_parcel_.reset();
    return parcel;
  }

  auto it = other_parcels_.find(transaction);
  if (it != other_parcels_.end() &&
      it->second->data_view().size() >= num_data_bytes) {
    Parcel parcel = std::move(*it->second);
    other_parcels_.erase(it);
    return parcel;
  }

  return absl::nullopt;
}

}  // namespace ipcz
