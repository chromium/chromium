#ifndef ICU4X_PropertyValueNameToEnumMapper_HPP
#define ICU4X_PropertyValueNameToEnumMapper_HPP

#include "PropertyValueNameToEnumMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    int16_t icu4x_PropertyValueNameToEnumMapper_get_strict_mv1(const icu4x::capi::PropertyValueNameToEnumMapper* self, icu4x::diplomat::capi::DiplomatStringView name);

    int16_t icu4x_PropertyValueNameToEnumMapper_get_loose_mv1(const icu4x::capi::PropertyValueNameToEnumMapper* self, icu4x::diplomat::capi::DiplomatStringView name);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_bidi_class_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_bidi_class_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_bidi_class_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_bidi_class_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_bidi_class_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_numeric_type_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_numeric_type_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_numeric_type_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_numeric_type_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_numeric_type_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_script_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_script_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_script_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_script_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_script_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_line_break_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_line_break_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_line_break_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_line_break_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_line_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_word_break_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_word_break_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_word_break_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_word_break_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_word_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_sentence_break_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_sentence_break_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_sentence_break_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_sentence_break_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_sentence_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_joining_group_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_joining_group_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_joining_group_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_joining_group_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_joining_group_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_joining_type_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_joining_type_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_joining_type_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_joining_type_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_joining_type_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_general_category_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_general_category_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_general_category_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_general_category_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_general_category_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::PropertyValueNameToEnumMapper* icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_mv1(void);

    typedef struct icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_with_provider_mv1_result {union {icu4x::capi::PropertyValueNameToEnumMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_with_provider_mv1_result;
    icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_with_provider_mv1_result icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_with_provider_mv1(const icu4x::capi::DataProvider* provider);

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

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_bidi_class() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_bidi_class_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_bidi_class_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_bidi_class_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_numeric_type() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_numeric_type_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_numeric_type_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_numeric_type_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_script() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_script_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_script_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_script_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_hangul_syllable_type() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_hangul_syllable_type_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_hangul_syllable_type_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_east_asian_width() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_east_asian_width_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_east_asian_width_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_line_break() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_line_break_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_line_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_line_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_grapheme_cluster_break() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_grapheme_cluster_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_grapheme_cluster_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_word_break() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_word_break_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_word_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_word_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_sentence_break() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_sentence_break_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_sentence_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_sentence_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_canonical_combining_class() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_canonical_combining_class_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_canonical_combining_class_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_indic_syllabic_category() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_indic_syllabic_category_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_indic_syllabic_category_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_indic_conjunct_break() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_indic_conjunct_break_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_indic_conjunct_break_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_joining_group() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_joining_group_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_joining_group_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_joining_group_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_joining_type() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_joining_type_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_joining_type_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_joining_type_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_general_category() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_general_category_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_general_category_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_general_category_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::PropertyValueNameToEnumMapper> icu4x::PropertyValueNameToEnumMapper::create_vertical_orientation() {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_mv1();
    return std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError> icu4x::PropertyValueNameToEnumMapper::create_vertical_orientation_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_PropertyValueNameToEnumMapper_create_vertical_orientation_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>>(std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>(icu4x::PropertyValueNameToEnumMapper::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::PropertyValueNameToEnumMapper>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
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


#endif // ICU4X_PropertyValueNameToEnumMapper_HPP
