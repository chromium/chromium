// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include <string>

#include "components/url_formatter/url_fixer.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

std::u16string ui::metadata::TypeConverter<GURL>::ToString(
    const GURL& source_value) {
  return base::ASCIIToUTF16(source_value.possibly_invalid_spec());
}

std::optional<GURL> ui::metadata::TypeConverter<GURL>::FromString(
    const std::u16string& source_value) {
  const GURL url =
      url_formatter::FixupURL(base::UTF16ToUTF8(source_value), std::string());
  return url.is_valid() ? std::make_optional(url) : std::nullopt;
}

ui::metadata::ValidStrings
ui::metadata::TypeConverter<GURL>::GetValidStrings() {
  return {};
}

DEFINE_ENUM_CONVERTERS(
    views::ScrollView::ScrollBarMode,
    {views::ScrollView::ScrollBarMode::kDisabled, u"kDisabled"},
    {views::ScrollView::ScrollBarMode::kHiddenButEnabled, u"kHiddenButEnabled"},
    {views::ScrollView::ScrollBarMode::kEnabled, u"kEnabled"})

DEFINE_ENUM_CONVERTERS(
    views::BubbleFrameView::PreferredArrowAdjustment,
    {views::BubbleFrameView::PreferredArrowAdjustment::kMirror, u"kMirror"},
    {views::BubbleFrameView::PreferredArrowAdjustment::kOffset, u"kOffset"})

DEFINE_ENUM_CONVERTERS(
    views::BubbleBorder::Arrow,
    {views::BubbleBorder::Arrow::TOP_LEFT, u"TOP_LEFT"},
    {views::BubbleBorder::Arrow::TOP_RIGHT, u"TOP_RIGHT"},
    {views::BubbleBorder::Arrow::BOTTOM_LEFT, u"BOTTOM_LEFT"},
    {views::BubbleBorder::Arrow::BOTTOM_RIGHT, u"BOTTOM_RIGHT"},
    {views::BubbleBorder::Arrow::LEFT_TOP, u"LEFT_TOP"},
    {views::BubbleBorder::Arrow::RIGHT_TOP, u"RIGHT_TOP"},
    {views::BubbleBorder::Arrow::LEFT_BOTTOM, u"LEFT_BOTTOM"},
    {views::BubbleBorder::Arrow::RIGHT_BOTTOM, u"RIGHT_BOTTOM"},
    {views::BubbleBorder::Arrow::TOP_CENTER, u"TOP_CENTER"},
    {views::BubbleBorder::Arrow::BOTTOM_CENTER, u"BOTTOM_CENTER"},
    {views::BubbleBorder::Arrow::LEFT_CENTER, u"LEFT_CENTER"},
    {views::BubbleBorder::Arrow::RIGHT_CENTER, u"RIGHT_CENTER"},
    {views::BubbleBorder::Arrow::NONE, u"NONE"},
    {views::BubbleBorder::Arrow::FLOAT, u"FLOAT"})

DEFINE_ENUM_CONVERTERS(
    ui::TextInputType,
    {ui::TextInputType::TEXT_INPUT_TYPE_NONE, u"TEXT_INPUT_TYPE_NONE"},
    {ui::TextInputType::TEXT_INPUT_TYPE_TEXT, u"TEXT_INPUT_TYPE_TEXT"},
    {ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD, u"TEXT_INPUT_TYPE_PASSWORD"},
    {ui::TextInputType::TEXT_INPUT_TYPE_SEARCH, u"TEXT_INPUT_TYPE_SEARCH"},
    {ui::TextInputType::TEXT_INPUT_TYPE_EMAIL, u"EXT_INPUT_TYPE_EMAIL"},
    {ui::TextInputType::TEXT_INPUT_TYPE_NUMBER, u"TEXT_INPUT_TYPE_NUMBER"},
    {ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE,
     u"TEXT_INPUT_TYPE_TELEPHONE"},
    {ui::TextInputType::TEXT_INPUT_TYPE_URL, u"TEXT_INPUT_TYPE_URL"},
    {ui::TextInputType::TEXT_INPUT_TYPE_DATE, u"TEXT_INPUT_TYPE_DATE"},
    {ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME,
     u"TEXT_INPUT_TYPE_DATE_TIME"},
    {ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
     u"TEXT_INPUT_TYPE_DATE_TIME_LOCAL"},
    {ui::TextInputType::TEXT_INPUT_TYPE_MONTH, u"TEXT_INPUT_TYPE_MONTH"},
    {ui::TextInputType::TEXT_INPUT_TYPE_TIME, u"TEXT_INPUT_TYPE_TIME"},
    {ui::TextInputType::TEXT_INPUT_TYPE_WEEK, u"TEXT_INPUT_TYPE_WEEK"},
    {ui::TextInputType::TEXT_INPUT_TYPE_TEXT_AREA,
     u"TEXT_INPUT_TYPE_TEXT_AREA"},
    {ui::TextInputType::TEXT_INPUT_TYPE_CONTENT_EDITABLE,
     u"TEXT_INPUT_TYPE_CONTENT_EDITABLE"},
    {ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_FIELD,
     u"TEXT_INPUT_TYPE_DATE_TIME_FIELD"},
    {ui::TextInputType::TEXT_INPUT_TYPE_NULL, u"TEXT_INPUT_TYPE_NULL"})

DEFINE_ENUM_CONVERTERS(views::Separator::Orientation,
                       {views::Separator::Orientation::kHorizontal,
                        u"kHorizontal"},
                       {views::Separator::Orientation::kVertical, u"kVertical"})
