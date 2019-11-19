// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"

namespace ui {

CaptionStyle::CaptionStyle() = default;
CaptionStyle::CaptionStyle(const CaptionStyle& other) = default;
CaptionStyle::~CaptionStyle() = default;

// static
base::Optional<CaptionStyle> CaptionStyle::FromSpec(const std::string& spec) {
  CaptionStyle style;
  base::Optional<base::Value> dict = base::JSONReader::Read(spec);

  if (!dict.has_value() || !dict->is_dict())
    return base::nullopt;

  if (const std::string* value = dict->FindStringKey("text-color"))
    style.text_color = *value;
  if (const std::string* value = dict->FindStringKey("background-color"))
    style.background_color = *value;

  return style;
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
base::Optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  return base::nullopt;
}
#endif

}  // namespace ui
