#ifndef icu4x_Locale_HPP
#define icu4x_Locale_HPP

#include "Locale.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "LocaleParseError.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_Locale_from_string_mv1_result {union {icu4x::capi::Locale* ok; icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_from_string_mv1_result;
    icu4x_Locale_from_string_mv1_result icu4x_Locale_from_string_mv1(diplomat::capi::DiplomatStringView name);
    
    icu4x::capi::Locale* icu4x_Locale_und_mv1(void);
    
    icu4x::capi::Locale* icu4x_Locale_clone_mv1(const icu4x::capi::Locale* self);
    
    void icu4x_Locale_basename_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_Locale_get_unicode_extension_mv1_result { bool is_ok;} icu4x_Locale_get_unicode_extension_mv1_result;
    icu4x_Locale_get_unicode_extension_mv1_result icu4x_Locale_get_unicode_extension_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatStringView s, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_Locale_language_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_Locale_set_language_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_set_language_mv1_result;
    icu4x_Locale_set_language_mv1_result icu4x_Locale_set_language_mv1(icu4x::capi::Locale* self, diplomat::capi::DiplomatStringView s);
    
    typedef struct icu4x_Locale_region_mv1_result { bool is_ok;} icu4x_Locale_region_mv1_result;
    icu4x_Locale_region_mv1_result icu4x_Locale_region_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_Locale_set_region_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_set_region_mv1_result;
    icu4x_Locale_set_region_mv1_result icu4x_Locale_set_region_mv1(icu4x::capi::Locale* self, diplomat::capi::DiplomatStringView s);
    
    typedef struct icu4x_Locale_script_mv1_result { bool is_ok;} icu4x_Locale_script_mv1_result;
    icu4x_Locale_script_mv1_result icu4x_Locale_script_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatWrite* write);
    
    typedef struct icu4x_Locale_set_script_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_set_script_mv1_result;
    icu4x_Locale_set_script_mv1_result icu4x_Locale_set_script_mv1(icu4x::capi::Locale* self, diplomat::capi::DiplomatStringView s);
    
    typedef struct icu4x_Locale_normalize_mv1_result {union { icu4x::capi::LocaleParseError err;}; bool is_ok;} icu4x_Locale_normalize_mv1_result;
    icu4x_Locale_normalize_mv1_result icu4x_Locale_normalize_mv1(diplomat::capi::DiplomatStringView s, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_Locale_to_string_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatWrite* write);
    
    bool icu4x_Locale_normalizing_eq_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatStringView other);
    
    int8_t icu4x_Locale_compare_to_string_mv1(const icu4x::capi::Locale* self, diplomat::capi::DiplomatStringView other);
    
    int8_t icu4x_Locale_compare_to_mv1(const icu4x::capi::Locale* self, const icu4x::capi::Locale* other);
    
    
    void icu4x_Locale_destroy_mv1(Locale* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError> icu4x::Locale::from_string(std::string_view name) {
  auto result = icu4x::capi::icu4x_Locale_from_string_mv1({name.data(), name.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError>(diplomat::Ok<std::unique_ptr<icu4x::Locale>>(std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::Locale>, icu4x::LocaleParseError>(diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Locale> icu4x::Locale::und() {
  auto result = icu4x::capi::icu4x_Locale_und_mv1();
  return std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result));
}

inline std::unique_ptr<icu4x::Locale> icu4x::Locale::clone() const {
  auto result = icu4x::capi::icu4x_Locale_clone_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::Locale>(icu4x::Locale::FromFFI(result));
}

inline std::string icu4x::Locale::basename() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_Locale_basename_mv1(this->AsFFI(),
    &write);
  return output;
}

inline std::optional<std::string> icu4x::Locale::get_unicode_extension(std::string_view s) const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_Locale_get_unicode_extension_mv1(this->AsFFI(),
    {s.data(), s.size()},
    &write);
  return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}

inline std::string icu4x::Locale::language() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_Locale_language_mv1(this->AsFFI(),
    &write);
  return output;
}

inline diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::set_language(std::string_view s) {
  auto result = icu4x::capi::icu4x_Locale_set_language_mv1(this->AsFFI(),
    {s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::monostate, icu4x::LocaleParseError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, icu4x::LocaleParseError>(diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::optional<std::string> icu4x::Locale::region() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_Locale_region_mv1(this->AsFFI(),
    &write);
  return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}

inline diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::set_region(std::string_view s) {
  auto result = icu4x::capi::icu4x_Locale_set_region_mv1(this->AsFFI(),
    {s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::monostate, icu4x::LocaleParseError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, icu4x::LocaleParseError>(diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::optional<std::string> icu4x::Locale::script() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_Locale_script_mv1(this->AsFFI(),
    &write);
  return result.is_ok ? std::optional<std::string>(std::move(output)) : std::nullopt;
}

inline diplomat::result<std::monostate, icu4x::LocaleParseError> icu4x::Locale::set_script(std::string_view s) {
  auto result = icu4x::capi::icu4x_Locale_set_script_mv1(this->AsFFI(),
    {s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::monostate, icu4x::LocaleParseError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, icu4x::LocaleParseError>(diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline diplomat::result<std::string, icu4x::LocaleParseError> icu4x::Locale::normalize(std::string_view s) {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  auto result = icu4x::capi::icu4x_Locale_normalize_mv1({s.data(), s.size()},
    &write);
  return result.is_ok ? diplomat::result<std::string, icu4x::LocaleParseError>(diplomat::Ok<std::string>(std::move(output))) : diplomat::result<std::string, icu4x::LocaleParseError>(diplomat::Err<icu4x::LocaleParseError>(icu4x::LocaleParseError::FromFFI(result.err)));
}

inline std::string icu4x::Locale::to_string() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_Locale_to_string_mv1(this->AsFFI(),
    &write);
  return output;
}

inline bool icu4x::Locale::normalizing_eq(std::string_view other) const {
  auto result = icu4x::capi::icu4x_Locale_normalizing_eq_mv1(this->AsFFI(),
    {other.data(), other.size()});
  return result;
}

inline int8_t icu4x::Locale::compare_to_string(std::string_view other) const {
  auto result = icu4x::capi::icu4x_Locale_compare_to_string_mv1(this->AsFFI(),
    {other.data(), other.size()});
  return result;
}

inline int8_t icu4x::Locale::compare_to(const icu4x::Locale& other) const {
  auto result = icu4x::capi::icu4x_Locale_compare_to_mv1(this->AsFFI(),
    other.AsFFI());
  return result;
}

inline const icu4x::capi::Locale* icu4x::Locale::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::Locale*>(this);
}

inline icu4x::capi::Locale* icu4x::Locale::AsFFI() {
  return reinterpret_cast<icu4x::capi::Locale*>(this);
}

inline const icu4x::Locale* icu4x::Locale::FromFFI(const icu4x::capi::Locale* ptr) {
  return reinterpret_cast<const icu4x::Locale*>(ptr);
}

inline icu4x::Locale* icu4x::Locale::FromFFI(icu4x::capi::Locale* ptr) {
  return reinterpret_cast<icu4x::Locale*>(ptr);
}

inline void icu4x::Locale::operator delete(void* ptr) {
  icu4x::capi::icu4x_Locale_destroy_mv1(reinterpret_cast<icu4x::capi::Locale*>(ptr));
}


#endif // icu4x_Locale_HPP
