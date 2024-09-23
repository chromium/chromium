// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/metadata/base_type_conversion.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time_delta_from_string.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
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
  return source_value ? u"true" : u"false";
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
  // This is different from gfx::Insets::ToString().
  return base::ASCIIToUTF16(
      base::StringPrintf("%d,%d,%d,%d", source_value.top(), source_value.left(),
                         source_value.bottom(), source_value.right()));
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
    ret += u" " + base::ASCIIToUTF16(shadow_value.ToString()) + u";";
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

std::u16string TypeConverter<std::string>::ToString(
    const std::string& source_value) {
  return base::UTF8ToUTF16(source_value);
}

std::u16string TypeConverter<url::Component>::ToString(
    const url::Component& source_value) {
  return base::ASCIIToUTF16(
      base::StringPrintf("{%d,%d}", source_value.begin, source_value.len));
}

std::optional<int8_t> TypeConverter<int8_t>::FromString(
    const std::u16string& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int8_t>(ret)) {
    return static_cast<int8_t>(ret);
  }
  return std::nullopt;
}

std::optional<int16_t> TypeConverter<int16_t>::FromString(
    const std::u16string& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int16_t>(ret)) {
    return static_cast<int16_t>(ret);
  }
  return std::nullopt;
}

std::optional<int32_t> TypeConverter<int32_t>::FromString(
    const std::u16string& source_value) {
  int value;
  return base::StringToInt(source_value, &value) ? std::make_optional(value)
                                                 : std::nullopt;
}

std::optional<int64_t> TypeConverter<int64_t>::FromString(
    const std::u16string& source_value) {
  int64_t value;
  return base::StringToInt64(source_value, &value) ? std::make_optional(value)
                                                   : std::nullopt;
}

std::optional<uint8_t> TypeConverter<uint8_t>::FromString(
    const std::u16string& source_value) {
  unsigned ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint8_t>(ret)) {
    return static_cast<uint8_t>(ret);
  }
  return std::nullopt;
}

std::optional<uint16_t> TypeConverter<uint16_t>::FromString(
    const std::u16string& source_value) {
  unsigned ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint16_t>(ret)) {
    return static_cast<uint16_t>(ret);
  }
  return std::nullopt;
}

std::optional<uint32_t> TypeConverter<uint32_t>::FromString(
    const std::u16string& source_value) {
  unsigned value;
  return base::StringToUint(source_value, &value) ? std::make_optional(value)
                                                  : std::nullopt;
}

std::optional<uint64_t> TypeConverter<uint64_t>::FromString(
    const std::u16string& source_value) {
  uint64_t value;
  return base::StringToUint64(source_value, &value) ? std::make_optional(value)
                                                    : std::nullopt;
}

std::optional<float> TypeConverter<float>::FromString(
    const std::u16string& source_value) {
  if (std::optional<double> temp =
          TypeConverter<double>::FromString(source_value)) {
    return static_cast<float>(temp.value());
  }
  return std::nullopt;
}

std::optional<double> TypeConverter<double>::FromString(
    const std::u16string& source_value) {
  double value;
  return base::StringToDouble(base::UTF16ToUTF8(source_value), &value)
             ? std::make_optional(value)
             : std::nullopt;
}

std::optional<bool> TypeConverter<bool>::FromString(
    const std::u16string& source_value) {
  const bool is_true = source_value == u"true";
  if (is_true || source_value == u"false")
    return is_true;
  return std::nullopt;
}

std::optional<std::u16string> TypeConverter<std::u16string>::FromString(
    const std::u16string& source_value) {
  return source_value;
}

std::optional<base::FilePath> TypeConverter<base::FilePath>::FromString(
    const std::u16string& source_value) {
  return base::FilePath::FromUTF16Unsafe(source_value);
}

std::optional<base::TimeDelta> TypeConverter<base::TimeDelta>::FromString(
    const std::u16string& source_value) {
  std::string source = base::UTF16ToUTF8(source_value);
  return base::TimeDeltaFromString(source);
}

std::optional<gfx::Insets> TypeConverter<gfx::Insets>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int top, left, bottom, right;
  if ((values.size() == 4) && base::StringToInt(values[0], &top) &&
      base::StringToInt(values[1], &left) &&
      base::StringToInt(values[2], &bottom) &&
      base::StringToInt(values[3], &right)) {
    return gfx::Insets::TLBR(top, left, bottom, right);
  }
  return std::nullopt;
}

