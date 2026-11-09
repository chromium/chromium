// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_ERROR_H_
#define UI_GFX_X_ERROR_H_

#include <cstdint>
#include <string>
#include <type_traits>

#include "base/component_export.h"

namespace x11 {

// Generic interface for X11 errors.
class COMPONENT_EXPORT(X11) Error {
 public:
  Error();
  virtual ~Error();

  virtual std::string ToString() const = 0;

  template <typename T>
  T* As() {
    static_assert(std::is_base_of_v<Error, T>, "T must be an x11::Error");
    static_assert(T::kTypeId > 0, "T must have a non-zero kTypeId");
    return GetTypeId() == T::kTypeId ? static_cast<T*>(this) : nullptr;
  }

  template <typename T>
  const T* As() const {
    return const_cast<Error*>(this)->As<T>();
  }

 protected:
  virtual uint8_t GetTypeId() const;
};

}  // namespace x11

#endif  // UI_GFX_X_ERROR_H_
