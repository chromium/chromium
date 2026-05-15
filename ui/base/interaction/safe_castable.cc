// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/safe_castable.h"

#include <string>

namespace ui {

std::string SafeCastable::ToString() const {
  return GetSafeCastableClassName();
}

void PrintTo(const SafeCastable& impl, std::ostream* os) {
  *os << impl.ToString();
}

std::ostream& operator<<(std::ostream& os, const SafeCastable& impl) {
  PrintTo(impl, &os);
  return os;
}

}  // namespace ui