std::optional<gfx::Point> TypeConverter<gfx::Point>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int x, y;
  if ((values.size() == 2) && base::StringToInt(values[0], &x) &&
      base::StringToInt(values[1], &y)) {
    return gfx::Point(x, y);
  }
  return std::nullopt;
}

std::optional<gfx::PointF> TypeConverter<gfx::PointF>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double x, y;
  if ((values.size() == 2) && base::StringToDouble(values[0], &x) &&
      base::StringToDouble(values[1], &y)) {
    return gfx::PointF(x, y);
  }
  return std::nullopt;
}

std::optional<gfx::Range> TypeConverter<gfx::Range>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"{,}", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  unsigned min, max;
  if ((values.size() == 2) && base::StringToUint(values[0], &min) &&
      base::StringToUint(values[1], &max)) {
    return gfx::Range(min, max);
  }
  return std::nullopt;
}

std::optional<gfx::Rect> TypeConverter<gfx::Rect>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitString(
      source_value, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (values.size() != 2)
    return std::nullopt;
  const std::optional<gfx::Point> origin =
      TypeConverter<gfx::Point>::FromString(values[0]);
  const std::optional<gfx::Size> size =
      TypeConverter<gfx::Size>::FromString(values[1]);
  if (origin && size)
    return gfx::Rect(*origin, *size);
  return std::nullopt;
}

std::optional<gfx::RectF> TypeConverter<gfx::RectF>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitString(
      source_value, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (values.size() != 2)
    return std::nullopt;
  const std::optional<gfx::PointF> origin =
      TypeConverter<gfx::PointF>::FromString(values[0]);
  const std::optional<gfx::SizeF> size =
      TypeConverter<gfx::SizeF>::FromString(values[1]);
  if (origin && size)
    return gfx::RectF(*origin, *size);
  return std::nullopt;
}

std::optional<gfx::ShadowValues> TypeConverter<gfx::ShadowValues>::FromString(
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
    if (tokenizer.GetNext() && tokenizer.token_piece() == u"(" &&
        tokenizer.GetNext() && base::StringToInt(tokenizer.token_piece(), &x) &&
        tokenizer.GetNext() && tokenizer.token_piece() == u"," &&
        tokenizer.GetNext() && base::StringToInt(tokenizer.token_piece(), &y) &&
        tokenizer.GetNext() && tokenizer.token_piece() == u")" &&
        tokenizer.GetNext() && tokenizer.token_piece() == u"," &&
        tokenizer.GetNext() &&
        base::StringToDouble(tokenizer.token_piece(), &blur) &&
        tokenizer.GetNext() && tokenizer.token_piece() == u"," &&
        tokenizer.GetNext()) {
      const auto color =
          SkColorConverter::GetNextColor(tokenizer.token_begin(), value.cend());
      if (color)
        ret.emplace_back(gfx::Vector2d(x, y), blur, color.value());
    }
  }
  return ret;
}

std::optional<gfx::Size> TypeConverter<gfx::Size>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int width, height;
  if ((values.size() == 2) && base::StringToInt(values[0], &width) &&
      base::StringToInt(values[1], &height)) {
    return gfx::Size(width, height);
  }
  return std::nullopt;
}

std::optional<gfx::SizeF> TypeConverter<gfx::SizeF>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  double width, height;
  if ((values.size() == 2) && base::StringToDouble(values[0], &width) &&
      base::StringToDouble(values[1], &height)) {
    return gfx::SizeF(width, height);
  }
  return std::nullopt;
}

std::optional<std::string> TypeConverter<std::string>::FromString(
    const std::u16string& source_value) {
  return base::UTF16ToUTF8(source_value);
}

