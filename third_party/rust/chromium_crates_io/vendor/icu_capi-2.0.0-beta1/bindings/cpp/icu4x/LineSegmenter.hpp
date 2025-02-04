#ifndef icu4x_LineSegmenter_HPP
#define icu4x_LineSegmenter_HPP

#include "LineSegmenter.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "LineBreakIteratorLatin1.hpp"
#include "LineBreakIteratorUtf16.hpp"
#include "LineBreakIteratorUtf8.hpp"
#include "LineBreakOptionsV2.hpp"
#include "Locale.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_LineSegmenter_create_auto_mv1_result {union {icu4x::capi::LineSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LineSegmenter_create_auto_mv1_result;
    icu4x_LineSegmenter_create_auto_mv1_result icu4x_LineSegmenter_create_auto_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_LineSegmenter_create_lstm_mv1_result {union {icu4x::capi::LineSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LineSegmenter_create_lstm_mv1_result;
    icu4x_LineSegmenter_create_lstm_mv1_result icu4x_LineSegmenter_create_lstm_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_LineSegmenter_create_dictionary_mv1_result {union {icu4x::capi::LineSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LineSegmenter_create_dictionary_mv1_result;
    icu4x_LineSegmenter_create_dictionary_mv1_result icu4x_LineSegmenter_create_dictionary_mv1(const icu4x::capi::DataProvider* provider);
    
    typedef struct icu4x_LineSegmenter_create_auto_with_options_v2_mv1_result {union {icu4x::capi::LineSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LineSegmenter_create_auto_with_options_v2_mv1_result;
    icu4x_LineSegmenter_create_auto_with_options_v2_mv1_result icu4x_LineSegmenter_create_auto_with_options_v2_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* content_locale, icu4x::capi::LineBreakOptionsV2 options);
    
    typedef struct icu4x_LineSegmenter_create_lstm_with_options_v2_mv1_result {union {icu4x::capi::LineSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LineSegmenter_create_lstm_with_options_v2_mv1_result;
    icu4x_LineSegmenter_create_lstm_with_options_v2_mv1_result icu4x_LineSegmenter_create_lstm_with_options_v2_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* content_locale, icu4x::capi::LineBreakOptionsV2 options);
    
    typedef struct icu4x_LineSegmenter_create_dictionary_with_options_v2_mv1_result {union {icu4x::capi::LineSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_LineSegmenter_create_dictionary_with_options_v2_mv1_result;
    icu4x_LineSegmenter_create_dictionary_with_options_v2_mv1_result icu4x_LineSegmenter_create_dictionary_with_options_v2_mv1(const icu4x::capi::DataProvider* provider, const icu4x::capi::Locale* content_locale, icu4x::capi::LineBreakOptionsV2 options);
    
    icu4x::capi::LineBreakIteratorUtf8* icu4x_LineSegmenter_segment_utf8_mv1(const icu4x::capi::LineSegmenter* self, diplomat::capi::DiplomatStringView input);
    
    icu4x::capi::LineBreakIteratorUtf16* icu4x_LineSegmenter_segment_utf16_mv1(const icu4x::capi::LineSegmenter* self, diplomat::capi::DiplomatString16View input);
    
    icu4x::capi::LineBreakIteratorLatin1* icu4x_LineSegmenter_segment_latin1_mv1(const icu4x::capi::LineSegmenter* self, diplomat::capi::DiplomatU8View input);
    
    
    void icu4x_LineSegmenter_destroy_mv1(LineSegmenter* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> icu4x::LineSegmenter::create_auto(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_LineSegmenter_create_auto_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LineSegmenter>>(std::unique_ptr<icu4x::LineSegmenter>(icu4x::LineSegmenter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> icu4x::LineSegmenter::create_lstm(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_LineSegmenter_create_lstm_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LineSegmenter>>(std::unique_ptr<icu4x::LineSegmenter>(icu4x::LineSegmenter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> icu4x::LineSegmenter::create_dictionary(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_LineSegmenter_create_dictionary_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LineSegmenter>>(std::unique_ptr<icu4x::LineSegmenter>(icu4x::LineSegmenter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> icu4x::LineSegmenter::create_auto_with_options_v2(const icu4x::DataProvider& provider, const icu4x::Locale& content_locale, icu4x::LineBreakOptionsV2 options) {
  auto result = icu4x::capi::icu4x_LineSegmenter_create_auto_with_options_v2_mv1(provider.AsFFI(),
    content_locale.AsFFI(),
    options.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LineSegmenter>>(std::unique_ptr<icu4x::LineSegmenter>(icu4x::LineSegmenter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> icu4x::LineSegmenter::create_lstm_with_options_v2(const icu4x::DataProvider& provider, const icu4x::Locale& content_locale, icu4x::LineBreakOptionsV2 options) {
  auto result = icu4x::capi::icu4x_LineSegmenter_create_lstm_with_options_v2_mv1(provider.AsFFI(),
    content_locale.AsFFI(),
    options.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LineSegmenter>>(std::unique_ptr<icu4x::LineSegmenter>(icu4x::LineSegmenter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError> icu4x::LineSegmenter::create_dictionary_with_options_v2(const icu4x::DataProvider& provider, const icu4x::Locale& content_locale, icu4x::LineBreakOptionsV2 options) {
  auto result = icu4x::capi::icu4x_LineSegmenter_create_dictionary_with_options_v2_mv1(provider.AsFFI(),
    content_locale.AsFFI(),
    options.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::LineSegmenter>>(std::unique_ptr<icu4x::LineSegmenter>(icu4x::LineSegmenter::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::LineSegmenter>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::LineBreakIteratorUtf8> icu4x::LineSegmenter::segment(std::string_view input) const {
  auto result = icu4x::capi::icu4x_LineSegmenter_segment_utf8_mv1(this->AsFFI(),
    {input.data(), input.size()});
  return std::unique_ptr<icu4x::LineBreakIteratorUtf8>(icu4x::LineBreakIteratorUtf8::FromFFI(result));
}

inline std::unique_ptr<icu4x::LineBreakIteratorUtf16> icu4x::LineSegmenter::segment16(std::u16string_view input) const {
  auto result = icu4x::capi::icu4x_LineSegmenter_segment_utf16_mv1(this->AsFFI(),
    {input.data(), input.size()});
  return std::unique_ptr<icu4x::LineBreakIteratorUtf16>(icu4x::LineBreakIteratorUtf16::FromFFI(result));
}

inline std::unique_ptr<icu4x::LineBreakIteratorLatin1> icu4x::LineSegmenter::segment_latin1(diplomat::span<const uint8_t> input) const {
  auto result = icu4x::capi::icu4x_LineSegmenter_segment_latin1_mv1(this->AsFFI(),
    {input.data(), input.size()});
  return std::unique_ptr<icu4x::LineBreakIteratorLatin1>(icu4x::LineBreakIteratorLatin1::FromFFI(result));
}

inline const icu4x::capi::LineSegmenter* icu4x::LineSegmenter::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::LineSegmenter*>(this);
}

inline icu4x::capi::LineSegmenter* icu4x::LineSegmenter::AsFFI() {
  return reinterpret_cast<icu4x::capi::LineSegmenter*>(this);
}

inline const icu4x::LineSegmenter* icu4x::LineSegmenter::FromFFI(const icu4x::capi::LineSegmenter* ptr) {
  return reinterpret_cast<const icu4x::LineSegmenter*>(ptr);
}

inline icu4x::LineSegmenter* icu4x::LineSegmenter::FromFFI(icu4x::capi::LineSegmenter* ptr) {
  return reinterpret_cast<icu4x::LineSegmenter*>(ptr);
}

inline void icu4x::LineSegmenter::operator delete(void* ptr) {
  icu4x::capi::icu4x_LineSegmenter_destroy_mv1(reinterpret_cast<icu4x::capi::LineSegmenter*>(ptr));
}


#endif // icu4x_LineSegmenter_HPP
