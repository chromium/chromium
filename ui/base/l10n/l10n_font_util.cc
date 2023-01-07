// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_font_util.h"

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"

namespace ui {

int GetLocalizedContentsWidthForFontList(int col_resource_id,
                                         const gfx::FontList& font_list) {
  int chars = 0;
  base::StringToInt(l10n_util::GetStringUTF8(col_resource_id), &chars);
  int width = font_list.GetExpectedTextWidth(chars);
  DCHECK_GT(width, 0);
  return width;
}

int GetLocalizedContentsHeightForFontList(int row_resource_id,
                                          const gfx::FontList& font_list) {
  int lines = 0;
  base::StringToInt(l10n_util::GetStringUTF8(row_resource_id), &lines);
  int height = font_list.GetHeight() * lines;
  DCHECK_GT(height, 0);
  return height;
}

}  // namespace ui
