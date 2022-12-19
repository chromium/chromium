// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/framework_specific_implementation.h"

#include <string>

namespace ui {

std::string FrameworkSpecificImplementation::ToString() const {
  return GetImplementationName();
}

void PrintTo(const FrameworkSpecificImplementation& impl, std::ostream* os) {
  *os << impl.ToString();
}

std::ostream& operator<<(std::ostream& os,
                         const FrameworkSpecificImplementation& impl) {
  PrintTo(impl, &os);
  return os;
}

}  // namespace ui
