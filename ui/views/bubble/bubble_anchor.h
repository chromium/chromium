// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_ANCHOR_H_
#define UI_VIEWS_BUBBLE_BUBBLE_ANCHOR_H_

#include <variant>

#include "base/memory/raw_ptr.h"
#include "ui/views/views_export.h"

namespace ui {
class TrackedElement;
}  // namespace ui

namespace gfx {
class Rect;
}

namespace views {

class View;

// A bubble can be anchored to a view, a tracked element, or nothing.
// BubbleAnchor is a variant type that can hold any of these.
//
// A tracked element is useful when the element could be either a View or a HTML
// element in a WebUI. The element can be retrieved using its ElementIdentifier,
// example:
//
//   #include "ui/base/interaction/element_tracker.h"
//   ui::TrackedElement* element = ui::ElementTracker::GetElementTracker()
//       ->GetElementInAnyContext(kElementId);
//   auto bubble_delegate = std::make_unique<BubbleDialogDelegate>(
//       views::BubbleAnchor(element), BubbleBorder::Arrow::TOP_LEFT);
//   views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
//   ...
//
// WARNING: Do not store these long-term, as underlying TrackedElement's
// (and Views) may get destroyed.
class VIEWS_EXPORT BubbleAnchor {
 public:
  BubbleAnchor();
  explicit BubbleAnchor(View* view);
  explicit BubbleAnchor(ui::TrackedElement* element);
  BubbleAnchor(const BubbleAnchor&);
  ~BubbleAnchor();

  BubbleAnchor& operator=(const BubbleAnchor&);

  bool IsNull() const {
    return std::holds_alternative<std::nullptr_t>(anchor_);
  }

  View* GetIfView() {
    if (auto* contents_ptr = std::get_if<raw_ptr<View>>(&anchor_)) {
      return *contents_ptr;
    }
    return nullptr;
  }

  const View* GetIfView() const {
    if (const raw_ptr<View>* contents_ptr =
            std::get_if<raw_ptr<View>>(&anchor_)) {
      return *contents_ptr;
    }
    return nullptr;
  }

  ui::TrackedElement* GetIfElement() {
    if (auto* contents_ptr =
            std::get_if<raw_ptr<ui::TrackedElement>>(&anchor_)) {
      return *contents_ptr;
    }
    return nullptr;
  }

  const ui::TrackedElement* GetIfElement() const {
    if (const raw_ptr<ui::TrackedElement>* contents_ptr =
            std::get_if<raw_ptr<ui::TrackedElement>>(&anchor_)) {
      return *contents_ptr;
    }
    return nullptr;
  }

  // Returns the anchor bounds in screen coordinates.
  gfx::Rect GetAnchorRect() const;

 private:
  std::variant<std::nullptr_t, raw_ptr<View>, raw_ptr<ui::TrackedElement>>
      anchor_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_ANCHOR_H_
