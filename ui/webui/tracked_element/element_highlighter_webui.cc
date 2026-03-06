// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/element_highlighter_webui.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "ui/base/interaction/element_highlighter.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace ui {

class ElementHighlighterWebUI::Highlight
    : public ElementHighlighter::Highlight {
 public:
  explicit Highlight(scoped_refptr<TrackedElementWebUI::HighlightHandle> handle)
      : handle_(std::move(handle)) {}
  ~Highlight() override = default;

 private:
  scoped_refptr<TrackedElementWebUI::HighlightHandle> handle_;
};

ElementHighlighterWebUI::ElementHighlighterWebUI() = default;
ElementHighlighterWebUI::~ElementHighlighterWebUI() = default;

bool ElementHighlighterWebUI::CanBeHighlighted(
    ui::TrackedElement& element) const {
  auto* webui_element = element.AsA<TrackedElementWebUI>();
  if (!webui_element) {
    return false;
  }

  return webui_element->can_highlight();
}

std::unique_ptr<ui::ElementHighlighter::Highlight>
ElementHighlighterWebUI::AddHighlight(ui::TrackedElement& element) {
  auto* webui_element = element.AsA<TrackedElementWebUI>();
  if (!webui_element) {
    return nullptr;
  }

  if (!webui_element->can_highlight()) {
    return nullptr;
  }

  return std::make_unique<Highlight>(webui_element->GetOrMakeHighlightHandle());
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(ElementHighlighterWebUI)

}  // namespace ui
