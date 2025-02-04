#ifndef icu4x_CodePointMapData8_HPP
#define icu4x_CodePointMapData8_HPP

#include "CodePointMapData8.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "CodePointRangeIterator.hpp"
#include "CodePointSetData.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    uint8_t icu4x_CodePointMapData8_get_mv1(const icu4x::capi::CodePointMapData8* self, char32_t cp);
    
    uint32_t icu4x_CodePointMapData8_general_category_to_mask_mv1(uint8_t gc);
    
    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_value_mv1(const icu4x::capi::CodePointMapData8* self, uint8_t value);
    
    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_value_complemented_mv1(const icu4x::capi::CodePointMapData8* self, uint8_t value);
    
    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_mask_mv1(const icu4x::capi::CodePointMapData8* self, uint32_t mask);
    
    icu4x::capi::CodePointSetData* icu4x_CodePointMapData8_get_set_for_value_mv1(const icu4x::capi::CodePointMapData8* self, uint8_t value);
    
    typedef struct icu4x_CodePointMapData8_load_general_category_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_general_category_mv1_result;
    icu4x_CodePointMapData8_load_general_category_mv1_result icu4x_CodePointMapData8_load_general_category_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_bidi_class_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_bidi_class_mv1_result;
    icu4x_CodePointMapData8_load_bidi_class_mv1_result icu4x_CodePointMapData8_load_bidi_class_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_east_asian_width_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_east_asian_width_mv1_result;
    icu4x_CodePointMapData8_load_east_asian_width_mv1_result icu4x_CodePointMapData8_load_east_asian_width_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_hangul_syllable_type_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_hangul_syllable_type_mv1_result;
    icu4x_CodePointMapData8_load_hangul_syllable_type_mv1_result icu4x_CodePointMapData8_load_hangul_syllable_type_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_indic_syllabic_category_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_indic_syllabic_category_mv1_result;
    icu4x_CodePointMapData8_load_indic_syllabic_category_mv1_result icu4x_CodePointMapData8_load_indic_syllabic_category_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_line_break_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_line_break_mv1_result;
    icu4x_CodePointMapData8_load_line_break_mv1_result icu4x_CodePointMapData8_load_line_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1_result;
    icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1_result icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_word_break_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_word_break_mv1_result;
    icu4x_CodePointMapData8_load_word_break_mv1_result icu4x_CodePointMapData8_load_word_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_sentence_break_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_sentence_break_mv1_result;
    icu4x_CodePointMapData8_load_sentence_break_mv1_result icu4x_CodePointMapData8_load_sentence_break_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_joining_type_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_joining_type_mv1_result;
    icu4x_CodePointMapData8_load_joining_type_mv1_result icu4x_CodePointMapData8_load_joining_type_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_CodePointMapData8_load_canonical_combining_class_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_load_canonical_combining_class_mv1_result;
    icu4x_CodePointMapData8_load_canonical_combining_class_mv1_result icu4x_CodePointMapData8_load_canonical_combining_class_mv1(const icu4x::capi::DataProvider* provider);
    
    
    void icu4x_CodePointMapData8_destroy_mv1(CodePointMapData8* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline uint8_t icu4x::CodePointMapData8::get(char32_t cp) const {
  auto result = icu4x::capi::icu4x_CodePointMapData8_get_mv1(this->AsFFI(),
    cp);
  return result;
}

inline uint32_t icu4x::CodePointMapData8::general_category_to_mask(uint8_t gc) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_general_category_to_mask_mv1(gc);
  return result;
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointMapData8::iter_ranges_for_value(uint8_t value) const {
  auto result = icu4x::capi::icu4x_CodePointMapData8_iter_ranges_for_value_mv1(this->AsFFI(),
    value);
  return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointMapData8::iter_ranges_for_value_complemented(uint8_t value) const {
  auto result = icu4x::capi::icu4x_CodePointMapData8_iter_ranges_for_value_complemented_mv1(this->AsFFI(),
    value);
  return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointMapData8::iter_ranges_for_mask(uint32_t mask) const {
  auto result = icu4x::capi::icu4x_CodePointMapData8_iter_ranges_for_mask_mv1(this->AsFFI(),
    mask);
  return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointMapData8::get_set_for_value(uint8_t value) const {
  auto result = icu4x::capi::icu4x_CodePointMapData8_get_set_for_value_mv1(this->AsFFI(),
    value);
  return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_general_category(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_general_category_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_bidi_class(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_bidi_class_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_east_asian_width(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_east_asian_width_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_hangul_syllable_type(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_hangul_syllable_type_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_indic_syllabic_category(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_indic_syllabic_category_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_line_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_line_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::try_grapheme_cluster_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_try_grapheme_cluster_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_word_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_word_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_sentence_break(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_sentence_break_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_joining_type(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_joining_type_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::load_canonical_combining_class(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_CodePointMapData8_load_canonical_combining_class_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::CodePointMapData8* icu4x::CodePointMapData8::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::CodePointMapData8*>(this);
}

inline icu4x::capi::CodePointMapData8* icu4x::CodePointMapData8::AsFFI() {
  return reinterpret_cast<icu4x::capi::CodePointMapData8*>(this);
}

inline const icu4x::CodePointMapData8* icu4x::CodePointMapData8::FromFFI(const icu4x::capi::CodePointMapData8* ptr) {
  return reinterpret_cast<const icu4x::CodePointMapData8*>(ptr);
}

inline icu4x::CodePointMapData8* icu4x::CodePointMapData8::FromFFI(icu4x::capi::CodePointMapData8* ptr) {
  return reinterpret_cast<icu4x::CodePointMapData8*>(ptr);
}

inline void icu4x::CodePointMapData8::operator delete(void* ptr) {
  icu4x::capi::icu4x_CodePointMapData8_destroy_mv1(reinterpret_cast<icu4x::capi::CodePointMapData8*>(ptr));
}


#endif // icu4x_CodePointMapData8_HPP
