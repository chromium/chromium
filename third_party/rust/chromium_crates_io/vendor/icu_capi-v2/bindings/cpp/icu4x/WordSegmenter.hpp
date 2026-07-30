#ifndef ICU4X_WordSegmenter_HPP
#define ICU4X_WordSegmenter_HPP

#include "WordSegmenter.d.hpp"

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
#include "Locale.hpp"
#include "WordBreakIteratorLatin1.hpp"
#include "WordBreakIteratorUtf16.hpp"
#include "WordBreakIteratorUtf8.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::WordSegmenter* icu4x_WordSegmenter_create_auto_mv1(void);

    typedef struct icu4x_WordSegmenter_create_auto_with_content_locale_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_auto_with_content_locale_mv1_result;
    icu4x_WordSegmenter_create_auto_with_content_locale_mv1_result icu4x_WordSegmenter_create_auto_with_content_locale_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_WordSegmenter_create_auto_with_content_locale_and_provider_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_auto_with_content_locale_and_provider_mv1_result;
    icu4x_WordSegmenter_create_auto_with_content_locale_and_provider_mv1_result icu4x_WordSegmenter_create_auto_with_content_locale_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::WordSegmenter* icu4x_WordSegmenter_create_lstm_mv1(void);

    typedef struct icu4x_WordSegmenter_create_lstm_with_content_locale_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_lstm_with_content_locale_mv1_result;
    icu4x_WordSegmenter_create_lstm_with_content_locale_mv1_result icu4x_WordSegmenter_create_lstm_with_content_locale_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_WordSegmenter_create_lstm_with_content_locale_and_provider_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_lstm_with_content_locale_and_provider_mv1_result;
    icu4x_WordSegmenter_create_lstm_with_content_locale_and_provider_mv1_result icu4x_WordSegmenter_create_lstm_with_content_locale_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::WordSegmenter* icu4x_WordSegmenter_create_dictionary_mv1(void);

    typedef struct icu4x_WordSegmenter_create_dictionary_with_content_locale_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_dictionary_with_content_locale_mv1_result;
    icu4x_WordSegmenter_create_dictionary_with_content_locale_mv1_result icu4x_WordSegmenter_create_dictionary_with_content_locale_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_WordSegmenter_create_dictionary_with_content_locale_and_provider_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_dictionary_with_content_locale_and_provider_mv1_result;
    icu4x_WordSegmenter_create_dictionary_with_content_locale_and_provider_mv1_result icu4x_WordSegmenter_create_dictionary_with_content_locale_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::WordSegmenter* icu4x_WordSegmenter_create_for_non_complex_scripts_mv1(void);

    typedef struct icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_mv1_result;
    icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_mv1_result icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_and_provider_mv1_result {union {icu4x::capi::WordSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_and_provider_mv1_result;
    icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_and_provider_mv1_result icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::WordBreakIteratorUtf8* icu4x_WordSegmenter_segment_utf8_mv1(const icu4x::capi::WordSegmenter* self, icu4x::diplomat::capi::DiplomatStringView input);

    icu4x::capi::WordBreakIteratorUtf16* icu4x_WordSegmenter_segment_utf16_mv1(const icu4x::capi::WordSegmenter* self, icu4x::diplomat::capi::DiplomatString16View input);

    icu4x::capi::WordBreakIteratorLatin1* icu4x_WordSegmenter_segment_latin1_mv1(const icu4x::capi::WordSegmenter* self, icu4x::diplomat::capi::DiplomatU8View input);

    void icu4x_WordSegmenter_destroy_mv1(WordSegmenter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::WordSegmenter> icu4x::WordSegmenter::create_auto() {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_auto_mv1();
    return std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_auto_with_content_locale(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_auto_with_content_locale_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_auto_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_auto_with_content_locale_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::WordSegmenter> icu4x::WordSegmenter::create_lstm() {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_lstm_mv1();
    return std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_lstm_with_content_locale(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_lstm_with_content_locale_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_lstm_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_lstm_with_content_locale_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::WordSegmenter> icu4x::WordSegmenter::create_dictionary() {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_dictionary_mv1();
    return std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_dictionary_with_content_locale(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_dictionary_with_content_locale_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_dictionary_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_dictionary_with_content_locale_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::WordSegmenter> icu4x::WordSegmenter::create_for_non_complex_scripts() {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_for_non_complex_scripts_mv1();
    return std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_for_non_complex_scripts_with_content_locale(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError> icu4x::WordSegmenter::create_for_non_complex_scripts_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_WordSegmenter_create_for_non_complex_scripts_with_content_locale_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::WordSegmenter>>(std::unique_ptr<icu4x::WordSegmenter>(icu4x::WordSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::WordSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::WordBreakIteratorUtf8> icu4x::WordSegmenter::segment(std::string_view input) const {
    auto result = icu4x::capi::icu4x_WordSegmenter_segment_utf8_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::WordBreakIteratorUtf8>(icu4x::WordBreakIteratorUtf8::FromFFI(result));
}

inline std::unique_ptr<icu4x::WordBreakIteratorUtf16> icu4x::WordSegmenter::segment16(std::u16string_view input) const {
    auto result = icu4x::capi::icu4x_WordSegmenter_segment_utf16_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::WordBreakIteratorUtf16>(icu4x::WordBreakIteratorUtf16::FromFFI(result));
}

inline std::unique_ptr<icu4x::WordBreakIteratorLatin1> icu4x::WordSegmenter::segment_latin1(icu4x::diplomat::span<const uint8_t> input) const {
    auto result = icu4x::capi::icu4x_WordSegmenter_segment_latin1_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::WordBreakIteratorLatin1>(icu4x::WordBreakIteratorLatin1::FromFFI(result));
}

inline const icu4x::capi::WordSegmenter* icu4x::WordSegmenter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::WordSegmenter*>(this);
}

inline icu4x::capi::WordSegmenter* icu4x::WordSegmenter::AsFFI() {
    return reinterpret_cast<icu4x::capi::WordSegmenter*>(this);
}

inline const icu4x::WordSegmenter* icu4x::WordSegmenter::FromFFI(const icu4x::capi::WordSegmenter* ptr) {
    return reinterpret_cast<const icu4x::WordSegmenter*>(ptr);
}

inline icu4x::WordSegmenter* icu4x::WordSegmenter::FromFFI(icu4x::capi::WordSegmenter* ptr) {
    return reinterpret_cast<icu4x::WordSegmenter*>(ptr);
}

inline void icu4x::WordSegmenter::operator delete(void* ptr) {
    icu4x::capi::icu4x_WordSegmenter_destroy_mv1(reinterpret_cast<icu4x::capi::WordSegmenter*>(ptr));
}


#endif // ICU4X_WordSegmenter_HPP
