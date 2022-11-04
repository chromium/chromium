// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography.h"

#include "base/check_op.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"

namespace views::style {
namespace {

void ValidateContextAndStyle(int context, int style) {
  DCHECK_GE(context, VIEWS_TEXT_CONTEXT_START);
  DCHECK_LT(context, TEXT_CONTEXT_MAX);
  DCHECK_GE(style, VIEWS_TEXT_STYLE_START);
}

}  // namespace

ui::ResourceBundle::FontDetails GetFontDetails(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetFontDetails(context,
                                                                       style);
}

const gfx::FontList& GetFont(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetFont(context, style);
}

SkColor GetColor(const views::View& view, int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetColor(view, context,
                                                                 style);
}

int GetLineHeight(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetLineHeight(context,
                                                                      style);
}

}  // namespace views::style
