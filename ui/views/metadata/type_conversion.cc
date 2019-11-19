// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
namespace metadata {

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
         base::ASCIIToUTF16(" s");
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
  return base::ASCIIToUTF16(base::StringPrintf("{%i, %i}", source_value.width(),
                                               source_value.height()));
}

base::string16 TypeConverter<gfx::Range>::ToString(
    const gfx::Range& source_value) {
  return base::ASCIIToUTF16(base::StringPrintf(
      "{%i, %i}", source_value.GetMin(), source_value.GetMax()));
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
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint8_t>(ret)) {
    return static_cast<uint8_t>(ret);
  }
  return base::nullopt;
}

base::Optional<uint16_t> TypeConverter<uint16_t>::FromString(
    const base::string16& source_value) {
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint16_t>(ret)) {
    return static_cast<uint16_t>(ret);
  }
  return base::nullopt;
}

base::Optional<uint32_t> TypeConverter<uint32_t>::FromString(
    const base::string16& source_value) {
  unsigned int value;
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
  if (!base::EndsWith(source_value, base::ASCIIToUTF16(" s"),
                      base::CompareCase::SENSITIVE))
    return base::nullopt;
  double ret;
  return base::StringToDouble(source_value.substr(0, source_value.length() - 2),
                              &ret)
             ? base::make_optional(base::TimeDelta::FromSecondsD(ret))
             : base::nullopt;
}

base::Optional<gfx::ShadowValues> TypeConverter<gfx::ShadowValues>::FromString(
    const base::string16& source_value) {
  gfx::ShadowValues ret;
  const auto shadow_value_strings =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("[;]"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto v : shadow_value_strings) {
    base::string16 member_string;
    base::RemoveChars(v.as_string(), base::ASCIIToUTF16("()rgba"),
                      &member_string);
    const auto members = base::SplitStringPiece(
        member_string, base::ASCIIToUTF16(","), base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    int x, y, r, g, b, a;
    double blur;

    if ((members.size() == 7) && base::StringToInt(members[0], &x) &&
        base::StringToInt(members[1], &y) &&
        base::StringToDouble(UTF16ToASCII(members[2]), &blur) &&
        base::StringToInt(members[3], &r) &&
        base::StringToInt(members[4], &g) &&
        base::StringToInt(members[5], &b) && base::StringToInt(members[6], &a))
      ret.emplace_back(gfx::Vector2d(x, y), blur, SkColorSetARGB(a, r, g, b));
  }
  return ret;
}

base::Optional<gfx::Size> TypeConverter<gfx::Size>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("{,}"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int width, height;
  if ((values.size() == 2) && base::StringToInt(values[0], &width) &&
      base::StringToInt(values[1], &height)) {
    return gfx::Size(width, height);
  }
  return base::nullopt;
}

base::Optional<gfx::Range> TypeConverter<gfx::Range>::FromString(
    const base::string16& source_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("{,}"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int min, max;
  if ((values.size() == 2) && base::StringToInt(values[0], &min) &&
      base::StringToInt(values[1], &max)) {
    return gfx::Range(min, max);
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
                       {ui::TextInputType::TEXT_INPUT_TYPE_MAX,
                        base::ASCIIToUTF16("TEXT_INPUT_TYPE_MAX")})
