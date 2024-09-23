// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEWS_UTILITIES_AURA_H_
#define UI_VIEWS_ACCESSIBILITY_VIEWS_UTILITIES_AURA_H_

#include <string>
#include <vector>

#include "ui/views/buildflags.h"

namespace aura {
class Window;
}

namespace gfx {
class RenderText;
}

namespace views {

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
struct WordBoundaries {
  WordBoundaries();
  WordBoundaries(const WordBoundaries&);
  ~WordBoundaries();

  std::vector<int32_t> starts;
  std::vector<int32_t> ends;
};
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

// Return the parent of `window`, first checking to see if it has a
// transient parent. This allows us to walk up the aura::Window
// hierarchy when it spans multiple window tree hosts, each with
// their own native window.
aura::Window* GetWindowParentIncludingTransient(aura::Window* window);

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
// Returns the start and end offsets of each word in `text`.
WordBoundaries ComputeWordBoundaries(const std::u16string& text);

// Retrieves a vector of the rounded starting offsets of each char in all
// runs. With 3 chars of width 10, the values would be 0, 10, 20, 30.
// Implementation only supports single line for now.
// TODO(crbug.com/40933356): Support multiline.
std::vector<int32_t> ComputeTextOffsets(gfx::RenderText* render_text);
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEWS_UTILITIES_AURA_H_
