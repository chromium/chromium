// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PENDING_TRANSACTION_SET_
#define IPCZ_SRC_IPCZ_PENDING_TRANSACTION_SET_

#include <memory>
#include <set>

#include "ipcz/ipcz.h"
#include "ipcz/parcel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "util/unique_ptr_comparator.h"

namespace ipcz {

// PendingTransactionSet wraps a set of pending Parcel objects with a special
// case for the common scenario where single elements are repeatedly inserted
// and then removed from the set, repeatedly.
//
// Care is taken to ensure that any Parcel owned by this set has a stable
// address throughout its lifetime, exposed as an opaque IpczTransaction value.
class PendingTransactionSet {
 public:
  PendingTransactionSet();
  PendingTransactionSet(const PendingTransactionSet&) = delete;
  PendingTransactionSet& operator=(const PendingTransactionSet&) = delete;
  ~PendingTransactionSet();

  bool empty() const {
    return transactions_.empty() || has_retained_empty_transaction();
  }

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
  class Transaction {
   public:
    Transaction() = default;
    explicit Transaction(Parcel parcel)
        : parcel_(absl::in_place, std::move(parcel)) {}
    ~Transaction() = default;

    bool has_parcel() const { return parcel_.has_value(); }
    void set_parcel(Parcel parcel) { parcel_ = std::move(parcel); }

    bool CanPut(size_t num_data_bytes) const {
      return parcel_ && parcel_->data_view().size() >= num_data_bytes;
    }

    Parcel TakeParcel() {
      Parcel parcel = std::move(*parcel_);
      parcel_.reset();
      return parcel;
    }

   private:
    // As an optimization, we may lazily retain an allocated Transaction after
    // it's finalized so we can reuse its allocation within the set. This is an
    // optimization for the common case where single transactions are frequently
    // added and removed in series. Hence the optional Parcel.
    absl::optional<Parcel> parcel_;
  };

  using TransactionSet =
      std::set<std::unique_ptr<Transaction>, UniquePtrComparator>;

  static IpczTransaction AsIpczTransaction(Transaction& transaction) {
    return reinterpret_cast<IpczTransaction>(&transaction);
  }

  bool has_retained_empty_transaction() const {
    return transactions_.size() == 1 && !first().has_parcel();
  }

  const Transaction& first() const { return **transactions_.begin(); }
  Transaction& first() { return **transactions_.begin(); }

  Parcel Remove(TransactionSet::iterator it);

  TransactionSet transactions_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PENDING_TRANSACTION_SET_
