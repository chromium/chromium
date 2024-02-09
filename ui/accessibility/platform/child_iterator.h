// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_CHILD_ITERATOR_H_
#define UI_ACCESSIBILITY_PLATFORM_CHILD_ITERATOR_H_

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) ChildIterator {
 public:
  virtual ~ChildIterator() = default;
  bool operator==(const ChildIterator& rhs) const {
    return GetIndexInParent() == rhs.GetIndexInParent();
  }
  bool operator!=(const ChildIterator& rhs) const {
    return GetIndexInParent() != rhs.GetIndexInParent();
  }
  // We can't have a pure virtual postfix increment/decrement operator
  // overloads, since postfix operator overloads need to be return by value, and
  // the overridden overloads in the derived classes would have to differ in
  // type to the ones declared here.
  virtual ChildIterator& operator++() = 0;
  virtual ChildIterator& operator--() = 0;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() const = 0;
  virtual std::optional<size_t> GetIndexInParent() const = 0;
  virtual AXPlatformNodeDelegate* get() const = 0;
  virtual AXPlatformNodeDelegate& operator*() const = 0;
  virtual AXPlatformNodeDelegate* operator->() const = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_CHILD_ITERATOR_H_
