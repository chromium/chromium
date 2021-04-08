// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include <cmath>
#include <string>

#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
namespace metadata {

const char kNoPrefix[] = "";
const char kSkColorPrefix[] = "--";

std::u16string PointerToString(const void* pointer_val) {
  return pointer_val ? u"(assigned)" : u"(not assigned)";
}

const std::u16string& GetNullOptStr() {
  static const base::NoDestructor<std::u16string> kNullOptStr(u"<Empty>");
  return *kNullOptStr;
}

/***** String Conversions *****/

#define CONVERT_NUMBER_TO_STRING(T)                           \
  std::u16string TypeConverter<T>::ToString(T source_value) { \
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

std::u16string TypeConverter<bool>::ToString(bool source_value) {
  return base::ASCIIToUTF16(source_value ? "true" : "false");
}

ValidStrings TypeConverter<bool>::GetValidStrings() {
  return {u"false", u"true"};
}

std::u16string TypeConverter<const char*>::ToString(const char* source_value) {
  return base::UTF8ToUTF16(source_value);
}

std::u16string TypeConverter<base::FilePath>::ToString(
    const base::FilePath& source_value) {
  return source_value.AsUTF16Unsafe();
}

std::u16string TypeConverter<std::u16string>::ToString(
    const std::u16string& source_value) {
  return source_value;
}

std::u16string TypeConverter<base::TimeDelta>::ToString(
    const base::TimeDelta& source_value) {
  return base::NumberToString16(source_value.InSecondsF()) + u"s";
}

std::u16string TypeConverter<gfx::Insets>::ToString(
    const gfx::Insets& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::Point>::ToString(
    const gfx::Point& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::PointF>::ToString(
    const gfx::PointF& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::Range>::ToString(
    const gfx::Range& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::Rect>::ToString(
    const gfx::Rect& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::RectF>::ToString(
    const gfx::RectF& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::ShadowValues>::ToString(
    const gfx::ShadowValues& source_value) {
  std::u16string ret = u"[";
  for (auto shadow_value : source_value) {
    ret += base::ASCIIToUTF16(" " + shadow_value.ToString() + ";");
  }

  ret[ret.length() - 1] = ' ';
  ret += u"]";
  return ret;
}

std::u16string TypeConverter<gfx::Size>::ToString(
    const gfx::Size& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<gfx::SizeF>::ToString(
    const gfx::SizeF& source_value) {
  return base::ASCIIToUTF16(source_value.ToString());
}

std::u16string TypeConverter<GURL>::ToString(const GURL& source_value) {
  return base::ASCIIToUTF16(source_value.possibly_invalid_spec());
}

std::u16string TypeConverter<url::Component>::ToString(
    const url::Component& source_value) {
  return base::ASCIIToUTF16(
      base::StringPrintf("{%d,%d}", source_value.begin, source_value.len));
}

base::Optional<int8_t> TypeConverter<int8_t>::FromString(
    const std::u16string& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int8_t>(ret)) {
    return static_cast<int8_t>(ret);
  }
  return base::nullopt;
}

base::Optional<int16_t> TypeConverter<int16_t>::FromString(
    const std::u16string& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int16_t>(ret)) {
    return static_cast<int16_t>(ret);
  }
  return base::nullopt;
}

base::Optional<int32_t> TypeConverter<int32_t>::FromString(
    const std::u16string& source_value) {
  int value;
  return base::StringToInt(source_value, &value) ? base::make_optional(value)
                                                 : base::nullopt;
}

base::Optional<int64_t> TypeConverter<int64_t>::FromString(
    const std::u16string& source_value) {
  int64_t value;
  return base::StringToInt64(source_value, &value) ? base::make_optional(value)
                                                   : base::nullopt;
}

base::Optional<uint8_t> TypeConverter<uint8_t>::FromString(
    const std::u16string& source_value) {
  unsigned ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint8_t>(ret)) {
    return static_cast<uint8_t>(ret);
  }
  return base::nullopt;
}

base::Optional<uint16_t> TypeConverter<uint16_t>::FromString(
    const std::u16string& source_value) {
  unsigned ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint16_t>(ret)) {
    return static_cast<uint16_t>(ret);
  }
  return base::nullopt;
}

base::Optional<uint32_t> TypeConverter<uint32_t>::FromString(
    const std::u16string& source_value) {
  unsigned value;
  return base::StringToUint(source_value, &value) ? base::make_optional(value)
                                                  : base::nullopt;
}

base::Optional<uint64_t> TypeConverter<uint64_t>::FromString(
    const std::u16string& source_value) {
  uint64_t value;
  return base::StringToUint64(source_value, &value) ? base::make_optional(value)
                                                    : base::nullopt;
}

base::Optional<float> TypeConverter<float>::FromString(
    const std::u16string& source_value) {
  if (base::Optional<double> temp =
          TypeConverter<double>::FromString(source_value))
    return static_cast<float>(temp.value());
  return base::nullopt;
}

base::Optional<double> TypeConverter<double>::FromString(
    const std::u16string& source_value) {
  double value;
  return base::StringToDouble(base::UTF16ToUTF8(source_value), &value)
             ? base::make_optional(value)
             : base::nullopt;
}

base::Optional<bool> TypeConverter<bool>::FromString(
    const std::u16string& source_value) {
  const bool is_true = source_value == u"true";
  if (is_true || source_value == u"false")
    return is_true;
  return base::nullopt;
}

base::Optional<std::u16string> TypeConverter<std::u16string>::FromString(
    const std::u16string& source_value) {
  return source_value;
}

base::Optional<base::FilePath> TypeConverter<base::FilePath>::FromString(
    const std::u16string& source_value) {
  return base::FilePath::FromUTF16Unsafe(source_value);
}

base::Optional<base::TimeDelta> TypeConverter<base::TimeDelta>::FromString(
    const std::u16string& source_value) {
  std::string source = base::UTF16ToUTF8(source_value);
  return base::TimeDelta::FromString(source);
}

base::Optional<gfx::Insets> TypeConverter<gfx::Insets>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int x, y;
  if ((values.size() == 2) && base::StringToInt(values[0], &x) &&
      base::StringToInt(values[1], &y)) {
    return gfx::Point(x, y);
  }
  return base::nullopt;
}

base::Optional<gfx::PointF> TypeConverter<gfx::PointF>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double x, y;
  if ((values.size() == 2) && base::StringToDouble(values[0], &x) &&
      base::StringToDouble(values[1], &y)) {
    return gfx::PointF(x, y);
  }
  return base::nullopt;
}

base::Optional<gfx::Range> TypeConverter<gfx::Range>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"{,}", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  unsigned min, max;
  if ((values.size() == 2) && base::StringToUint(values[0], &min) &&
      base::StringToUint(values[1], &max)) {
    return gfx::Range(min, max);
  }
  return base::nullopt;
}

base::Optional<gfx::Rect> TypeConverter<gfx::Rect>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitString(
      source_value, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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
    const std::u16string& source_value) {
  const auto values = base::SplitString(
      source_value, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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
    const std::u16string& source_value) {
  gfx::ShadowValues ret;
  const auto shadow_value_strings = base::SplitStringPiece(
      source_value, u"[;]", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto v : shadow_value_strings) {
    std::u16string value = std::u16string(v);
    base::String16Tokenizer tokenizer(
        value, u"(,)", base::String16Tokenizer::WhitespacePolicy::kSkipOver);
    tokenizer.set_options(base::String16Tokenizer::RETURN_DELIMS);
    int x, y;
    double blur;
    if (tokenizer.GetNext() && tokenizer.token() == u"(" &&
        tokenizer.GetNext() && base::StringToInt(tokenizer.token(), &x) &&
        tokenizer.GetNext() && tokenizer.token() == u"," &&
        tokenizer.GetNext() && base::StringToInt(tokenizer.token(), &y) &&
        tokenizer.GetNext() && tokenizer.token() == u")" &&
        tokenizer.GetNext() && tokenizer.token() == u"," &&
        tokenizer.GetNext() && base::StringToDouble(tokenizer.token(), &blur) &&
        tokenizer.GetNext() && tokenizer.token() == u"," &&
        tokenizer.GetNext()) {
      const auto color =
          SkColorConverter::GetNextColor(tokenizer.token_begin(), value.cend());
      if (color)
        ret.emplace_back(gfx::Vector2d(x, y), blur, color.value());
    }
  }
  return ret;
}

base::Optional<gfx::Size> TypeConverter<gfx::Size>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int width, height;
  if ((values.size() == 2) && base::StringToInt(values[0], &width) &&
      base::StringToInt(values[1], &height)) {
    return gfx::Size(width, height);
  }
  return base::nullopt;
}

base::Optional<gfx::SizeF> TypeConverter<gfx::SizeF>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double width, height;
  if ((values.size() == 2) && base::StringToDouble(values[0], &width) &&
      base::StringToDouble(values[1], &height)) {
    return gfx::SizeF(width, height);
  }
  return base::nullopt;
}

base::Optional<GURL> TypeConverter<GURL>::FromString(
    const std::u16string& source_value) {
  const GURL url =
      url_formatter::FixupURL(base::UTF16ToUTF8(source_value), std::string());
  return url.is_valid() ? base::make_optional(url) : base::nullopt;
}

base::Optional<url::Component> TypeConverter<url::Component>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"{,}", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int begin, len;
  if ((values.size() == 2) && base::StringToInt(values[0], &begin) &&
      base::StringToInt(values[1], &len) && len >= -1) {
    return url::Component(begin, len);
  }
  return base::nullopt;
}

