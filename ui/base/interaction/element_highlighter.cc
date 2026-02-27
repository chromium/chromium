// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_highlighter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace ui {

ElementHighlighter::~ElementHighlighter() = default;

// static
ElementHighlighter* ElementHighlighter::GetElementHighlighter() {
  static base::NoDestructor<ElementHighlighter> instance;
  return instance.get();
}

// static
std::unique_ptr<ElementHighlighter>
ElementHighlighter::MakeInstanceForTesting() {
  return base::WrapUnique(new ElementHighlighter());
}

bool ElementHighlighter::CanBeHighlighted(TrackedElement* element) const {
  if (!element) {
    return false;
  }

  for (const auto& backend : backends_) {
    if (backend.CanBeHighlighted(*element)) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<ElementHighlighter::Highlight> ElementHighlighter::AddHighlight(
    TrackedElement* element) {
  if (!element) {
    return nullptr;
  }

  for (auto& backend : backends_) {
    if (std::unique_ptr<Highlight> highlight = backend.AddHighlight(*element)) {
      return highlight;
    }
  }
  return nullptr;
}

ElementHighlighter::ElementHighlighter() = default;

}  // namespace ui
