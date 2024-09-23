// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_VIEW_AURA_H_
#define UI_VIEWS_COREWM_TOOLTIP_VIEW_AURA_H_

#include <memory>
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/render_text.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views::corewm {

// The contents view for tooltip widget on aura platforms.

// TODO(oshima): Consider to use views::Label when the performance issue is
// resolved.
class VIEWS_EXPORT TooltipViewAura : public views::View {
  METADATA_HEADER(TooltipViewAura, views::View)

 public:
  TooltipViewAura();
  TooltipViewAura(const TooltipViewAura&) = delete;
  TooltipViewAura& operator=(const TooltipViewAura&) = delete;
  ~TooltipViewAura() override;

  const gfx::RenderText* render_text() const { return render_text_.get(); }

  void SetText(const std::u16string& text);
  void SetFontList(const gfx::FontList& font_list);
  void SetMinLineHeight(int line_height);
  void SetMaxWidth(int width);
  void SetMaxLines(size_t max_lines);
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // views:View:
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnThemeChanged() override;

 private:
  void UpdateAccessibleName();
  void ResetDisplayRect();

  std::unique_ptr<gfx::RenderText> render_text_;
  int max_width_ = 0;
};

}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TOOLTIP_VIEW_AURA_H_
