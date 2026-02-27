// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_ELEMENT_HIGHLIGHTER_VIEWS_H_
#define UI_VIEWS_INTERACTION_ELEMENT_HIGHLIGHTER_VIEWS_H_

#include <memory>

#include "ui/base/interaction/element_highlighter.h"
#include "ui/views/views_export.h"

namespace views {

// Helps highlight views via a TrackedElement
class VIEWS_EXPORT ElementHighlighterViews
    : public ui::ElementHighlighter::Backend {
 public:
  ElementHighlighterViews();
  ~ElementHighlighterViews() override;

  // ui::ElementHighlighter::Backend implementation:
  bool CanBeHighlighted(ui::TrackedElement& element) const override;
  std::unique_ptr<ui::ElementHighlighter::Highlight> AddHighlight(
      ui::TrackedElement& element) override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

}  // namespace views

#endif  // UI_VIEWS_INTERACTION_ELEMENT_HIGHLIGHTER_VIEWS_H_
