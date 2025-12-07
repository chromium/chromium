// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_
#define UI_BASE_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_

#include <concepts>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "ui/base/interaction/typed_identifier.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace ui {

// A scoped class to set and unset an UnownedUserData entry on an
// UnownedUserDataHost.
//
// Example Usage:
//   class MyFeature {
//    public:
//     // This provides both the kDataKey and the Get() function.
//     DECLARE_USER_DATA(MyFeature);
//
//     // The class doesn't have to contain its own holder, but if you want you
//     // can add one to the private data and initialize it in the constructor:
//
//     explicit MyFeature(UnownedUserDataHost& host)
//         : scoped_data_holder_(host, *this) {}
//
//    private:
//     ScopedUnownedUserData<MyFeature> scoped_data_holder_;
//   };
//
//   DEFINE_USER_DATA(MyFeature);
template <class T>
class ScopedUnownedUserData {
 public:
  using PassKey = base::PassKey<ScopedUnownedUserData<T>>;

  ScopedUnownedUserData(UnownedUserDataHost& host, T& data)
      : host_(host), data_(data) {
    host_->Set(PassKey(), T::kDataKey, *data_);
  }

  virtual ~ScopedUnownedUserData() { host_->Erase(PassKey(), T::kDataKey); }

  static T* Get(UnownedUserDataHost& host) {
    return host.Get(PassKey(), T::kDataKey);
  }

  static const T* Get(const UnownedUserDataHost& host) {
    return host.Get(PassKey(), T::kDataKey);
  }

 private:
  raw_ref<UnownedUserDataHost> host_;
  raw_ref<T> data_;
};

}  // namespace ui

// Helper macros. See above for usage.

#define DECLARE_USER_DATA(ClassName)                         \
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(ClassName, kDataKey); \
  static ClassName* Get(::ui::UnownedUserDataHost& host);    \
  static const ClassName* Get(const ::ui::UnownedUserDataHost& host)

#define DEFINE_USER_DATA(ClassName)                                        \
  ClassName* ClassName::Get(::ui::UnownedUserDataHost& host) {             \
    return ::ui::ScopedUnownedUserData<ClassName>::Get(host);              \
  }                                                                        \
  const ClassName* ClassName::Get(const ::ui::UnownedUserDataHost& host) { \
    return ::ui::ScopedUnownedUserData<ClassName>::Get(host);              \
  }                                                                        \
  DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(ClassName, ClassName, kDataKey)

#endif  // UI_BASE_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_
