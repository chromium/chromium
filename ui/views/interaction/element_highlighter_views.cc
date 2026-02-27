// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_highlighter_views.h"

#include <memory>
#include <utility>

#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace views {

namespace {

class ButtonHighlight : public ui::ElementHighlighter::Highlight {
 public:
  explicit ButtonHighlight(Button::ScopedAnchorHighlight highlight)
      : highlight_(std::move(highlight)) {}
  ~ButtonHighlight() override = default;

 private:
  Button::ScopedAnchorHighlight highlight_;
};

}  // namespace

ElementHighlighterViews::ElementHighlighterViews() = default;
ElementHighlighterViews::~ElementHighlighterViews() = default;

bool ElementHighlighterViews::CanBeHighlighted(
    ui::TrackedElement& element) const {
  if (auto* views_element = element.AsA<TrackedElementViews>()) {
    return Button::AsButton(views_element->view());
  }
  return false;
}

std::unique_ptr<ui::ElementHighlighter::Highlight>
ElementHighlighterViews::AddHighlight(ui::TrackedElement& element) {
  auto* views_element = element.AsA<TrackedElementViews>();
  if (!views_element) {
    return nullptr;
  }

  Button* button = Button::AsButton(views_element->view());
  if (!button) {
    return nullptr;
  }

  return std::make_unique<ButtonHighlight>(button->AddAnchorHighlight());
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(ElementHighlighterViews)

}  // namespace views