std::optional<url::Component> TypeConverter<url::Component>::FromString(
    const std::u16string& source_value) {
  const auto values = base::SplitStringPiece(
      source_value, u"{,}", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int begin, len;
  if ((values.size() == 2) && base::StringToInt(values[0], &begin) &&
      base::StringToInt(values[1], &len) && len >= -1) {
    return url::Component(begin, len);
  }
  return std::nullopt;
}

std::u16string TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ToString(
    SkColor source_value) {
  return base::UTF8ToUTF16(color_utils::SkColorToRgbaString(source_value));
}

std::optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::FromString(
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
  static constexpr auto schemes = base::MakeFixedFlatSet<std::u16string_view>(
      {u"hsl", u"hsla", u"rgb", u"rgba"});

  base::String16Tokenizer tokenizer(
      start, end, u"(,)", base::String16Tokenizer::WhitespacePolicy::kSkipOver);
  tokenizer.set_options(base::String16Tokenizer::RETURN_DELIMS);
  for (; tokenizer.GetNext();) {
    if (!tokenizer.token_is_delim()) {
      std::u16string_view token = tokenizer.token_piece();
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

std::optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetNextColor(
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
      return std::make_optional(value);
  }
  return std::nullopt;
}

std::optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::GetNextColor(
    std::u16string::const_iterator start,
    std::u16string::const_iterator end) {
  std::u16string::const_iterator next_token;
  return GetNextColor(start, end, next_token);
}

std::optional<SkColor>
TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::RgbaPiecesToSkColor(
    const std::vector<std::u16string_view>& pieces,
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
             ? std::make_optional(SkColorSetARGB(
                   base::ClampRound<SkAlpha>(a * SK_AlphaOPAQUE), r, g, b))
             : std::nullopt;
}

std::optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ParseHexString(
    const std::u16string& hex_string) {
  SkColor value;
  if (base::HexStringToUInt(base::UTF16ToUTF8(hex_string), &value)) {
    // Add in a 1.0 alpha channel if it wasn't included in the input.
    if (hex_string.length() <= 8)
      value = SkColorSetA(value, 0xFF);
    return std::make_optional(value);
  }
  return std::nullopt;
}

std::optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ParseHslString(
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
    hsv[0] = std::clamp(std::fmod(h, 360.0), 0.0, 360.0);
    hsv[1] =
        s > 1.0 ? std::clamp(s, 0.0, 100.0) / 100.0 : std::clamp(s, 0.0, 1.0);
    hsv[2] =
        v > 1.0 ? std::clamp(v, 0.0, 100.0) / 100.0 : std::clamp(v, 0.0, 1.0);
    return std::make_optional(
        SkHSVToColor(base::ClampRound<SkAlpha>(a * SK_AlphaOPAQUE), hsv));
  }
  return std::nullopt;
}

std::optional<SkColor> TypeConverter<UNIQUE_TYPE_NAME(SkColor)>::ParseRgbString(
    const std::u16string& rgb_string) {
  // Declare a constant string here for use below since it might trigger an
  // ASAN error due to the stack temp going out of scope before the call to
  // RgbaPiecesToSkColor.
  std::u16string pruned_string;
  base::RemoveChars(rgb_string, u"()rgba", &pruned_string);
  auto values = base::SplitStringPiece(
      pruned_string, u", ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // if it was just an rgb string, add the 1.0 alpha
  if (values.size() == 3)
    values.push_back(u"1.0");
  return RgbaPiecesToSkColor(values, 0);
}

}  // namespace metadata
}  // namespace ui

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
    ui::MenuSeparatorType,
    {ui::MenuSeparatorType::NORMAL_SEPARATOR, u"NORMAL_SEPARATOR"},
    {ui::MenuSeparatorType::DOUBLE_SEPARATOR, u"DOUBLE_SEPARATOR"},
    {ui::MenuSeparatorType::UPPER_SEPARATOR, u"UPPER_SEPARATOR"},
    {ui::MenuSeparatorType::LOWER_SEPARATOR, u"LOWER_SEPARATOR"},
    {ui::MenuSeparatorType::SPACING_SEPARATOR, u"SPACING_SEPARATOR"},
    {ui::MenuSeparatorType::VERTICAL_SEPARATOR, u"VERTICAL_SEPARATOR"},
    {ui::MenuSeparatorType::PADDED_SEPARATOR, u"PADDED_SEPARATOR"})

DEFINE_ENUM_CONVERTERS(ui::ButtonStyle,
                       {ui::ButtonStyle::kDefault, u"kDefault"},
                       {ui::ButtonStyle::kProminent, u"kProminent"},
                       {ui::ButtonStyle::kTonal, u"kTonal"},
                       {ui::ButtonStyle::kText, u"kText"})
