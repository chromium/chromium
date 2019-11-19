// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_CAPTION_STYLE_H_
#define UI_NATIVE_THEME_CAPTION_STYLE_H_

#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/native_theme/native_theme_export.h"

#include <string>

namespace ui {

struct NATIVE_THEME_EXPORT CaptionStyle {
  CaptionStyle();
  CaptionStyle(const CaptionStyle& other);
  ~CaptionStyle();

  // Returns a CaptionStyle parsed from a specification string, which is a
  // serialized JSON object whose keys are strings and whose values are of
  // variable types. See the body of this method for details. This is used to
  // parse the value of the "--force-caption-style" command-line argument and
  // for testing.
  static base::Optional<CaptionStyle> FromSpec(const std::string& spec);

  // Returns a CaptionStyle populated from the System's Settings.
  static base::Optional<CaptionStyle> FromSystemSettings();

  // Some or all of these property strings can be empty.
  // For example, on Win10 in Settings when a property is set to Default, the
  // corresponding string here stays empty. This allows the author styling on
  // the webpage to be applied. As the user hasn't specified a preferred style,
  // we pass along an empty string from here.
  std::string text_color;
  std::string background_color;
  // Holds text size percentage as a css string.
  std::string text_size;
  std::string text_shadow;
  std::string font_family;
  std::string font_variant;
  std::string window_color;
  std::string window_padding;
  std::string window_radius;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_CAPTION_STYLE_H_
