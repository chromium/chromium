// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_TYPE_CONVERSION_H_
#define UI_VIEWS_METADATA_TYPE_CONVERSION_H_

#include <stdint.h>

#include <algorithm>  // Silence broken lint check
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/views_export.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace views {
namespace metadata {

using ValidStrings = std::vector<base::string16>;

// Various metadata methods pass types either by value or const ref depending on
// whether the types are "small" (defined as "fundamental, enum, or pointer").
// ArgType<T> gives the appropriate type to use as an argument in such cases.
template <typename T>
using ArgType =
    typename std::conditional<std::is_fundamental<T>::value ||
                                  std::is_enum<T>::value ||
                                  std::is_pointer<T>::value ||
                                  (std::is_move_assignable<T>::value &&
                                   std::is_move_constructible<T>::value &&
                                   !std::is_copy_assignable<T>::value &&
                                   !std::is_copy_constructible<T>::value),
                              T,
                              const T&>::type;

VIEWS_EXPORT extern const char kNoPrefix[];
VIEWS_EXPORT extern const char kSkColorPrefix[];

// General Type Conversion Template Functions ---------------------------------
template <bool serializable,
          bool read_only = false,
          const char* name_prefix = kNoPrefix>
struct BaseTypeConverter {
  static constexpr bool is_serializable = serializable;
  static constexpr bool is_read_only = read_only;
  static bool IsSerializable() { return is_serializable; }
  static bool IsReadOnly() { return is_read_only; }
  static const char* PropertyNamePrefix() { return name_prefix; }
};

template <typename T>
struct TypeConverter : BaseTypeConverter<std::is_enum<T>::value> {
  static base::string16 ToString(ArgType<T> source_value);
  static base::Optional<T> FromString(const base::string16& source_value);
  static ValidStrings GetValidStrings();
};

// The following definitions and macros are needed only in cases where a type
// is a mere alias to a POD type AND a specialized type converter is also needed
// to handle different the string conversions different from the existing POD
// type converter. See SkColor below as an example of their use.
// NOTE: This should be a rare occurrence and if possible use a unique type and
// a TypeConverter specialization based on that unique type.

template <typename T, typename K>
struct Uniquifier {
  using type = T;
  using tag = K;
};

#define MAKE_TYPE_UNIQUE(type_name) \
  struct type_name##Tag {};         \
  using type_name##Unique =         \
      ::views::metadata::Uniquifier<type_name, type_name##Tag>

#define _UNIQUE_TYPE_NAME1(type_name) type_name##Unique

#define _UNIQUE_TYPE_NAME2(qualifier, type_name) qualifier::type_name##Unique

#define _GET_TYPE_MACRO(_1, _2, NAME, ...) NAME

#define UNIQUE_TYPE_NAME(name, ...)                                            \
  _GET_TYPE_MACRO(name, ##__VA_ARGS__, _UNIQUE_TYPE_NAME2, _UNIQUE_TYPE_NAME1) \
  (name, ##__VA_ARGS__)

// Types and macros for generating enum converters ----------------------------
template <typename T>
struct EnumStrings {
  struct EnumString {
    T enum_value;
    base::string16 str_value;
  };

  explicit EnumStrings(std::vector<EnumString> init_val)
      : pairs(std::move(init_val)) {}

  ValidStrings GetStringValues() const {
    ValidStrings string_values;
    for (const auto& pair : pairs)
      string_values.push_back(pair.str_value);
    return string_values;
  }

  const std::vector<EnumString> pairs;
};

template <typename T>
static const EnumStrings<T>& GetEnumStringsInstance();

// Generate the code to define a enum type to and from base::string16
// conversions. The first argument is the type T, and the rest of the argument
// should have the enum value and string pairs defined in a format like
// "{enum_value0, string16_value0}, {enum_value1, string16_value1} ...".
#define DEFINE_ENUM_CONVERTERS(T, ...)                             \
  template <>                                                      \
  const views::metadata::EnumStrings<T>&                           \
  views::metadata::GetEnumStringsInstance<T>() {                   \
    static const base::NoDestructor<EnumStrings<T>> instance(      \
        std::vector<views::metadata::EnumStrings<T>::EnumString>(  \
            {__VA_ARGS__}));                                       \
    return *instance;                                              \
  }                                                                \
                                                                   \
  template <>                                                      \
  base::string16 views::metadata::TypeConverter<T>::ToString(      \
      ArgType<T> source_value) {                                   \
    for (const auto& pair : GetEnumStringsInstance<T>().pairs) {   \
      if (source_value == pair.enum_value)                         \
        return pair.str_value;                                     \
    }                                                              \
    return base::string16();                                       \
  }                                                                \
                                                                   \
  template <>                                                      \
  base::Optional<T> views::metadata::TypeConverter<T>::FromString( \
      const base::string16& source_value) {                        \
    for (const auto& pair : GetEnumStringsInstance<T>().pairs) {   \
      if (source_value == pair.str_value) {                        \
        return pair.enum_value;                                    \
      }                                                            \
    }                                                              \
    return base::nullopt;                                          \
  }                                                                \
                                                                   \
  template <>                                                      \
  views::metadata::ValidStrings                                    \
  views::metadata::TypeConverter<T>::GetValidStrings() {           \
    return GetEnumStringsInstance<T>().GetStringValues();          \
  }

// String Conversions ---------------------------------------------------------

VIEWS_EXPORT base::string16 PointerToString(const void* pointer_val);

#define DECLARE_CONVERSIONS(T)                                               \
  template <>                                                                \
  struct VIEWS_EXPORT TypeConverter<T> : BaseTypeConverter<true> {           \
    static base::string16 ToString(ArgType<T> source_value);                 \
    static base::Optional<T> FromString(const base::string16& source_value); \
    static ValidStrings GetValidStrings() { return {}; }                     \
  };

DECLARE_CONVERSIONS(int8_t)
DECLARE_CONVERSIONS(int16_t)
DECLARE_CONVERSIONS(int32_t)
DECLARE_CONVERSIONS(int64_t)
DECLARE_CONVERSIONS(uint8_t)
DECLARE_CONVERSIONS(uint16_t)
DECLARE_CONVERSIONS(uint32_t)
DECLARE_CONVERSIONS(uint64_t)
DECLARE_CONVERSIONS(float)
DECLARE_CONVERSIONS(double)
DECLARE_CONVERSIONS(const char*)
DECLARE_CONVERSIONS(base::FilePath)
DECLARE_CONVERSIONS(base::string16)
DECLARE_CONVERSIONS(base::TimeDelta)
DECLARE_CONVERSIONS(gfx::Insets)
DECLARE_CONVERSIONS(gfx::Point)
DECLARE_CONVERSIONS(gfx::PointF)
DECLARE_CONVERSIONS(gfx::Range)
DECLARE_CONVERSIONS(gfx::Rect)
DECLARE_CONVERSIONS(gfx::RectF)
DECLARE_CONVERSIONS(gfx::ShadowValues)
DECLARE_CONVERSIONS(gfx::Size)
DECLARE_CONVERSIONS(gfx::SizeF)
DECLARE_CONVERSIONS(GURL)
DECLARE_CONVERSIONS(url::Component)

#undef DECLARE_CONVERSIONS

template <>
struct VIEWS_EXPORT TypeConverter<bool> : BaseTypeConverter<true> {
  static base::string16 ToString(bool source_value);
  static base::Optional<bool> FromString(const base::string16& source_value);
  static ValidStrings GetValidStrings();
};

// Special conversions for wrapper types --------------------------------------

VIEWS_EXPORT const base::string16& GetNullOptStr();

template <typename T>
struct TypeConverter<base::Optional<T>>
    : BaseTypeConverter<TypeConverter<T>::is_serializable> {
  static base::string16 ToString(ArgType<base::Optional<T>> source_value) {
    if (!source_value)
      return GetNullOptStr();
    return TypeConverter<T>::ToString(source_value.value());
  }
  static base::Optional<base::Optional<T>> FromString(
      const base::string16& source_value) {
    if (source_value == GetNullOptStr())
      return base::make_optional<base::Optional<T>>(base::nullopt);

    auto ret = TypeConverter<T>::FromString(source_value);
    return ret ? base::make_optional(ret) : base::nullopt;
  }
  static ValidStrings GetValidStrings() { return {}; }
};

// Special Conversions for std:unique_ptr<T> and T* types ----------------------

template <typename T>
struct TypeConverter<std::unique_ptr<T>> : BaseTypeConverter<false, true> {
  static base::string16 ToString(const std::unique_ptr<T>& source_value) {
    return PointerToString(source_value.get());
  }
  static base::string16 ToString(const T* source_value) {
    return PointerToString(source_value);
  }
  static base::Optional<std::unique_ptr<T>> FromString(
      const base::string16& source_value) {
    DCHECK(false) << "Type converter cannot convert from string.";
    return base::nullopt;
  }
  static ValidStrings GetValidStrings() { return {}; }
};

template <typename T>
struct TypeConverter<T*> : BaseTypeConverter<false, true> {
  static base::string16 ToString(ArgType<T*> source_value) {
    return PointerToString(source_value);
  }
  static base::Optional<T*> FromString(const base::string16& source_value) {
    DCHECK(false) << "Type converter cannot convert from string.";
    return base::nullopt;
  }
  static ValidStrings GetValidStrings() { return {}; }
};

template <typename T>
struct TypeConverter<std::vector<T>>
    : BaseTypeConverter<TypeConverter<T>::is_serializable> {
  static base::string16 ToString(ArgType<std::vector<T>> source_value) {
    std::vector<base::string16> serialized;
    base::ranges::transform(source_value, std::back_inserter(serialized),
                            &TypeConverter<T>::ToString);
    return STRING16_LITERAL("{") +
           base::JoinString(serialized, STRING16_LITERAL(",")) +
           STRING16_LITERAL("}");
  }
  static base::Optional<std::vector<T>> FromString(
      const base::string16& source_value) {
    if (source_value.empty() || source_value.front() != STRING16_LITERAL('{') ||
        source_value.back() != STRING16_LITERAL('}'))
      return base::nullopt;
    const auto values = base::SplitString(
        source_value.substr(1, source_value.length() - 2),
        base::ASCIIToUTF16(","), base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::vector<T> output;
    for (const auto& value : values) {
      auto ret = TypeConverter<T>::FromString(value);
      if (!ret)
        return base::nullopt;
      output.push_back(*ret);
    }
    return base::make_optional(output);
  }
  static ValidStrings GetValidStrings() { return {}; }
};

MAKE_TYPE_UNIQUE(SkColor);

template <>
struct VIEWS_EXPORT TypeConverter<UNIQUE_TYPE_NAME(SkColor)>
    : BaseTypeConverter<true, false, kSkColorPrefix> {
  static base::string16 ToString(SkColor source_value);
  static base::Optional<SkColor> FromString(const base::string16& source_value);
  static ValidStrings GetValidStrings();

  // Parses a string within |start| and |end| for a color string in the forms
  // rgb(r, g, b), rgba(r, g, b, a), hsl(h, s%, l%), hsla(h, s%, l%, a),
  // 0xXXXXXX, 0xXXXXXXXX, <decimal number>
  // Returns the full string in |color| and the position immediately following
  // the last token in |next_token|.
  // Returns false if the input string cannot be properly parsed. |color| and
  // |next_token| will be undefined.
  static bool GetNextColor(base::string16::const_iterator start,
                           base::string16::const_iterator end,
                           base::string16& color,
                           base::string16::const_iterator& next_token);
  static bool GetNextColor(base::string16::const_iterator start,
                           base::string16::const_iterator end,
                           base::string16& color);

  // Same as above, except returns the color string converted into an |SkColor|.
  // Returns base::nullopt if the color string cannot be properly parsed or the
  // string cannot be converted into a valid SkColor and |next_token| may be
  // undefined.
  static base::Optional<SkColor> GetNextColor(
      base::string16::const_iterator start,
      base::string16::const_iterator end,
      base::string16::const_iterator& next_token);
  static base::Optional<SkColor> GetNextColor(
      base::string16::const_iterator start,
      base::string16::const_iterator end);

  // Converts the four elements of |pieces| beginning at |start_piece| to an
  // SkColor by assuming the pieces are split from a string like
  // "rgba(r,g,b,a)". Returns nullopt if conversion was unsuccessful.
  static base::Optional<SkColor> RgbaPiecesToSkColor(
      const std::vector<base::StringPiece16>& pieces,
      size_t start_piece);

 private:
  static base::Optional<SkColor> ParseHexString(
      const base::string16& hex_string);
  static base::Optional<SkColor> ParseHslString(
      const base::string16& hsl_string);
  static base::Optional<SkColor> ParseRgbString(
      const base::string16& rgb_string);
};

using SkColorConverter = TypeConverter<UNIQUE_TYPE_NAME(SkColor)>;

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_TYPE_CONVERSION_H_
