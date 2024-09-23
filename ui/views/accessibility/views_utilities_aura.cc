// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/views_utilities_aura.h"

#include <algorithm>

#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/selection_model.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace views {

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
WordBoundaries::WordBoundaries() = default;
WordBoundaries::WordBoundaries(const WordBoundaries& other) = default;
WordBoundaries::~WordBoundaries() = default;
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

aura::Window* GetWindowParentIncludingTransient(aura::Window* window) {
  aura::Window* transient_parent = wm::GetTransientParent(window);
  if (transient_parent)
    return transient_parent;

  return window->parent();
}

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
WordBoundaries ComputeWordBoundaries(const std::u16string& text) {
  WordBoundaries boundaries;

  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return boundaries;
  }

  while (iter.Advance()) {
    if (iter.IsWord()) {
      boundaries.starts.push_back(static_cast<int32_t>(iter.prev()));
      boundaries.ends.push_back(static_cast<int32_t>(iter.pos()));
    }
  }

  return boundaries;
}

std::vector<int32_t> ComputeTextOffsets(gfx::RenderText* render_text) {
  std::vector<int32_t> offsets;
  // TODO(crbug.com/40132003): Allow elided text once the support for
  // elided text in `RenderText::GetLookupDataForRange` is completed.
  // TODO(crbug.com/40933356): Add support for multiline textfields.
  if (!render_text || render_text->multiline() ||
      render_text->elide_behavior() == gfx::ELIDE_MIDDLE ||
      render_text->elide_behavior() == gfx::ELIDE_HEAD ||
      render_text->elide_behavior() == gfx::ELIDE_EMAIL) {
    return offsets;
  }

  // TODO(crbug.com/40946445): Add a maximum length check to avoid hangs.
  size_t begin_position = 0;
  int last_x = 0;

  // Subtract the display offset to get the offsets relative to the origin. The
  // display offset will be applied later, in `ViewAXPlatformNodeDelegate`.
  int offset = render_text->GetUpdatedDisplayOffset().x();

  while (begin_position < render_text->text().length()) {
    size_t end_position = render_text->IndexOfAdjacentGrapheme(
        begin_position, gfx::CURSOR_FORWARD);
    gfx::Range range(begin_position, end_position);

    gfx::Rect bounds;
    gfx::DecoratedText decorated_text;
    render_text->GetLookupDataForRange(range, &decorated_text, &bounds);

    if (bounds.IsEmpty()) {
      // This is a fallback for when the text is elided or truncated. We must
      // always have a valid offset for each grapheme in the text backing the
      // display text.
      offsets.push_back(last_x);
    } else {
      offsets.push_back(bounds.x() - offset);
      last_x = bounds.right() - offset;
    }

    begin_position = end_position;
  }

  // Return an empty vector if we were unable to find the start of any glyph, or
  // if the text was empty.
  if (offsets.empty()) {
    return offsets;
  }

  // Add the very last offset at the end of the vector so that we know where the
  // last glyph ends.
  offsets.push_back(last_x);
  return offsets;
}
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

}  // namespace views
