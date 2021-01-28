// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_fixer.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
namespace metadata {

base::Optional<SkColor> RgbaPiecesToSkColor(
    const std::vector<base::StringPiece16>& pieces,
    size_t start_piece) {
  int r, g, b;
  double a;
  return ((pieces.size() >= start_piece + 4) &&
          base::StringToInt(pieces[start_piece], &r) &&
          base::IsValueInRangeForNumericType<uint8_t>(r) &&
          base::StringToInt(pieces[start_piece + 1], &g) &&
          base::IsValueInRangeForNumericType<uint8_t>(g) &&
          base::StringToInt(pieces[start_piece + 2], &b) &&
          base::IsValueInRangeForNumericType<uint8_t>(b) &&
          base::StringToDouble(pieces[start_piece + 3], &a))
             ? base::make_optional(SkColorSetARGB(
                   base::ClampRound<SkAlpha>(a * SK_AlphaOPAQUE), r, g, b))
             : base::nullopt;
}

base::string16 PointerToString(const void* pointer_val) {
  return pointer_val ? base::ASCIIToUTF16("(assigned)")
                     : base::ASCIIToUTF16("(not assigned)");
}

const base::string16& GetNullOptStr() {
  static const base::NoDestructor<base::string16> kNullOptStr(
      base::ASCIIToUTF16("<Empty>"));
  return *kNullOptStr;
}

/***** String Conversions *****/

#define CONVERT_NUMBER_TO_STRING(T)                           \
  base::string16 TypeConverter<T>::ToString(T source_value) { \
    return base::NumberToString16(source_value);              \
  }

CONVERT_NUMBER_TO_STRING(int8_t)
CONVERT_NUMBER_TO_STRING(int16_t)
CONVERT_NUMBER_TO_STRING(int32_t)
CONVERT_NUMBER_TO_STRING(int64_t)
CONVERT_NUMBER_TO_STRING(uint8_t)
CONVERT_NUMBER_TO_STRING(uint16_t)
CONVERT_NUMBER_TO_STRING(uint32_t)
CONVERT_NUMBER_TO_STRING(uint64_t)
CONVERT_NUMBER_TO_STRING(float)
CONVERT_NUMBER_TO_STRING(double)

base::string16 TypeConverter<bool>::ToString(bool source_value) {
  return base::ASCIIToUTF16(source_value ? "true" : "false");
}

ValidStrings TypeConverter<bool>::GetValidStrings() {
  return {base::ASCIIToUTF16("false"), base::ASCIIToUTF16("true")};
}

base::string16 TypeConverter<const char*>::ToString(const char* source_value) {
  return base::UTF8ToUTF16(source_value);
}

base::string16 TypeConverter<base::string16>::ToString(
    const base::string16& source_value) {
  return source_value;
}

base::string16 TypeConverter<base::TimeDelta>::ToString(
    const base::TimeDelta& source_value) {
  return base::NumberToString16(source_value.InSecondsF()) +
         base::ASCIIToUTF16("s");
}

base::string16 TypeConverter<gfx::Insets>::ToString(
    const gfx::Insets& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::Point>::ToString(
    const gfx::Point& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::PointF>::ToString(
    const gfx::PointF& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::Range>::ToString(
    const gfx::Range& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::Rect>::ToString(
    const gfx::Rect& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::RectF>::ToString(
    const gfx::RectF& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::ShadowValues>::ToString(
    const gfx::ShadowValues& source_value) {
  base::string16 ret = base::ASCIIToUTF16("[");
  for (auto shadow_value : source_value) {
    ret += base::ASCIIToUTF16(" " + shadow_value.ToString() + ";");
  }

  ret[ret.length() - 1] = ' ';
  ret += base::ASCIIToUTF16("]");
  return ret;
}

base::string16 TypeConverter<gfx::Size>::ToString(
    const gfx::Size& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<gfx::SizeF>::ToString(
    const gfx::SizeF& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

base::string16 TypeConverter<GURL>::ToString(const GURL& source_value) {
  return base::ASCIIToUTF16(source_value.possibly_invalid_spec());
}

base::string16 TypeConverter<url::Component>::ToString(
    const url::Component& source_value) {
  return base::ASCIIToUTF16(
      base::StringPrintf("{%d,%d}", source_value.begin, source_value.len));
}

base::Optional<int8_t> TypeConverter<int8_t>::FromString(
    const base::string16& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int8_t>(ret)) {
    return static_cast<int8_t>(ret);
  }
  return base::nullopt;
}

base::Optional<int16_t> TypeConverter<int16_t>::FromString(
    const base::string16& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int16_t>(ret)) {
    return static_cast<int16_t>(ret);
  }
  return base::nullopt;
}

base::Optional<int32_t> TypeConverter<int32_t>::FromString(
    const base::string16& source_value) {
  int value;
  return base::StringToInt(source_value, &value) ? base::make_optional(value)
                                                 : base::nullopt;
}

base::Optional<int64_t> TypeConverter<int64_t>::FromString(
    const base::string16& source_value) {
  int64_t value;
  return base::StringToInt64(source_value, &value) ? base::make_optional(value)
                                                   : base::nullopt;
}

base::Optional<uint8_t> TypeConverter<uint8_t>::FromString(
    const base::string16& source_value) {
  unsigned ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint8_t>(ret)) {
    return static_cast<uint8_t>(ret);
  }
  return base::nullopt;
}

base::Optional<uint16_t> TypeConverter<uint16_t>::FromString(
    const base::string16& source_value) {
  unsigned ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint16_t>(ret)) {
    return static_cast<uint16_t>(ret);
  }
  return base::nullopt;
}

base::Optional<uint32_t> TypeConverter<uint32_t>::FromString(
    const base::string16& source_value) {
  unsigned value;
  return base::StringToUint(source_value, &value) ? base::make_optional(value)
                                                  : base::nullopt;
}

base::Optional<uint64_t> TypeConverter<uint64_t>::FromString(
    const base::string16& source_value) {
  uint64_t value;
  return base::StringToUint64(source_value, &value) ? base::make_optional(value)
                                                    : base::nullopt;
}

base::Optional<float> TypeConverter<float>::FromString(
    const base::string16& source_value) {
  if (base::Optional<double> temp =
          TypeConverter<double>::FromString(source_value))
    return static_cast<float>(temp.value());
  return base::nullopt;
}

base::Optional<double> TypeConverter<double>::FromString(
    const base::string16& source_value) {
  double value;
  return base::StringToDouble(base::UTF16ToUTF8(source_value), &value)
             ? base::make_optional(value)
             : base::nullopt;
}

base::Optional<bool> TypeConverter<bool>::FromString(
    const base::string16& source_value) {
  const bool is_true = source_value == base::ASCIIToUTF16("true");
  if (is_true || source_value == base::ASCIIToUTF16("false"))
    return is_true;
  return base::nullopt;
}

base::Optional<base::string16> TypeConverter<base::string16>::FromString(
    const base::string16& source_value) {
  return source_value;
}

base::Optional<base::TimeDelta> TypeConverter<base::TimeDelta>::FromString(
    const base::string16& source_value) {
  std::string source = base::UTF16ToUTF8(source_value);
  return base::TimeDelta::FromString(source);
}

base::Optional<gfx::Insets> TypeConverter<gfx::Insets>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16(","),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int top, left, bottom, right;
  if ((values.size() == 4) && base::StringToInt(values[0], &top) &&
      base::StringToInt(values[1], &left) &&
      base::StringToInt(values[2], &bottom) &&
      base::StringToInt(values[3], &right)) {
    return gfx::Insets(top, left, bottom, right);
  }
  return base::nullopt;
}

base::Optional<gfx::Point> TypeConverter<gfx::Point>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16(","),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int x, y;
  if ((values.size() == 2) && base::StringToInt(values[0], &x) &&
      base::StringToInt(values[1], &y)) {
    return gfx::Point(x, y);
  }
  return base::nullopt;
}

base::Optional<gfx::PointF> TypeConverter<gfx::PointF>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16(","),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double x, y;
  if ((values.size() == 2) && base::StringToDouble(values[0], &x) &&
      base::StringToDouble(values[1], &y)) {
    return gfx::PointF(x, y);
  }
  return base::nullopt;
}

base::Optional<gfx::Range> TypeConverter<gfx::Range>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("{,}"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  unsigned min, max;
  if ((values.size() == 2) && base::StringToUint(values[0], &min) &&
      base::StringToUint(values[1], &max)) {
    return gfx::Range(min, max);
  }
  return base::nullopt;
}

base::Optional<gfx::Rect> TypeConverter<gfx::Rect>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitString(source_value, base::ASCIIToUTF16(" "),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (values.size() != 2)
    return base::nullopt;
  const base::Optional<gfx::Point> origin =
      TypeConverter<gfx::Point>::FromString(values[0]);
  const base::Optional<gfx::Size> size =
      TypeConverter<gfx::Size>::FromString(values[1]);
  if (origin && size)
    return gfx::Rect(*origin, *size);
  return base::nullopt;
}

base::Optional<gfx::RectF> TypeConverter<gfx::RectF>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitString(source_value, base::ASCIIToUTF16(" "),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (values.size() != 2)
    return base::nullopt;
  const base::Optional<gfx::PointF> origin =
      TypeConverter<gfx::PointF>::FromString(values[0]);
  const base::Optional<gfx::SizeF> size =
      TypeConverter<gfx::SizeF>::FromString(values[1]);
  if (origin && size)
    return gfx::RectF(*origin, *size);
  return base::nullopt;
}

base::Optional<gfx::ShadowValues> TypeConverter<gfx::ShadowValues>::FromString(
    const base::string16& source_value) {
  gfx::ShadowValues ret;
  const auto shadow_value_strings =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("[;]"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto v : shadow_value_strings) {
    base::string16 member_string;
    base::RemoveChars(v, base::ASCIIToUTF16("()rgba"), &member_string);
    const auto members = base::SplitStringPiece(
        member_string, base::ASCIIToUTF16(","), base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    int x, y;
    double blur;
    const auto color = RgbaPiecesToSkColor(members, 3);

    if ((members.size() == 7) && base::StringToInt(members[0], &x) &&
        base::StringToInt(members[1], &y) &&
        base::StringToDouble(UTF16ToASCII(members[2]), &blur) &&
        color.has_value())
      ret.emplace_back(gfx::Vector2d(x, y), blur, color.value());
  }
  return ret;
}

base::Optional<gfx::Size> TypeConverter<gfx::Size>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("x"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int width, height;
  if ((values.size() == 2) && base::StringToInt(values[0], &width) &&
      base::StringToInt(values[1], &height)) {
    return gfx::Size(width, height);
  }
  return base::nullopt;
}

base::Optional<gfx::SizeF> TypeConverter<gfx::SizeF>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("x"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double width, height;
  if ((values.size() == 2) && base::StringToDouble(values[0], &width) &&
      base::StringToDouble(values[1], &height)) {
    return gfx::SizeF(width, height);
  }
  return base::nullopt;
}

base::Optional<GURL> TypeConverter<GURL>::FromString(
    const base::string16& source_value) {
  const GURL url =
      url_formatter::FixupURL(base::UTF16ToUTF8(source_value), std::string());
  return url.is_valid() ? base::make_optional(url) : base::nullopt;
}

base::Optional<url::Component> TypeConverter<url::Component>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("{,}"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int begin, len;
  if ((values.size() == 2) && base::StringToInt(values[0], &begin) &&
      base::StringToInt(values[1], &len) && len >= -1) {
    return url::Component(begin, len);
  }
  return base::nullopt;
}

}  // namespace metadata
}  // namespace views

DEFINE_ENUM_CONVERTERS(gfx::HorizontalAlignment,
                       {gfx::HorizontalAlignment::ALIGN_LEFT,
                        base::ASCIIToUTF16("ALIGN_LEFT")},
                       {gfx::HorizontalAlignment::ALIGN_CENTER,
                        base::ASCIIToUTF16("ALIGN_CENTER")},
                       {gfx::HorizontalAlignment::ALIGN_RIGHT,
                        base::ASCIIToUTF16("ALIGN_RIGHT")},
                       {gfx::HorizontalAlignment::ALIGN_TO_HEAD,
                        base::ASCIIToUTF16("ALIGN_TO_HEAD")})

DEFINE_ENUM_CONVERTERS(
    gfx::VerticalAlignment,
    {gfx::VerticalAlignment::ALIGN_TOP, base::ASCIIToUTF16("ALIGN_TOP")},
    {gfx::VerticalAlignment::ALIGN_MIDDLE, base::ASCIIToUTF16("ALIGN_MIDDLE")},
    {gfx::VerticalAlignment::ALIGN_BOTTOM, base::ASCIIToUTF16("ALIGN_BOTTOM")})

DEFINE_ENUM_CONVERTERS(
    gfx::ElideBehavior,
    {gfx::ElideBehavior::NO_ELIDE, base::ASCIIToUTF16("NO_ELIDE")},
    {gfx::ElideBehavior::TRUNCATE, base::ASCIIToUTF16("TRUNCATE")},
    {gfx::ElideBehavior::ELIDE_HEAD, base::ASCIIToUTF16("ELIDE_HEAD")},
    {gfx::ElideBehavior::ELIDE_MIDDLE, base::ASCIIToUTF16("ELIDE_MIDDLE")},
    {gfx::ElideBehavior::ELIDE_TAIL, base::ASCIIToUTF16("ELIDE_TAIL")},
    {gfx::ElideBehavior::ELIDE_EMAIL, base::ASCIIToUTF16("ELIDE_EMAIL")},
    {gfx::ElideBehavior::FADE_TAIL, base::ASCIIToUTF16("FADE_TAIL")})

DEFINE_ENUM_CONVERTERS(ui::TextInputType,
                       {ui::TextInputType::TEXT_INPUT_TYPE_NONE,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_NONE")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_TEXT,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_TEXT")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_PASSWORD")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_SEARCH,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_SEARCH")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_EMAIL,
                        base::ASCIIToUTF16("EXT_INPUT_TYPE_EMAIL")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_NUMBER,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_NUMBER")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_TELEPHONE")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_URL,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_URL")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_DATE,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_DATE")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_DATE_TIME")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_DATE_TIME_LOCAL")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_MONTH,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_MONTH")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_TIME,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_TIME")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_WEEK,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_WEEK")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_TEXT_AREA,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_TEXT_AREA")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_CONTENT_EDITABLE,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_CONTENT_EDITABLE")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_FIELD,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_DATE_TIME_FIELD")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_NULL,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_NULL")},
                       {ui::TextInputType::TEXT_INPUT_TYPE_MAX,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_MAX")})

DEFINE_ENUM_CONVERTERS(ui::MenuSeparatorType,
                       {ui::MenuSeparatorType::NORMAL_SEPARATOR,
                        base::ASCIIToUTF16("NORMAL_SEPARATOR")},
                       {ui::MenuSeparatorType::DOUBLE_SEPARATOR,
                        base::ASCIIToUTF16("DOUBLE_SEPARATOR")},
                       {ui::MenuSeparatorType::UPPER_SEPARATOR,
                        base::ASCIIToUTF16("UPPER_SEPARATOR")},
                       {ui::MenuSeparatorType::LOWER_SEPARATOR,
                        base::ASCIIToUTF16("LOWER_SEPARATOR")},
                       {ui::MenuSeparatorType::SPACING_SEPARATOR,
                        base::ASCIIToUTF16("SPACING_SEPARATOR")},
                       {ui::MenuSeparatorType::VERTICAL_SEPARATOR,
                        base::ASCIIToUTF16("VERTICAL_SEPARATOR")},
                       {ui::MenuSeparatorType::PADDED_SEPARATOR,
                        base::ASCIIToUTF16("PADDED_SEPARATOR")})

DEFINE_ENUM_CONVERTERS(views::ScrollView::ScrollBarMode,
                       {views::ScrollView::ScrollBarMode::kDisabled,
                        base::ASCIIToUTF16("kDisabled")},
                       {views::ScrollView::ScrollBarMode::kHiddenButEnabled,
                        base::ASCIIToUTF16("kHiddenButEnabled")},
                       {views::ScrollView::ScrollBarMode::kEnabled,
                        base::ASCIIToUTF16("kEnabled")})

DEFINE_ENUM_CONVERTERS(
    views::BubbleFrameView::PreferredArrowAdjustment,
    {views::BubbleFrameView::PreferredArrowAdjustment::kMirror,
     base::ASCIIToUTF16("kMirror")},
    {views::BubbleFrameView::PreferredArrowAdjustment::kOffset,
     base::ASCIIToUTF16("kOffset")})

DEFINE_ENUM_CONVERTERS(
    views::BubbleBorder::Arrow,
    {views::BubbleBorder::Arrow::TOP_LEFT, base::ASCIIToUTF16("TOP_LEFT")},
    {views::BubbleBorder::Arrow::TOP_RIGHT, base::ASCIIToUTF16("TOP_RIGHT")},
    {views::BubbleBorder::Arrow::BOTTOM_LEFT,
     base::ASCIIToUTF16("BOTTOM_LEFT")},
    {views::BubbleBorder::Arrow::BOTTOM_RIGHT,
     base::ASCIIToUTF16("BOTTOM_RIGHT")},
    {views::BubbleBorder::Arrow::LEFT_TOP, base::ASCIIToUTF16("LEFT_TOP")},
    {views::BubbleBorder::Arrow::RIGHT_TOP, base::ASCIIToUTF16("RIGHT_TOP")},
    {views::BubbleBorder::Arrow::LEFT_BOTTOM,
     base::ASCIIToUTF16("LEFT_BOTTOM")},
    {views::BubbleBorder::Arrow::RIGHT_BOTTOM,
     base::ASCIIToUTF16("RIGHT_BOTTOM")},
    {views::BubbleBorder::Arrow::TOP_CENTER, base::ASCIIToUTF16("TOP_CENTER")},
    {views::BubbleBorder::Arrow::BOTTOM_CENTER,
     base::ASCIIToUTF16("BOTTOM_CENTER")},
    {views::BubbleBorder::Arrow::LEFT_CENTER,
     base::ASCIIToUTF16("LEFT_CENTER")},
    {views::BubbleBorder::Arrow::RIGHT_CENTER,
     base::ASCIIToUTF16("RIGHT_CENTER")},
    {views::BubbleBorder::Arrow::NONE, base::ASCIIToUTF16("NONE")},
    {views::BubbleBorder::Arrow::FLOAT, base::ASCIIToUTF16("FLOAT")})

#define OP(enum_name) \
  { ui::NativeTheme::enum_name, base::ASCIIToUTF16(#enum_name) }
DEFINE_ENUM_CONVERTERS(ui::NativeTheme::ColorId, NATIVE_THEME_COLOR_IDS)
#undef OP
