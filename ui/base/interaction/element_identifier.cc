// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_identifier.h"

namespace ui {

void PrintTo(ElementIdentifier element_identifier, std::ostream* os) {
  const internal::ElementIdentifierImpl* impl =
      reinterpret_cast<const internal::ElementIdentifierImpl*>(
          element_identifier.raw_value());
  *os << "ElementIdentifier " << impl << " [" << (impl ? impl->name : "")
      << "]";
}

void PrintTo(ElementContext element_context, std::ostream* os) {
  *os << "ElementContext " << static_cast<const void*>(element_context);
}

}  // namespace ui
