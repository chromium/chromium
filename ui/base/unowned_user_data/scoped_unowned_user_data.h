// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_
#define UI_BASE_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_

#include <concepts>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "ui/base/identifier/typed_identifier.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace ui {

namespace internal {

// Tag type used for ADL lookup of `CheckSingleUnownedUserDataInHierarchy`.
// Each `DECLARE_USER_DATA(ClassName)` injects a hidden friend overload for
// `DerivedT` derived from `ClassName`. Calling that function with
// `UnownedUserDataHierarchyCheck<T>{}` succeeds iff exactly one class in `T`'s
// inheritance hierarchy declares `DECLARE_USER_DATA` (zero declarations finds
// no overload; multiple declarations in a hierarchy are ambiguous because
// neither `std::derived_from` constraint subsumes the other).
template <typename T>
struct UnownedUserDataHierarchyCheck {};

template <typename T>
concept SingleUnownedUserDataInHierarchy = requires {
  CheckSingleUnownedUserDataInHierarchy(UnownedUserDataHierarchyCheck<T>{});
};

// Note: C++23 does not allow passing a `constexpr std::string_view` to
// `static_assert` (`-Wc++26-extensions`), so share the diagnostic message via a
// `consteval` helper instead.
template <typename T>
consteval void AssertSingleUnownedUserDataInHierarchy() {
  static_assert(SingleUnownedUserDataInHierarchy<T>, R"(
DECLARE_USER_DATA may not be used by both a class and one of its descendants.

If you wish to have From() methods on both a base class and one of its
descendants, derive the base class from SafeCastable and have the derived
class' From() method use AsA():

DerivedClass* DerivedClass::From(Owner* owner) {
  if (auto* base = BaseClass::From(owner)) {
    return base->AsA<DerivedClass>();
  }
  return nullptr;
}
)");
}

}  // namespace internal

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
    // Checked here rather than at class scope because `T` is typically
    // incomplete when `ScopedUnownedUserData<T>` is declared as a member of
    // `T`.
    internal::AssertSingleUnownedUserDataInHierarchy<T>();
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

#define DECLARE_USER_DATA(ClassName)                                          \
  template <typename DerivedT>                                                \
    requires std::derived_from<DerivedT, ClassName>                           \
  friend consteval void CheckSingleUnownedUserDataInHierarchy(                \
      ::ui::internal::UnownedUserDataHierarchyCheck<DerivedT>) {}             \
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(::ui::UnownedUserDataHost::UntypedKey, \
                                       ClassName, kDataKey);                  \
  static ClassName* Get(::ui::UnownedUserDataHost& host);                     \
  static const ClassName* Get(const ::ui::UnownedUserDataHost& host)

#define DEFINE_USER_DATA(ClassName)                                        \
  ClassName* ClassName::Get(::ui::UnownedUserDataHost& host) {             \
    return ::ui::ScopedUnownedUserData<ClassName>::Get(host);              \
  }                                                                        \
  const ClassName* ClassName::Get(const ::ui::UnownedUserDataHost& host) { \
    return ::ui::ScopedUnownedUserData<ClassName>::Get(host);              \
  }                                                                        \
  DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(                                     \
      ClassName, ::ui::UnownedUserDataHost::UntypedKey, ClassName, kDataKey)

#endif  // UI_BASE_UNOWNED_USER_DATA_SCOPED_UNOWNED_USER_DATA_H_
