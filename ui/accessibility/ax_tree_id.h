// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_ID_H_
#define UI_ACCESSIBILITY_AX_TREE_ID_H_

#include <string>

#include "ui/accessibility/ax_export.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace ax {
namespace mojom {
class AXTreeIDDataView;
}
}  // namespace ax

namespace ui {

// A unique ID representing an accessibility tree.
class AX_EXPORT AXTreeID {
 public:
  AXTreeID();
  static AXTreeID FromString(const std::string& string);
  const std::string& ToString() const { return id_; }
  operator std::string() const { return id_; }

  bool operator==(const AXTreeID& rhs) const;
  bool operator!=(const AXTreeID& rhs) const;
  bool operator<(const AXTreeID& rhs) const;
  bool operator<=(const AXTreeID& rhs) const;
  bool operator>(const AXTreeID& rhs) const;
  bool operator>=(const AXTreeID& rhs) const;

 private:
  explicit AXTreeID(const std::string& string);

  friend struct mojo::StructTraits<ax::mojom::AXTreeIDDataView, ui::AXTreeID>;

  std::string id_;
};

AX_EXPORT std::ostream& operator<<(std::ostream& stream, const AXTreeID& value);

// The value to use when an AXTreeID is unknown.
AX_EXPORT extern const AXTreeID& AXTreeIDUnknown();

// The AXTreeID of the desktop.
AX_EXPORT extern const AXTreeID& DesktopAXTreeID();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_ID_H_
