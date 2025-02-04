#ifndef icu4x_PropertyValueNameToEnumMapper_HPP
#define icu4x_PropertyValueNameToEnumMapper_HPP

#include "PropertyValueNameToEnumMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    int16_t icu4x_PropertyValueNameToEnumMapper_get_strict_mv1(const icu4x::capi::PropertyValueNameToEnumMapper* self, diplomat::capi::DiplomatStringView name);
    
    int16_t icu4x_PropertyValueNameToEnumMapper_get_loose_mv1(const icu4x::capi::PropertyValueNameToEnumMapper* self, diplomat::capi::DiplomatStringView name);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_general_category_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_general_category_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_general_category_mv1_result icu4x_PropertyValueNameToEnumMapper_load_general_category_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_hangul_syllable_type_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_hangul_syllable_type_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_hangul_syllable_type_mv1_result icu4x_PropertyValueNameToEnumMapper_load_hangul_syllable_type_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_east_asian_width_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_east_asian_width_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_east_asian_width_mv1_result icu4x_PropertyValueNameToEnumMapper_load_east_asian_width_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_bidi_class_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_bidi_class_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_bidi_class_mv1_result icu4x_PropertyValueNameToEnumMapper_load_bidi_class_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_indic_syllabic_category_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_indic_syllabic_category_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_indic_syllabic_category_mv1_result icu4x_PropertyValueNameToEnumMapper_load_indic_syllabic_category_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_line_break_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_line_break_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_line_break_mv1_result icu4x_PropertyValueNameToEnumMapper_load_line_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_grapheme_cluster_break_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_grapheme_cluster_break_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_grapheme_cluster_break_mv1_result icu4x_PropertyValueNameToEnumMapper_load_grapheme_cluster_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_word_break_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_word_break_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_word_break_mv1_result icu4x_PropertyValueNameToEnumMapper_load_word_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_sentence_break_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_sentence_break_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_sentence_break_mv1_result icu4x_PropertyValueNameToEnumMapper_load_sentence_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_PropertyValueNameToEnumMapper_load_script_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_load_script_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_load_script_mv1_result icu4x_PropertyValueNameToEnumMapper_load_script_mv1(const icu4x::capi::DataProvider* provider);
    
    
    void icu4x_PropertyValueNameToEnumMapper_destroy_mv1(PropertyValueNameToEnumMapper* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline int16_t icu4x::PropertyValueNameToEnumMapper::get_strict(std::string_view name) const {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_get_strict_mv1(this->AsFFI(),
    {name.data(), name.size()});
  return result;
}

inline int16_t icu4x::PropertyValueNameToEnumMapper::get_loose(std::string_view name) const {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_get_loose_mv1(this->AsFFI(),
    {name.data(), name.size()});
  return result;
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_general_category(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_general_category_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_hangul_syllable_type(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_hangul_syllable_type_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_east_asian_width(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_east_asian_width_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_bidi_class(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_bidi_class_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_indic_syllabic_category(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_indic_syllabic_category_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_line_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_line_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_grapheme_cluster_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_grapheme_cluster_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_word_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_word_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_sentence_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_sentence_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::load_script(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_load_script_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::PropertyValueNameToEnumMapper* icu4x::PropertyValueNameToEnumMapper::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::PropertyValueNameToEnumMapper*>(this);
}

inline icu4x::capi::PropertyValueNameToEnumMapper* icu4x::PropertyValueNameToEnumMapper::AsFFI() {
  return reinterpret_cast<icu4x::capi::PropertyValueNameToEnumMapper*>(this);
}

inline const icu4x::PropertyValueNameToEnumMapper* icu4x::PropertyValueNameToEnumMapper::FromFFI(const icu4x::capi::PropertyValueNameToEnumMapper* ptr) {
  return reinterpret_cast<const icu4x::PropertyValueNameToEnumMapper*>(ptr);
}

inline icu4x::PropertyValueNameToEnumMapper* icu4x::PropertyValueNameToEnumMapper::FromFFI(icu4x::capi::PropertyValueNameToEnumMapper* ptr) {
  return reinterpret_cast<icu4x::PropertyValueNameToEnumMapper*>(ptr);
}

inline void icu4x::PropertyValueNameToEnumMapper::operator delete(void* ptr) {
  icu4x::capi::icu4x_PropertyValueNameToEnumMapper_destroy_mv1(reinterpret_cast<icu4x::capi::PropertyValueNameToEnumMapper*>(ptr));
}


#endif // icu4x_PropertyValueNameToEnumMapper_HPP
