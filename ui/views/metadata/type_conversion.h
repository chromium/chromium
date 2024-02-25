// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_TYPE_CONVERSION_H_
#define UI_VIEWS_METADATA_TYPE_CONVERSION_H_

#include <optional>

#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/views_export.h"
#include "url/gurl.h"

template <>
struct VIEWS_EXPORT ui::metadata::TypeConverter<GURL>
    : BaseTypeConverter<true> {
  static std::u16string ToString(const GURL& source_value);
  static std::optional<GURL> FromString(const std::u16string& source_value);
  static ui::metadata::ValidStrings GetValidStrings();
};

#endif  // UI_VIEWS_METADATA_TYPE_CONVERSION_H_
