#ifndef ICU4X_SentenceSegmenter_HPP
#define ICU4X_SentenceSegmenter_HPP

#include "SentenceSegmenter.d.hpp"

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
#include "SentenceBreakIteratorLatin1.hpp"
#include "SentenceBreakIteratorUtf16.hpp"
#include "SentenceBreakIteratorUtf8.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::SentenceSegmenter* icu4x_SentenceSegmenter_create_mv1(void);

    typedef struct icu4x_SentenceSegmenter_create_with_content_locale_mv1_result {union {icu4x::capi::SentenceSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_SentenceSegmenter_create_with_content_locale_mv1_result;
    icu4x_SentenceSegmenter_create_with_content_locale_mv1_result icu4x_SentenceSegmenter_create_with_content_locale_mv1(const icu4x::capi::Locale* locale);

    typedef struct icu4x_SentenceSegmenter_create_with_content_locale_and_provider_mv1_result {union {icu4x::capi::SentenceSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_SentenceSegmenter_create_with_content_locale_and_provider_mv1_result;
    icu4x_SentenceSegmenter_create_with_content_locale_and_provider_mv1_result icu4x_SentenceSegmenter_create_with_content_locale_and_provider_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* locale);

    icu4x::capi::SentenceBreakIteratorUtf8* icu4x_SentenceSegmenter_segment_utf8_mv1(const icu4x::capi::SentenceSegmenter* self, icu4x::diplomat::capi::DiplomatStringView input);

    icu4x::capi::SentenceBreakIteratorUtf16* icu4x_SentenceSegmenter_segment_utf16_mv1(const icu4x::capi::SentenceSegmenter* self, icu4x::diplomat::capi::DiplomatString16View input);

    icu4x::capi::SentenceBreakIteratorLatin1* icu4x_SentenceSegmenter_segment_latin1_mv1(const icu4x::capi::SentenceSegmenter* self, icu4x::diplomat::capi::DiplomatU8View input);

    void icu4x_SentenceSegmenter_destroy_mv1(SentenceSegmenter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::SentenceSegmenter> icu4x::SentenceSegmenter::create() {
    auto result = icu4x::capi::icu4x_SentenceSegmenter_create_mv1();
    return std::unique_ptr<icu4x::SentenceSegmenter>(icu4x::SentenceSegmenter::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError> icu4x::SentenceSegmenter::create_with_content_locale(const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_SentenceSegmenter_create_with_content_locale_mv1(locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::SentenceSegmenter>>(std::unique_ptr<icu4x::SentenceSegmenter>(icu4x::SentenceSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError> icu4x::SentenceSegmenter::create_with_content_locale_and_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale) {
    auto result = icu4x::capi::icu4x_SentenceSegmenter_create_with_content_locale_and_provider_mv1(provider.AsFFI(),
        locale.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::SentenceSegmenter>>(std::unique_ptr<icu4x::SentenceSegmenter>(icu4x::SentenceSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::SentenceSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::SentenceBreakIteratorUtf8> icu4x::SentenceSegmenter::segment(std::string_view input) const {
    auto result = icu4x::capi::icu4x_SentenceSegmenter_segment_utf8_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::SentenceBreakIteratorUtf8>(icu4x::SentenceBreakIteratorUtf8::FromFFI(result));
}

inline std::unique_ptr<icu4x::SentenceBreakIteratorUtf16> icu4x::SentenceSegmenter::segment16(std::u16string_view input) const {
    auto result = icu4x::capi::icu4x_SentenceSegmenter_segment_utf16_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::SentenceBreakIteratorUtf16>(icu4x::SentenceBreakIteratorUtf16::FromFFI(result));
}

inline std::unique_ptr<icu4x::SentenceBreakIteratorLatin1> icu4x::SentenceSegmenter::segment_latin1(icu4x::diplomat::span<const uint8_t> input) const {
    auto result = icu4x::capi::icu4x_SentenceSegmenter_segment_latin1_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::SentenceBreakIteratorLatin1>(icu4x::SentenceBreakIteratorLatin1::FromFFI(result));
}

inline const icu4x::capi::SentenceSegmenter* icu4x::SentenceSegmenter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::SentenceSegmenter*>(this);
}

inline icu4x::capi::SentenceSegmenter* icu4x::SentenceSegmenter::AsFFI() {
    return reinterpret_cast<icu4x::capi::SentenceSegmenter*>(this);
}

inline const icu4x::SentenceSegmenter* icu4x::SentenceSegmenter::FromFFI(const icu4x::capi::SentenceSegmenter* ptr) {
    return reinterpret_cast<const icu4x::SentenceSegmenter*>(ptr);
}

inline icu4x::SentenceSegmenter* icu4x::SentenceSegmenter::FromFFI(icu4x::capi::SentenceSegmenter* ptr) {
    return reinterpret_cast<icu4x::SentenceSegmenter*>(ptr);
}

inline void icu4x::SentenceSegmenter::operator delete(void* ptr) {
    icu4x::capi::icu4x_SentenceSegmenter_destroy_mv1(reinterpret_cast<icu4x::capi::SentenceSegmenter*>(ptr));
}


#endif // ICU4X_SentenceSegmenter_HPP
