// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_BASE_TYPE_CONVERSION_H_
#define UI_BASE_METADATA_BASE_TYPE_CONVERSION_H_

#include <stdint.h>

#include <algorithm>
#include <cinttypes>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_variant.h"
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
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace ui {
namespace metadata {

using ValidStrings = std::vector<std::u16string>;

// Various metadata methods pass types either by value or const ref depending on
// whether the types are "small" (defined as "fundamental, enum, or pointer").
// ArgType<T> gives the appropriate type to use as an argument in such cases.
template <typename T>
using ArgType =
    std::conditional_t<std::is_fundamental_v<T> || std::is_enum_v<T> ||
                           std::is_pointer_v<T> ||
                           (std::movable<T> && !std::copyable<T>),
                       T,
                       const T&>;

// General Type Conversion Template Functions ---------------------------------
template <bool serializable,
          bool read_only = false,
          const char* name_prefix = nullptr>
struct BaseTypeConverter {
  static constexpr bool is_serializable = serializable;
  static constexpr bool is_read_only = read_only;
  static constexpr std::string_view property_name_prefix = requires {
    requires(!!name_prefix);
  } ? std::string_view(name_prefix) : std::string_view();
};

template <typename T>
struct TypeConverter : BaseTypeConverter<std::is_enum_v<T>> {
  static std::u16string ToString(ArgType<T> source_value) {
    return u"<unknown>";
  }
  static std::optional<T> FromString(const std::u16string& source_value) {
    NOTREACHED();
  }
  static ValidStrings GetValidStrings() { return {}; }
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
      ::ui::metadata::Uniquifier<type_name, type_name##Tag>

#define _UNIQUE_TYPE_NAME1(type_name) type_name##Unique

#define _UNIQUE_TYPE_NAME2(qualifier, type_name) qualifier::type_name##Unique

#define _GET_TYPE_MACRO(_1, _2, NAME, ...) NAME

#define UNIQUE_TYPE_NAME(name, ...)                                            \
  _GET_TYPE_MACRO(name, ##__VA_ARGS__, _UNIQUE_TYPE_NAME2, _UNIQUE_TYPE_NAME1) \
  (name, ##__VA_ARGS__)

// Types and macros for generating enum converters ----------------------------
template <typename T>
struct EnumStringsMap;

// *****************************************************************
// * NOTE: The ENUM macros *must* be placed outside any namespace. *
// *****************************************************************
//
// Use this macro only if your enum converters need to be accessible across
// modules. Place this in the header file for the module and use the other macro
// below as described.
//
#define EXPORT_ENUM_CONVERTERS(T, EXPORT)                             \
  template <>                                                         \
  EXPORT std::u16string ui::metadata::TypeConverter<T>::ToString(     \
      ui::metadata::ArgType<T> source_value);                         \
                                                                      \
  template <>                                                         \
  EXPORT std::optional<T> ui::metadata::TypeConverter<T>::FromString( \
      const std::u16string& str);                                     \
                                                                      \
  template <>                                                         \
  EXPORT ui::metadata::ValidStrings                                   \
  ui::metadata::TypeConverter<T>::GetValidStrings();

// Generate the code to define a enum type to and from std::u16string
// conversions. The first argument is the type T, and the rest of the argument
// should have the enum value and string pairs defined in a format like
// "{enum_value0, string16_value0}, {enum_value1, string16_value1} ...".
// Both enum_values and string16_values need to be compile time constants.
//
#define DEFINE_ENUM_CONVERTERS(T, ...)                                      \
  template <>                                                               \
  struct ui::metadata::EnumStringsMap<T> {                                  \
    static_assert(std::is_enum_v<T>, "Error: " #T " is not an enum.");      \
                                                                            \
    static const auto& Get() {                                              \
      static constexpr auto kMap =                                          \
          base::MakeFixedFlatMap<T, std::u16string_view>({__VA_ARGS__});    \
      return kMap;                                                          \
    }                                                                       \
  };                                                                        \
                                                                            \
  template <>                                                               \
  std::u16string ui::metadata::TypeConverter<T>::ToString(                  \
      ui::metadata::ArgType<T> source_value) {                              \
    const auto& map = EnumStringsMap<T>::Get();                             \
    auto it = map.find(source_value);                                       \
    return it != map.end() ? std::u16string(it->second) : std::u16string(); \
  }                                                                         \
                                                                            \
  template <>                                                               \
  std::optional<T> ui::metadata::TypeConverter<T>::FromString(              \
      const std::u16string& str) {                                          \
    const auto& map = EnumStringsMap<T>::Get();                             \
    using Pair = std::ranges::range_value_t<decltype(map)>;                 \
    auto it = std::ranges::find(map, str, &Pair::second);                   \
    return it != map.end() ? std::make_optional(it->first) : std::nullopt;  \
  }                                                                         \
                                                                            \
  template <>                                                               \
  ui::metadata::ValidStrings                                                \
  ui::metadata::TypeConverter<T>::GetValidStrings() {                       \
    ValidStrings string_values;                                             \
    std::ranges::transform(                                                 \
        EnumStringsMap<T>::Get(), std::back_inserter(string_values),        \
        [](const auto& pair) { return std::u16string(pair.second); });      \
    return string_values;                                                   \
  }

// String Conversions ---------------------------------------------------------

template <typename T>
std::u16string PointerToString(T* ptr);

template <>
struct COMPONENT_EXPORT(UI_BASE_METADATA) TypeConverter<bool>
    : BaseTypeConverter<true> {
  static std::u16string ToString(bool source_value);
  static std::optional<bool> FromString(const std::u16string& source_value);
  static ValidStrings GetValidStrings();
};

template <>
struct COMPONENT_EXPORT(UI_BASE_METADATA) TypeConverter<const char*>
    : BaseTypeConverter<true, true> {
  static std::u16string ToString(const char* source_value);
};

template <>
struct COMPONENT_EXPORT(UI_BASE_METADATA) TypeConverter<std::string_view>
    : BaseTypeConverter<true, true> {
  static std::u16string ToString(std::string_view source_value);
};

#define DECLARE_CONVERSIONS(T)                                              \
  template <>                                                               \
  struct COMPONENT_EXPORT(UI_BASE_METADATA)                                 \
      TypeConverter<T> : BaseTypeConverter<true> {                          \
    static std::u16string ToString(ArgType<T> source_value);                \
    static std::optional<T> FromString(const std::u16string& source_value); \
    static ValidStrings GetValidStrings() {                                 \
      return {};                                                            \
    }                                                                       \
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
DECLARE_CONVERSIONS(GURL)
DECLARE_CONVERSIONS(base::FilePath)
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
DECLARE_CONVERSIONS(std::string)
DECLARE_CONVERSIONS(std::u16string)
DECLARE_CONVERSIONS(ui::ColorVariant)
DECLARE_CONVERSIONS(url::Component)

#undef DECLARE_CONVERSIONS

// Special conversions for wrapper types --------------------------------------

inline constexpr char16_t kNullOptStr[] = u"<Empty>";

template <typename T>
struct TypeConverter<std::optional<T>>
    : BaseTypeConverter<TypeConverter<T>::is_serializable> {
  static std::u16string ToString(ArgType<std::optional<T>> source_value) {
    return source_value ? TypeConverter<T>::ToString(source_value.value())
                        : std::u16string(kNullOptStr);
  }
  static std::optional<std::optional<T>> FromString(
      const std::u16string& source_value) {
    if (source_value == kNullOptStr) {
      return std::make_optional<std::optional<T>>(std::nullopt);
    }

    auto ret = TypeConverter<T>::FromString(source_value);
    return ret ? std::make_optional(ret) : std::nullopt;
  }
  static ValidStrings GetValidStrings() { return {}; }
};

// Special Conversions for std:unique_ptr<T> and T* types ----------------------

template <typename T>
struct TypeConverter<std::unique_ptr<T>> : BaseTypeConverter<false, true> {
  static std::u16string ToString(const std::unique_ptr<T>& source_value) {
    return PointerToString(source_value.get());
  }
  static std::u16string ToString(const T* source_value) {
    return PointerToString(source_value);
  }
  static std::optional<std::unique_ptr<T>> FromString(
      const std::u16string& source_value) {
    DCHECK(false) << "Type converter cannot convert from string.";
    return std::nullopt;
  }
  static ValidStrings GetValidStrings() { return {}; }
};

template <typename T>
struct TypeConverter<T*> : BaseTypeConverter<false, true> {
  static std::u16string ToString(T* source_value) {
    return PointerToString(source_value);
  }
  static std::optional<T*> FromString(const std::u16string& source_value) {
    DCHECK(false) << "Type converter cannot convert from string.";
    return std::nullopt;
  }
  static ValidStrings GetValidStrings() { return {}; }
};

template <typename T>
struct TypeConverter<std::vector<T>>
    : BaseTypeConverter<TypeConverter<T>::is_serializable> {
  static std::u16string ToString(ArgType<std::vector<T>> source_value) {
    std::vector<std::u16string> serialized;
    std::ranges::transform(source_value, std::back_inserter(serialized),
                           &TypeConverter<T>::ToString);
    return u"{" + base::JoinString(serialized, u",") + u"}";
  }
  static std::optional<std::vector<T>> FromString(
      const std::u16string& source_value) {
    if (source_value.empty() || source_value.front() != u'{' ||
        source_value.back() != u'}') {
      return std::nullopt;
    }
    const auto values =
        base::SplitString(source_value.substr(1, source_value.length() - 2),
                          u",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::vector<T> output;
    for (const auto& value : values) {
      if (auto ret = TypeConverter<T>::FromString(value)) {
        output.push_back(*std::move(ret));
      } else {
        return std::nullopt;
      }
    }
    return std::make_optional(output);
  }
  static ValidStrings GetValidStrings() { return {}; }
};

MAKE_TYPE_UNIQUE(SkColor);

inline constexpr char kSkColorPrefix[] = "--";
template <>
struct COMPONENT_EXPORT(UI_BASE_METADATA)
    TypeConverter<UNIQUE_TYPE_NAME(SkColor)>
    : BaseTypeConverter<true, false, kSkColorPrefix> {
  static std::u16string ToString(SkColor source_value);
  static std::optional<SkColor> FromString(const std::u16string& source_value);
  static ValidStrings GetValidStrings();

  // Parses a string within |start| and |end| for a color string in the forms
  // rgb(r, g, b), rgba(r, g, b, a), hsl(h, s%, l%), hsla(h, s%, l%, a),
  // 0xXXXXXX, 0xXXXXXXXX, <decimal number>
  // Returns the full string in |color| and the position immediately following
  // the last token in |next_token|.
  // Returns false if the input string cannot be properly parsed. |color| and
  // |next_token| will be undefined.
  static bool GetNextColor(std::u16string::const_iterator start,
                           std::u16string::const_iterator end,
                           std::u16string& color,
                           std::u16string::const_iterator& next_token);
  static bool GetNextColor(std::u16string::const_iterator start,
                           std::u16string::const_iterator end,
                           std::u16string& color);

  // Same as above, except returns the color string converted into an |SkColor|.
  // Returns std::nullopt if the color string cannot be properly parsed or the
  // string cannot be converted into a valid SkColor and |next_token| may be
  // undefined.
  static std::optional<SkColor> GetNextColor(
      std::u16string::const_iterator start,
      std::u16string::const_iterator end,
      std::u16string::const_iterator& next_token);
  static std::optional<SkColor> GetNextColor(
      std::u16string::const_iterator start,
      std::u16string::const_iterator end);

  // Converts the four elements of |pieces| beginning at |start_piece| to an
  // SkColor by assuming the pieces are split from a string like
  // "rgba(r,g,b,a)". Returns nullopt if conversion was unsuccessful.
  static std::optional<SkColor> RgbaPiecesToSkColor(
      const std::vector<std::u16string_view>& pieces,
      size_t start_piece);

 private:
  static std::optional<SkColor> ParseHexString(
      const std::u16string& hex_string);
  static std::optional<SkColor> ParseHslString(
      const std::u16string& hsl_string);
  static std::optional<SkColor> ParseRgbString(
      const std::u16string& rgb_string);
};

using SkColorConverter = TypeConverter<UNIQUE_TYPE_NAME(SkColor)>;

// Defined after the type converters to avoid using specializations before
// they're declared.
template <typename T>
std::u16string PointerToString(T* ptr) {
  if (!ptr) {
    return u"<null>";
  }
  if constexpr (std::is_void_v<T> ||
                !TypeConverter<std::remove_const_t<T>>::is_serializable) {
    return base::ASCIIToUTF16(
        base::StringPrintf("<opaque object at 0x%016" PRIxPTR ">",
                           reinterpret_cast<uintptr_t>(ptr)));
  } else {
    return base::StrCat(
        {u"&{", TypeConverter<std::remove_const_t<T>>::ToString(*ptr), u"}"});
  }
}

template <typename T>
concept ClassHasToString = requires(T t) {
  { t.ToString() } -> std::same_as<std::string>;
};

template <typename T>
concept ClassHasFromString = requires(T t, std::string_view sv) {
  { T::FromString(sv) } -> std::same_as<std::optional<T>>;
};

template <ClassHasToString T>
struct TypeConverter<T>
    : BaseTypeConverter<ClassHasFromString<T>, !ClassHasFromString<T>> {
  static std::u16string ToString(ui::metadata::ArgType<T> source_value) {
    return base::UTF8ToUTF16(source_value.ToString());
  }

  static std::optional<T> FromString(const std::u16string& source_value) {
    if constexpr (ClassHasFromString<T>) {
      return T::FromString(base::UTF16ToUTF8(source_value));
    }

    return std::nullopt;
  }

  static ValidStrings GetValidStrings() { return {}; }
};

}  // namespace metadata
}  // namespace ui

EXPORT_ENUM_CONVERTERS(gfx::ElideBehavior, COMPONENT_EXPORT(UI_BASE_METADATA))
EXPORT_ENUM_CONVERTERS(gfx::HorizontalAlignment,
                       COMPONENT_EXPORT(UI_BASE_METADATA))
EXPORT_ENUM_CONVERTERS(gfx::VerticalAlignment,
                       COMPONENT_EXPORT(UI_BASE_METADATA))
EXPORT_ENUM_CONVERTERS(ui::ButtonStyle, COMPONENT_EXPORT(UI_BASE_METADATA))
EXPORT_ENUM_CONVERTERS(ui::MenuSeparatorType,
                       COMPONENT_EXPORT(UI_BASE_METADATA))
EXPORT_ENUM_CONVERTERS(ui::TextInputType, COMPONENT_EXPORT(UI_BASE_METADATA))

#endif  // UI_BASE_METADATA_BASE_TYPE_CONVERSION_H_
