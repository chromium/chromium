// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "build/build_config.h"

#if !BUILDFLAG(IS_MAC)
// #include <stddef.h>    //// used for size_t  but it is already available in <string_view> and <memory>

#include <memory>
#include <string_view>

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/utf16_indexing.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#endif
#endif

namespace views {

#if !BUILDFLAG(IS_MAC)

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(
    ScrollBar::Orientation orientation) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  return std::make_unique<OverlayScrollBar>(orientation);
#else
  return std::make_unique<ScrollBarViews>(orientation);
#endif
}

// static
void PlatformStyle::OnTextfieldEditFailed() {
    // TODO: Provide visual or audio feedback for text edit failure.
}

// static
gfx::Range PlatformStyle::RangeToDeleteBackwards(std::u16string_view text,
                                                 size_t cursor_position) {
  // Delete one grapheme cluster , which may span multiple UTF-16 code units.
    if (cursor_position == 0) // prevent crash when cursor is at position 0
        return gfx::Range(0, 0);
    
        size_t previous_grapheme_index =
      gfx::UTF16OffsetToIndex(text, cursor_position, -1);
  return gfx::Range( previous_grapheme_index , cursor_position); //Range ( start , end) correct order
}

#endif  // !BUILDFLAG(IS_MAC)

}  // namespace views
