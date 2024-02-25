// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

namespace gfx {

namespace {

// Strips accelerator character prefixes in |text| if needed, based on |flags|.
// Returns a range in |text| to underline or Range::InvalidRange() if
// underlining is not needed.
Range StripAcceleratorChars(int flags, std::u16string* text) {
  if (flags & Canvas::SHOW_PREFIX) {
    int char_pos = -1;
    int char_span = 0;
    *text = LocateAndRemoveAcceleratorChar(*text, &char_pos, &char_span);
    if (char_pos != -1)
      return Range(char_pos, char_pos + char_span);
  } else if (flags & Canvas::HIDE_PREFIX) {
    *text = RemoveAccelerator(*text);
  }

  return Range::InvalidRange();
}

// Elides |text| and adjusts |range| appropriately. If eliding causes |range|
// to no longer point to the same character in |text|, |range| is made invalid.
void ElideTextAndAdjustRange(const FontList& font_list,
                             float width,
                             std::u16string* text,
                             Range* range) {
  const char16_t start_char =
      (range->IsValid() ? text->at(range->start()) : u'\0');
  *text = ElideText(*text, font_list, width, ELIDE_TAIL);
  if (!range->IsValid())
    return;
  if (range->start() >= text->length() ||
      text->at(range->start()) != start_char) {
    *range = Range::InvalidRange();
  }
}

// Updates |render_text| from the specified parameters.
void UpdateRenderText(const Rect& rect,
                      const std::u16string& text,
                      const FontList& font_list,
                      int flags,
                      SkColor color,
                      RenderText* render_text) {
  render_text->SetFontList(font_list);
  render_text->SetText(text);
  render_text->SetCursorEnabled(false);
  render_text->SetDisplayRect(rect);

  // Set the text alignment explicitly based on the directionality of the UI,
  // if not specified.
  if (!(flags & (Canvas::TEXT_ALIGN_CENTER |
                 Canvas::TEXT_ALIGN_RIGHT |
                 Canvas::TEXT_ALIGN_LEFT |
                 Canvas::TEXT_ALIGN_TO_HEAD))) {
    flags |= Canvas::DefaultCanvasTextAlignment();
  }

  if (flags & Canvas::TEXT_ALIGN_TO_HEAD)
    render_text->SetHorizontalAlignment(ALIGN_TO_HEAD);
  else if (flags & Canvas::TEXT_ALIGN_RIGHT)
    render_text->SetHorizontalAlignment(ALIGN_RIGHT);
  else if (flags & Canvas::TEXT_ALIGN_CENTER)
    render_text->SetHorizontalAlignment(ALIGN_CENTER);
  else
    render_text->SetHorizontalAlignment(ALIGN_LEFT);

  render_text->set_subpixel_rendering_suppressed(
      (flags & Canvas::NO_SUBPIXEL_RENDERING) != 0);

  render_text->SetColor(color);
  const int font_style = font_list.GetFontStyle();
  render_text->SetStyle(TEXT_STYLE_ITALIC, (font_style & Font::ITALIC) != 0);
  render_text->SetStyle(TEXT_STYLE_UNDERLINE,
                        (font_style & Font::UNDERLINE) != 0);
  render_text->SetStyle(TEXT_STYLE_STRIKE,
                        (font_style & Font::STRIKE_THROUGH) != 0);
  render_text->SetWeight(font_list.GetFontWeight());
}

}  // namespace

// static
void Canvas::SizeStringFloat(const std::u16string& text,
                             const FontList& font_list,
                             float* width,
                             float* height,
                             int line_height,
                             int flags) {
  DCHECK_GE(*width, 0);
  DCHECK_GE(*height, 0);

  if ((flags & MULTI_LINE) && *width != 0) {
    WordWrapBehavior wrap_behavior = TRUNCATE_LONG_WORDS;
    if (flags & CHARACTER_BREAKABLE)
      wrap_behavior = WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ELIDE_LONG_WORDS;

    std::vector<std::u16string> strings;
    ElideRectangleText(text, font_list, *width, INT_MAX, wrap_behavior,
                       &strings);
    Rect rect(base::saturated_cast<int>(*width), INT_MAX);

    std::unique_ptr<RenderText> render_text = RenderText::CreateRenderText();

    UpdateRenderText(rect, std::u16string(), font_list, flags, 0,
                     render_text.get());

    float h = 0;
    float w = 0;
    for (size_t i = 0; i < strings.size(); ++i) {
      StripAcceleratorChars(flags, &strings[i]);
      render_text->SetText(std::move(strings[i]));
      const SizeF& string_size = render_text->GetStringSizeF();
      w = std::max(w, string_size.width());
      h += (i > 0 && line_height > 0) ?
               std::max(static_cast<float>(line_height), string_size.height())
                   : string_size.height();
    }
    *width = w;
    *height = h;
  } else {
    std::unique_ptr<RenderText> render_text = RenderText::CreateRenderText();

    Rect rect(base::saturated_cast<int>(*width),
              base::saturated_cast<int>(*height));
    std::u16string adjusted_text = text;
    StripAcceleratorChars(flags, &adjusted_text);
    UpdateRenderText(rect, adjusted_text, font_list, flags, 0,
                     render_text.get());
    const SizeF& string_size = render_text->GetStringSizeF();
    *width = string_size.width();
    *height = string_size.height();
  }
}

void Canvas::DrawStringRectWithFlags(const std::u16string& text,
                                     const FontList& font_list,
                                     SkColor color,
                                     const Rect& text_bounds,
                                     int flags) {
  if (!IntersectsClipRect(RectToSkRect(text_bounds)))
    return;

  canvas_->save();

  gfx::RectF clip_rect(text_bounds);

  // Pixels on the border of `text_bounds` will get clipped if the
  // border is not pixel-aligned. This can only happen when the canvas
  // is scaled. Expand the clip rect by 0.5 dip to fix that.
  // See crbug.com/1469229.
  if (std::abs(std::trunc(image_scale()) - image_scale()) > 1e-5f) {
    clip_rect.Outset(0.5f);
  }
  ClipRect(clip_rect);
  Rect rect(text_bounds);

  std::unique_ptr<RenderText> render_text = RenderText::CreateRenderText();
  render_text->set_clip_to_display_rect(false);

  if (flags & MULTI_LINE) {
    WordWrapBehavior wrap_behavior = IGNORE_LONG_WORDS;
    if (flags & CHARACTER_BREAKABLE)
      wrap_behavior = WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ELIDE_LONG_WORDS;

    std::vector<std::u16string> strings;
    ElideRectangleText(text, font_list,
                       static_cast<float>(text_bounds.width()),
                       text_bounds.height(), wrap_behavior, &strings);

    for (size_t i = 0; i < strings.size(); i++) {
      Range range = StripAcceleratorChars(flags, &strings[i]);
      UpdateRenderText(rect, strings[i], font_list, flags, color,
                       render_text.get());
      int line_padding = 0;
      const int line_height = render_text->GetStringSize().height();

      rect.set_height(line_height - line_padding);

      if (range.IsValid())
        render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, range);
      render_text->SetDisplayRect(rect);
      render_text->Draw(this);
      rect += Vector2d(0, line_height);
    }
  } else {
    std::u16string adjusted_text = text;
    Range range = StripAcceleratorChars(flags, &adjusted_text);
    bool elide_text = ((flags & NO_ELLIPSIS) == 0);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // On Linux, eliding really means fading the end of the string. But only
    // for LTR text. RTL text is still elided (on the left) with "...".
    if (elide_text) {
      render_text->SetText(adjusted_text);
      if (render_text->GetDisplayTextDirection() == base::i18n::LEFT_TO_RIGHT) {
        render_text->SetElideBehavior(FADE_TAIL);
        elide_text = false;
      }
    }
#endif

    if (elide_text) {
      ElideTextAndAdjustRange(font_list,
                              static_cast<float>(text_bounds.width()),
                              &adjusted_text, &range);
    }

    UpdateRenderText(rect, adjusted_text, font_list, flags, color,
                     render_text.get());
    if (range.IsValid())
      render_text->ApplyStyle(TEXT_STYLE_UNDERLINE, true, range);
    render_text->Draw(this);
  }

  canvas_->restore();
}

}  // namespace gfx
