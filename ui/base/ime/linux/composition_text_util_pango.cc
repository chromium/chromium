// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/linux/composition_text_util_pango.h"

#include <pango/pango-attributes.h>
#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/i18n/char_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"

namespace ui {

void ExtractCompositionTextFromGtkPreedit(const char* utf8_text,
                                          PangoAttrList* attrs,
                                          int cursor_position,
                                          CompositionText* composition) {
  *composition = CompositionText();
  composition->text = base::UTF8ToUTF16(utf8_text);

  if (composition->text.empty())
    return;

  // Gtk/Pango uses character index for cursor position and byte index for
  // attribute range, but we use char16 offset for them. So we need to do
  // conversion here.
  std::vector<size_t> char16_offsets;
  size_t length = composition->text.length();
  for (base::i18n::UTF16CharIterator char_iterator(composition->text);
       !char_iterator.end(); char_iterator.Advance()) {
    char16_offsets.push_back(char_iterator.array_pos());
  }

  // The text length in Unicode characters.
  int char_length = static_cast<int>(char16_offsets.size());
  // Make sure we can convert the value of |char_length| as well.
  char16_offsets.push_back(length);

  size_t cursor_offset =
      char16_offsets[std::clamp(cursor_position, 0, char_length)];

  composition->selection = gfx::Range(cursor_offset);

  if (attrs) {
    int utf8_length = strlen(utf8_text);
    PangoAttrIterator* iter = pango_attr_list_get_iterator(attrs);

    // We only care about underline and background attributes and convert
    // background attribute into selection if possible.
    do {
      gint start, end;
      pango_attr_iterator_range(iter, &start, &end);

      start = std::min(start, utf8_length);
      end = std::min(end, utf8_length);
      if (start >= end)
        continue;

      start = g_utf8_pointer_to_offset(utf8_text, utf8_text + start);
      end = g_utf8_pointer_to_offset(utf8_text, utf8_text + end);

      // Double check, in case |utf8_text| is not a valid utf-8 string.
      start = std::min(start, char_length);
      end = std::min(end, char_length);
      if (start >= end)
        continue;

      PangoAttribute* background_attr =
          pango_attr_iterator_get(iter, PANGO_ATTR_BACKGROUND);
      PangoAttribute* underline_attr =
          pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);

      if (background_attr || underline_attr) {
        // Use a thin underline with text color by default.
        ImeTextSpan ime_text_span(
            ImeTextSpan::Type::kComposition, char16_offsets[start],
            char16_offsets[end], ImeTextSpan::Thickness::kThin,
            ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT);

        // Always use thick underline for a range with background color, which
        // is usually the selection range.
        if (background_attr) {
          ime_text_span.thickness = ImeTextSpan::Thickness::kThick;
          // If the cursor is at start or end of this underline, then we treat
          // it as the selection range as well, but make sure to set the cursor
          // position to the selection end.
          if (ime_text_span.start_offset == cursor_offset) {
            composition->selection.set_start(ime_text_span.end_offset);
            composition->selection.set_end(cursor_offset);
          } else if (ime_text_span.end_offset == cursor_offset) {
            composition->selection.set_start(ime_text_span.start_offset);
            composition->selection.set_end(cursor_offset);
          }
        }
        if (underline_attr) {
          int type = reinterpret_cast<PangoAttrInt*>(underline_attr)->value;
          if (type == PANGO_UNDERLINE_DOUBLE)
            ime_text_span.thickness = ImeTextSpan::Thickness::kThick;
          else if (type == PANGO_UNDERLINE_ERROR)
            ime_text_span.underline_color = SK_ColorRED;
        }
        composition->ime_text_spans.push_back(ime_text_span);
      }
    } while (pango_attr_iterator_next(iter));
    pango_attr_iterator_destroy(iter);
  }

  // Use a thin underline with text color by default.
  if (composition->ime_text_spans.empty()) {
    composition->ime_text_spans.push_back(
        ImeTextSpan(ImeTextSpan::Type::kComposition, 0, length,
                    ImeTextSpan::Thickness::kThin,
                    ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT));
  }
}

}  // namespace ui
