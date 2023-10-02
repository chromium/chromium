// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "variant_of_gced_type.h"

namespace blink {

void ForbidsVariantsOfGcedTypes() {
  {
    absl::variant<Base> not_ok;
    (void)not_ok;

    absl::variant<Base, Base> similarly_not_ok;
    (void)similarly_not_ok;

    absl::variant<int, Base> not_ok_either;
    (void)not_ok_either;

    absl::variant<int, Derived> ditto;
    (void)ditto;

    new absl::variant<Mixin>;
  }

  {
    std::variant<Base> not_ok;
    (void)not_ok;

    std::variant<Base, Base> similarly_not_ok;
    (void)similarly_not_ok;

    std::variant<int, Base> not_ok_either;
    (void)not_ok_either;

    std::variant<int, Derived> ditto;
    (void)ditto;

    new std::variant<Mixin>;
  }
}

}  // namespace blink
