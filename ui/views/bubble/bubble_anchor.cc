// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_anchor.h"

#include <variant>

namespace views {

BubbleAnchor::BubbleAnchor() = default;
BubbleAnchor::BubbleAnchor(View* view) {
  if (view) {
    anchor_ = view;
  }
}

BubbleAnchor::BubbleAnchor(ui::TrackedElement* element) {
  if (element) {
    anchor_ = element;
  }
}

BubbleAnchor::BubbleAnchor(const BubbleAnchor&) = default;

BubbleAnchor::~BubbleAnchor() = default;

BubbleAnchor& BubbleAnchor::operator=(const BubbleAnchor&) = default;

}  // namespace views
