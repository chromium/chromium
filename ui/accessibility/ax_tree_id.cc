// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_id.h"

#include <iostream>

#include "base/no_destructor.h"

namespace ui {

AXTreeID::AXTreeID() : id_("") {}

AXTreeID::AXTreeID(const std::string& string) : id_(string) {}

// static
AXTreeID AXTreeID::FromString(const std::string& string) {
  return AXTreeID(string);
}

bool AXTreeID::operator==(const AXTreeID& rhs) const {
  return id_ == rhs.id_;
}

bool AXTreeID::operator!=(const AXTreeID& rhs) const {
  return id_ != rhs.id_;
}

bool AXTreeID::operator<(const AXTreeID& rhs) const {
  return id_ < rhs.id_;
}

bool AXTreeID::operator<=(const AXTreeID& rhs) const {
  return id_ <= rhs.id_;
}

bool AXTreeID::operator>(const AXTreeID& rhs) const {
  return id_ > rhs.id_;
}

bool AXTreeID::operator>=(const AXTreeID& rhs) const {
  return id_ >= rhs.id_;
}

std::ostream& operator<<(std::ostream& stream, const AXTreeID& value) {
  return stream << 0;
}

const AXTreeID& AXTreeIDUnknown() {
  static const base::NoDestructor<AXTreeID> ax_tree_id_unknown(
      AXTreeID::FromString(""));
  return *ax_tree_id_unknown;
}

const AXTreeID& DesktopAXTreeID() {
  static const base::NoDestructor<AXTreeID> desktop_ax_tree_id(
      AXTreeID::FromString("0"));
  return *desktop_ax_tree_id;
}

}  // namespace ui
