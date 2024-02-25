// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_CHILD_ITERATOR_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_CHILD_ITERATOR_BASE_H_

#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/child_iterator.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) ChildIteratorBase : public ChildIterator {
 public:
  ChildIteratorBase(const AXPlatformNodeDelegate* parent, size_t index);
  ChildIteratorBase(const ChildIteratorBase& it);
  ~ChildIteratorBase() override = default;
  ChildIteratorBase& operator++() override;
  // Postfix increment/decrement can't be overrides. See comment in
  // child_iterator.h
  ChildIteratorBase operator++(int);
  ChildIteratorBase& operator--() override;
  ChildIteratorBase operator--(int);
  gfx::NativeViewAccessible GetNativeViewAccessible() const override;
  std::optional<size_t> GetIndexInParent() const override;
  AXPlatformNodeDelegate* get() const override;
  AXPlatformNodeDelegate& operator*() const override;
  AXPlatformNodeDelegate* operator->() const override;

 private:
  raw_ptr<const AXPlatformNodeDelegate> parent_;
  size_t index_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_CHILD_ITERATOR_BASE_H_
