// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEXT_UTILS_H_
#define UI_GFX_TEXT_UTILS_H_

#include <stddef.h>

#include <string>

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/text_constants.h"

namespace gfx {

class FontList;
class Insets;
class Size;

// Strips the accelerator char ('&') from a menu string. Useful for platforms
// which use underlining to indicate accelerators.
//
// Single accelerator chars ('&') will be stripped from the string. Double
// accelerator chars ('&&') will be converted to a single '&'. The out params
// |accelerated_char_pos| and |accelerated_char_span| will be set to the index
// and span of the last accelerated character, respectively, or -1 and 0 if
// there was none.
GFX_EXPORT std::u16string LocateAndRemoveAcceleratorChar(
    const std::u16string& s,
    int* accelerated_char_pos,
    int* accelerated_char_span);

// Strips all accelerator notation from a menu string. Useful for platforms
// which use underlining to indicate accelerators, as well as situations where
// accelerators are not indicated.
//
// Single accelerator chars ('&') will be stripped from the string. Double
// accelerator chars ('&&') will be converted to a single '&'. CJK language
// accelerators, specified as "(&x)", will be entirely removed too.
GFX_EXPORT std::u16string RemoveAccelerator(const std::u16string& s);

// Returns the number of horizontal pixels needed to display the specified
// |text| with |font_list|. |typesetter| indicates where the text will be
// displayed.
GFX_EXPORT int GetStringWidth(const std::u16string& text,
                              const FontList& font_list);

// Returns the size required to render |text| in |font_list|. This includes all
// leading space, descender area, etc. even if the text to render does not
// contain characters with ascenders or descenders.
GFX_EXPORT Size GetStringSize(const std::u16string& text,
                              const FontList& font_list);

// This is same as GetStringWidth except that fractional width is returned.
GFX_EXPORT float GetStringWidthF(const std::u16string& text,
                                 const FontList& font_list);

// Returns a valid cut boundary at or before |index|. The surrogate pair and
// combining characters should not be separated.
GFX_EXPORT size_t FindValidBoundaryBefore(const std::u16string& text,
                                          size_t index,
                                          bool trim_whitespace = false);

// Returns a valid cut boundary at or after |index|. The surrogate pair and
// combining characters should not be separated.
GFX_EXPORT size_t FindValidBoundaryAfter(const std::u16string& text,
                                         size_t index,
                                         bool trim_whitespace = false);

// If the UI layout is right-to-left, flip the alignment direction.
GFX_EXPORT HorizontalAlignment MaybeFlipForRTL(HorizontalAlignment alignment);

// Returns insets that can be used to draw a highlight or border that appears to
// be distance |desired_visual_padding| from the body of a string of text
// rendered using |font_list|. The insets are adjusted based on the box used to
// render capital letters (or the bodies of most letters in non-capital fonts
// like Hebrew and Devanagari), in order to give the best visual appearance.
//
// That is, any portion of |desired_visual_padding| overlapping the font's
// leading space or descender area are truncated, to a minimum of zero.
//
// In this example, the text is rendered in a highlight that stretches above and
// below the height of the H as well as to the left and right of the text
// (|desired_visual_padding| = {2, 2, 2, 2}). Note that the descender of the 'y'
// overlaps with the padding, as it is outside the capital letter box.
//
// The resulting padding is {1, 2, 1, 2}.
//
//  . . . . . . . . . .                               | actual top
//  .                 .  |              | leading space
//  .  |  |  _        .  | font    | capital
//  .  |--| /_\ \  /  .  | height  | height
//  .  |  | \_   \/   .  |         |
//  .            /    .  |              | descender
//  . . . . . . . . . .                               | actual bottom
//  ___             ___
//  actual        actual
//  left           right
//
GFX_EXPORT Insets
AdjustVisualBorderForFont(const FontList& font_list,
                          const Insets& desired_visual_padding);

// Returns the y adjustment necessary to align the center of the "cap size" box
// - the space between a capital letter's top and bottom - between two fonts.
// For non-capital scripts (e.g. Hebrew, Devanagari) the box containing the body
// of most letters is used.
//
// A positive return value means the font |to_center| needs to be moved down
// relative to the font |original_font|, while a negative value means it needs
// to be moved up.
//
// Illustration:
//
//  original_font    to_center
//  ----------                    ] - return value (+1)
//  leading          ----------
//  ----------       leading
//                   ----------
//
//  cap-height       cap-height
//
//                   ----------
//  ----------       descent
//  descent          ----------
//  ----------
//
// Visual result:           Non-Latin example (Devanagari ऐ "ai"):
//                               \
//  |\   |                    ------     \
//  | \  |   |\ |              |  |    ----
//  |  \ |   | \|               \ /     \|
//  |   \|                       \       /
//                                /
//
GFX_EXPORT int GetFontCapHeightCenterOffset(const gfx::FontList& original_font,
                                            const gfx::FontList& to_center);

}  // namespace gfx

#endif  // UI_GFX_TEXT_UTILS_H_
