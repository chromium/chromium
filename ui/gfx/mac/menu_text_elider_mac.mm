// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/menu_text_elider_mac.h"

#include "base/no_destructor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/gfx/text_elider.h"

namespace gfx {

namespace {

// Returns the FontList for the system menu font. Cached for performance.
const FontList& GetMenuFontList() {
  static const base::NoDestructor<FontList> menu_font_list(
      Font(new PlatformFontMac(PlatformFontMac::SystemFontType::kMenu)));
  return *menu_font_list;
}

}  // namespace

std::u16string ElideMenuItemTitle(const std::u16string& title,
                                  float max_width) {
  return ElideText(title, GetMenuFontList(), max_width, ELIDE_MIDDLE);
}

std::u16string ElideMenuItemTitle(const std::u16string& title) {
  return ElideMenuItemTitle(title, kDefaultMenuItemTitleMaxWidth);
}

}  // namespace gfx
