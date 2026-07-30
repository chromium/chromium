#ifndef ICU4X_CodePointMapData8_HPP
#define ICU4X_CodePointMapData8_HPP

#include "CodePointMapData8.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointRangeIterator.hpp"
#include "CodePointSetData.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "GeneralCategoryGroup.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    uint8_t icu4x_CodePointMapData8_get_mv1(const icu4x::capi::CodePointMapData8* self, char32_t cp);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_value_mv1(const icu4x::capi::CodePointMapData8* self, uint8_t value);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_value_complemented_mv1(const icu4x::capi::CodePointMapData8* self, uint8_t value);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData8_iter_ranges_for_group_mv1(const icu4x::capi::CodePointMapData8* self, icu4x::capi::GeneralCategoryGroup group);

    icu4x::capi::CodePointSetData* icu4x_CodePointMapData8_get_set_for_value_mv1(const icu4x::capi::CodePointMapData8* self, uint8_t value);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_bidi_class_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_bidi_class_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_bidi_class_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_bidi_class_with_provider_mv1_result icu4x_CodePointMapData8_create_bidi_class_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_numeric_type_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_numeric_type_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_numeric_type_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_numeric_type_with_provider_mv1_result icu4x_CodePointMapData8_create_numeric_type_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_hangul_syllable_type_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_hangul_syllable_type_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_hangul_syllable_type_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_hangul_syllable_type_with_provider_mv1_result icu4x_CodePointMapData8_create_hangul_syllable_type_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_east_asian_width_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_east_asian_width_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_east_asian_width_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_east_asian_width_with_provider_mv1_result icu4x_CodePointMapData8_create_east_asian_width_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_line_break_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_line_break_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_line_break_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_line_break_with_provider_mv1_result icu4x_CodePointMapData8_create_line_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_grapheme_cluster_break_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_grapheme_cluster_break_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_grapheme_cluster_break_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_grapheme_cluster_break_with_provider_mv1_result icu4x_CodePointMapData8_create_grapheme_cluster_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_word_break_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_word_break_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_word_break_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_word_break_with_provider_mv1_result icu4x_CodePointMapData8_create_word_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_sentence_break_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_sentence_break_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_sentence_break_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_sentence_break_with_provider_mv1_result icu4x_CodePointMapData8_create_sentence_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_canonical_combining_class_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_canonical_combining_class_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_canonical_combining_class_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_canonical_combining_class_with_provider_mv1_result icu4x_CodePointMapData8_create_canonical_combining_class_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_indic_syllabic_category_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_indic_syllabic_category_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_indic_syllabic_category_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_indic_syllabic_category_with_provider_mv1_result icu4x_CodePointMapData8_create_indic_syllabic_category_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_indic_conjunct_break_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_indic_conjunct_break_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_indic_conjunct_break_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_indic_conjunct_break_with_provider_mv1_result icu4x_CodePointMapData8_create_indic_conjunct_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_joining_group_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_joining_group_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_joining_group_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_joining_group_with_provider_mv1_result icu4x_CodePointMapData8_create_joining_group_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_joining_type_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_joining_type_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_joining_type_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_joining_type_with_provider_mv1_result icu4x_CodePointMapData8_create_joining_type_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_general_category_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_general_category_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_general_category_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_general_category_with_provider_mv1_result icu4x_CodePointMapData8_create_general_category_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::CodePointMapData8* icu4x_CodePointMapData8_create_vertical_orientation_mv1(void);

    typedef struct icu4x_CodePointMapData8_create_vertical_orientation_with_provider_mv1_result {union {icu4x::capi::CodePointMapData8* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData8_create_vertical_orientation_with_provider_mv1_result;
    icu4x_CodePointMapData8_create_vertical_orientation_with_provider_mv1_result icu4x_CodePointMapData8_create_vertical_orientation_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_CodePointMapData8_destroy_mv1(CodePointMapData8* self);

    } // extern "C"
} // namespace capi
} // namespace

inline uint8_t icu4x::CodePointMapData8::operator[](char32_t cp) const {
    auto result = icu4x::capi::icu4x_CodePointMapData8_get_mv1(this->AsFFI(),
        cp);
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

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointMapData8::iter_ranges_for_group(icu4x::GeneralCategoryGroup group) const {
    auto result = icu4x::capi::icu4x_CodePointMapData8_iter_ranges_for_group_mv1(this->AsFFI(),
        group.AsFFI());
    return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointMapData8::get_set_for_value(uint8_t value) const {
    auto result = icu4x::capi::icu4x_CodePointMapData8_get_set_for_value_mv1(this->AsFFI(),
        value);
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_bidi_class() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_bidi_class_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_bidi_class_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_bidi_class_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_numeric_type() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_numeric_type_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_numeric_type_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_numeric_type_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_hangul_syllable_type() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_hangul_syllable_type_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_hangul_syllable_type_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_hangul_syllable_type_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_east_asian_width() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_east_asian_width_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_east_asian_width_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_east_asian_width_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_line_break() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_line_break_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_line_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_line_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_grapheme_cluster_break() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_grapheme_cluster_break_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_grapheme_cluster_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_grapheme_cluster_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_word_break() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_word_break_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_word_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_word_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_sentence_break() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_sentence_break_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_sentence_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_sentence_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_canonical_combining_class() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_canonical_combining_class_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_canonical_combining_class_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_canonical_combining_class_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_indic_syllabic_category() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_indic_syllabic_category_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_indic_syllabic_category_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_indic_syllabic_category_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_indic_conjunct_break() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_indic_conjunct_break_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_indic_conjunct_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_indic_conjunct_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_joining_group() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_joining_group_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_joining_group_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_joining_group_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_joining_type() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_joining_type_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_joining_type_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_joining_type_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_general_category() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_general_category_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_general_category_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_general_category_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::CodePointMapData8> icu4x::CodePointMapData8::create_vertical_orientation() {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_vertical_orientation_mv1();
    return std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError> icu4x::CodePointMapData8::create_vertical_orientation_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData8_create_vertical_orientation_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData8>>(std::unique_ptr<icu4x::CodePointMapData8>(icu4x::CodePointMapData8::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData8>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
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


#endif // ICU4X_CodePointMapData8_HPP