std::u16string TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ToString(
    SkColor source_value) {
  return base::UTF8ToUTF16(color_utils::SkColorToRgbaString(source_value));
}

base::Optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::FromString(
    const std::u16string& source_value) {
  return GetNextColor(source_value.cbegin(), source_value.cend());
}

ValidStrings TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetValidStrings() {
  return {};
}

bool TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetNextColor(
    std::u16string::const_iterator start,
    std::u16string::const_iterator end,
    std::u16string& color,
    std::u16string::const_iterator& next_token) {
  static const auto open_paren = u'(';
  static const auto close_paren = u')';
  static const std::vector<std::u16string> schemes = {u"hsl", u"hsla", u"rgb",
                                                      u"rgba"};
  base::String16Tokenizer tokenizer(
      start, end, u"(,)", base::String16Tokenizer::WhitespacePolicy::kSkipOver);
  tokenizer.set_options(base::String16Tokenizer::RETURN_DELIMS);
  for (; tokenizer.GetNext();) {
    if (!tokenizer.token_is_delim()) {
      base::StringPiece16 token = tokenizer.token_piece();
      std::u16string::const_iterator start_color = tokenizer.token_begin();
      if (base::ranges::find(schemes.begin(), schemes.end(), token) !=
          schemes.end()) {
        if (!tokenizer.GetNext() || *tokenizer.token_begin() != open_paren)
          return false;
        for (;
             tokenizer.GetNext() && *tokenizer.token_begin() != close_paren;) {
        }
        if (*tokenizer.token_begin() != close_paren)
          return false;
      }
      next_token = tokenizer.token_end();
      color = std::u16string(start_color, next_token);
      return true;
    }
  }
  return false;
}

