// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include <string>

#include "base/strings/strcat.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

std::u16string ui::metadata::TypeConverter<views::Background>::ToString(
    const views::Background& source_value) {
  return ui::metadata::TypeConverter<SkColor>::ToString(
      source_value.get_color());
}

std::u16string ui::metadata::TypeConverter<views::Border>::ToString(
    const views::Border& source_value) {
  return base::StrCat(
      {u"Insets: ",
       ui::metadata::TypeConverter<gfx::Insets>::ToString(
           source_value.GetInsets()),
       u"\nMin size: ",
       ui::metadata::TypeConverter<gfx::Size>::ToString(
           source_value.GetMinimumSize()),
       u"\nColor: ",
       ui::metadata::TypeConverter<SkColor>::ToString(source_value.color())});
}
