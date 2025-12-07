// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_SPECIFIER_H_
#define UI_BASE_INTERACTION_ELEMENT_SPECIFIER_H_

#include <compare>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

#include "base/component_export.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

// Describes an element either by ID or by name.
//
// The default-constructed value is falsy. Passing a null ID or empty string
// also results in a falsy value.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) ElementSpecifier final {
 public:
  ElementSpecifier();
  ElementSpecifier(ElementIdentifier id);     // NOLINT
  ElementSpecifier(std::string_view name);    // NOLINT
  ElementSpecifier(const std::string& name);  // NOLINT
  ElementSpecifier(const char* name);         // NOLINT
  ElementSpecifier(const ElementSpecifier&);
  ElementSpecifier& operator=(const ElementSpecifier&);
  ~ElementSpecifier();

  operator bool() const {  // NOLINT
    return !std::holds_alternative<std::monostate>(specifier_);
  }

  bool operator!() const {
    return std::holds_alternative<std::monostate>(specifier_);
  }

  std::strong_ordering operator<=>(const ElementSpecifier& other) const =
      default;
  bool operator==(const ElementSpecifier& other) const = default;

  bool is_identifier() const {
    return std::holds_alternative<ElementIdentifier>(specifier_);
  }
  ElementIdentifier identifier() const {
    return std::holds_alternative<ElementIdentifier>(specifier_)
               ? std::get<ElementIdentifier>(specifier_)
               : ElementIdentifier();
  }

  bool is_name() const {
    return std::holds_alternative<std::string>(specifier_);
  }
  std::string_view name() const {
    return std::holds_alternative<std::string>(specifier_)
               ? std::get<std::string>(specifier_)
               : std::string_view();
  }

 private:
  using SpecifierType =
      std::variant<std::monostate, ElementIdentifier, std::string>;
  SpecifierType specifier_;
};

COMPONENT_EXPORT(UI_BASE_INTERACTION)
std::ostream& operator<<(std::ostream& os, ElementSpecifier specifier);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ELEMENT_SPECIFIER_H_