bool TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetNextColor(
    std::u16string::const_iterator start,
    std::u16string::const_iterator end,
    std::u16string& color) {
  std::u16string::const_iterator next_token;
  return GetNextColor(start, end, color, next_token);
}

base::Optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetNextColor(
    std::u16string::const_iterator start,
    std::u16string::const_iterator end,
    std::u16string::const_iterator& next_token) {
  std::u16string color;
  if (GetNextColor(start, end, color, next_token)) {
    if (base::StartsWith(color, u"hsl", base::CompareCase::SENSITIVE))
      return ParseHslString(color);
    if (base::StartsWith(color, u"rgb", base::CompareCase::SENSITIVE))
      return ParseRgbString(color);
    if (base::StartsWith(color, u"0x", base::CompareCase::INSENSITIVE_ASCII))
      return ParseHexString(color);
    SkColor value;
    if (base::StringToUint(color, &value))
      return base::make_optional(value);
  }
  return base::nullopt;
}

base::Optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetNextColor(
    std::u16string::const_iterator start,
    std::u16string::const_iterator end) {
  std::u16string::const_iterator next_token;
  return GetNextColor(start, end, next_token);
}

base::Optional<SkColor>
TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::RgbaPiecesToSkColor(
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
          base::StringToDouble(pieces[start_piece + 3], &a) && a >= 0.0 &&
          a <= 1.0)
             ? base::make_optional(SkColorSetARGB(
                   base::ClampRound<SkAlpha>(a * SK_AlphaOPAQUE), r, g, b))
             : base::nullopt;
}

base::Optional<SkColor>
TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ParseHexString(
    const std::u16string& hex_string) {
  SkColor value;
  if (base::HexStringToUInt(base::UTF16ToUTF8(hex_string), &value)) {
    // Add in a 1.0 alpha channel if it wasn't included in the input.
    if (hex_string.length() <= 8)
      value = SkColorSetA(value, 0xFF);
    return base::make_optional(value);
  }
  return base::nullopt;
}

