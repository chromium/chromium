// Copyright 2018 The Chromium Authors
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
absl::optional<CaptionStyle> CaptionStyle::FromSpec(const std::string& spec) {
  CaptionStyle style;
  absl::optional<base::Value> dict = base::JSONReader::Read(spec);
  if (!dict.has_value()) {
    return absl::nullopt;
  }

  base::Value::Dict* value_dict = dict->GetIfDict();
  if (!value_dict) {
    return absl::nullopt;
  }

  if (const std::string* value = value_dict->FindString("text-color")) {
    style.text_color = *value;
  }
  if (const std::string* value = value_dict->FindString("background-color")) {
    style.background_color = *value;
  }

  return style;
}

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
absl::optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  return absl::nullopt;
}
#endif

}  // namespace ui
