// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_specifier.h"

#include <ostream>
#include <string_view>

#include "base/check.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

ElementSpecifier::ElementSpecifier() = default;

ElementSpecifier::ElementSpecifier(ElementIdentifier id)
    : specifier_(id ? SpecifierType(id) : SpecifierType()) {}

ElementSpecifier::ElementSpecifier(std::string_view name)
    : specifier_(name.empty() ? SpecifierType()
                              : SpecifierType(std::string(name))) {}

ElementSpecifier::ElementSpecifier(const std::string& name)
    : ElementSpecifier(std::string_view(name)) {}

ElementSpecifier::ElementSpecifier(const char* name)
    : ElementSpecifier(std::string_view(name)) {}

ElementSpecifier::ElementSpecifier(const ElementSpecifier&) = default;
ElementSpecifier& ElementSpecifier::operator=(const ElementSpecifier&) =
    default;
ElementSpecifier::~ElementSpecifier() = default;

std::ostream& operator<<(std::ostream& os, ElementSpecifier spec) {
  if (spec.is_identifier()) {
    os << spec.identifier();
  } else if (spec.is_name()) {
    os << '"' << spec.name() << '"';
  } else {
    os << "[null]";
  }
  return os;
}

}  // namespace ui