base::Optional<SkColor>
TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ParseHslString(
    const std::u16string& hsl_string) {
  std::u16string pruned_string;
  base::RemoveChars(hsl_string, u"(%)hsla", &pruned_string);
  const auto values = base::SplitStringPiece(
      pruned_string, u", ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double h, s, v;
  double a = 1.0;
  if (values.size() >= 3 && values.size() <= 4 &&
      base::StringToDouble(values[0], &h) &&
      base::StringToDouble(values[1], &s) &&
      base::StringToDouble(values[2], &v) &&
      (values.size() == 3 ||
       (base::StringToDouble(values[3], &a) && a >= 0.0 && a <= 1.0))) {
    SkScalar hsv[3];
    hsv[0] = base::ClampToRange(std::fmod(h, 360.0), 0.0, 360.0);
    hsv[1] = s > 1.0 ? base::ClampToRange(s, 0.0, 100.0) / 100.0
                     : base::ClampToRange(s, 0.0, 1.0);
    hsv[2] = v > 1.0 ? base::ClampToRange(v, 0.0, 100.0) / 100.0
                     : base::ClampToRange(v, 0.0, 1.0);
    return base::make_optional(
        SkHSVToColor(base::ClampRound<SkAlpha>(a * SK_AlphaOPAQUE), hsv));
  }
  return base::nullopt;
}

base::Optional<SkColor>
TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ParseRgbString(
    const std::u16string& rgb_string) {
  // Declare a constant string here for use below since it might trigger an
  // ASAN error due to the stack temp going out of scope before the call to
  // RgbaPiecesToSkColor.
  static const auto opaque_alpha = base::ASCIIToUTF16("1.0");
  std::u16string pruned_string;
  base::RemoveChars(rgb_string, u"()rgba", &pruned_string);
  auto values = base::SplitStringPiece(
      pruned_string, u", ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // if it was just an rgb string, add the 1.0 alpha
  if (values.size() == 3)
    values.push_back(opaque_alpha);
  return RgbaPiecesToSkColor(values, 0);
}

}  // namespace metadata
}  // namespace views

DEFINE_ENUM_CONVERTERS(gfx::HorizontalAlignment,
                       {gfx::HorizontalAlignment::ALIGN_LEFT, u"ALIGN_LEFT"},
                       {gfx::HorizontalAlignment::ALIGN_CENTER,
                        u"ALIGN_CENTER"},
                       {gfx::HorizontalAlignment::ALIGN_RIGHT, u"ALIGN_RIGHT"},
                       {gfx::HorizontalAlignment::ALIGN_TO_HEAD,
                        u"ALIGN_TO_HEAD"})

DEFINE_ENUM_CONVERTERS(gfx::VerticalAlignment,
                       {gfx::VerticalAlignment::ALIGN_TOP, u"ALIGN_TOP"},
                       {gfx::VerticalAlignment::ALIGN_MIDDLE, u"ALIGN_MIDDLE"},
                       {gfx::VerticalAlignment::ALIGN_BOTTOM, u"ALIGN_BOTTOM"})

DEFINE_ENUM_CONVERTERS(gfx::ElideBehavior,
                       {gfx::ElideBehavior::NO_ELIDE, u"NO_ELIDE"},
                       {gfx::ElideBehavior::TRUNCATE, u"TRUNCATE"},
                       {gfx::ElideBehavior::ELIDE_HEAD, u"ELIDE_HEAD"},
                       {gfx::ElideBehavior::ELIDE_MIDDLE, u"ELIDE_MIDDLE"},
                       {gfx::ElideBehavior::ELIDE_TAIL, u"ELIDE_TAIL"},
                       {gfx::ElideBehavior::ELIDE_EMAIL, u"ELIDE_EMAIL"},
                       {gfx::ElideBehavior::FADE_TAIL, u"FADE_TAIL"})

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
    {ui::TextInputType::TEXT_INPUT_TYPE_NULL, u"TEXT_INPUT_TYPE_NULL"},
    {ui::TextInputType::TEXT_INPUT_TYPE_MAX, u"TEXT_INPUT_TYPE_MAX"})

DEFINE_ENUM_CONVERTERS(
    ui::MenuSeparatorType,
    {ui::MenuSeparatorType::NORMAL_SEPARATOR, u"NORMAL_SEPARATOR"},
    {ui::MenuSeparatorType::DOUBLE_SEPARATOR, u"DOUBLE_SEPARATOR"},
    {ui::MenuSeparatorType::UPPER_SEPARATOR, u"UPPER_SEPARATOR"},
    {ui::MenuSeparatorType::LOWER_SEPARATOR, u"LOWER_SEPARATOR"},
    {ui::MenuSeparatorType::SPACING_SEPARATOR, u"SPACING_SEPARATOR"},
    {ui::MenuSeparatorType::VERTICAL_SEPARATOR, u"VERTICAL_SEPARATOR"},
    {ui::MenuSeparatorType::PADDED_SEPARATOR, u"PADDED_SEPARATOR"})

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

#define OP(enum_name) \
  { ui::NativeTheme::enum_name, base::ASCIIToUTF16(#enum_name) }
DEFINE_ENUM_CONVERTERS(ui::NativeTheme::ColorId, NATIVE_THEME_COLOR_IDS)
#undef OP
