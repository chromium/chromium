// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_FONT_UTIL_H_
#define UI_BASE_L10N_L10N_FONT_UTIL_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class FontList;
}

namespace ui {

// Returns the preferred size of the contents view of a window based on
// its localized size data and the given font. The width in cols is held in a
// localized string resource identified by |col_resource_id|, the height in the
// same fashion.
COMPONENT_EXPORT(UI_BASE)
int GetLocalizedContentsWidthForFontList(int col_resource_id,
                                         const gfx::FontList& font_list);
COMPONENT_EXPORT(UI_BASE)
int GetLocalizedContentsHeightForFontList(int row_resource_id,
                                          const gfx::FontList& font_list);

}  // namespace ui

#endif  // UI_BASE_L10N_L10N_FONT_UTIL_H_
