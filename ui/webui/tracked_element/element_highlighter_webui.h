// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_TRACKED_ELEMENT_ELEMENT_HIGHLIGHTER_WEBUI_H_
#define UI_WEBUI_TRACKED_ELEMENT_ELEMENT_HIGHLIGHTER_WEBUI_H_

#include <memory>

#include "ui/base/interaction/element_highlighter.h"

namespace ui {

// Helps highlight WebUI element that are tracked via a mojo connection managed
// by TrackedElementHandler. TrackedElementHandler installs this.
class ElementHighlighterWebUI : public ui::ElementHighlighter::Backend {
 public:
  ElementHighlighterWebUI();
  ~ElementHighlighterWebUI() override;

  // ui::ElementHighlighter::Backend implementation:
  bool CanBeHighlighted(ui::TrackedElement& element) const override;
  std::unique_ptr<ui::ElementHighlighter::Highlight> AddHighlight(
      ui::TrackedElement& element) override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

 private:
  class Highlight;
};

}  // namespace ui

#endif  // UI_WEBUI_TRACKED_ELEMENT_ELEMENT_HIGHLIGHTER_WEBUI_H_
