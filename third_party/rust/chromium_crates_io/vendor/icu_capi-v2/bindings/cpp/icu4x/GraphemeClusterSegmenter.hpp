#ifndef ICU4X_GraphemeClusterSegmenter_HPP
#define ICU4X_GraphemeClusterSegmenter_HPP

#include "GraphemeClusterSegmenter.d.hpp"

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
#include "GraphemeClusterBreakIteratorLatin1.hpp"
#include "GraphemeClusterBreakIteratorUtf16.hpp"
#include "GraphemeClusterBreakIteratorUtf8.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::GraphemeClusterSegmenter* icu4x_GraphemeClusterSegmenter_create_mv1(void);

    typedef struct icu4x_GraphemeClusterSegmenter_create_with_provider_mv1_result {union {icu4x::capi::GraphemeClusterSegmenter* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_GraphemeClusterSegmenter_create_with_provider_mv1_result;
    icu4x_GraphemeClusterSegmenter_create_with_provider_mv1_result icu4x_GraphemeClusterSegmenter_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::GraphemeClusterBreakIteratorUtf8* icu4x_GraphemeClusterSegmenter_segment_utf8_mv1(const icu4x::capi::GraphemeClusterSegmenter* self, icu4x::diplomat::capi::DiplomatStringView input);

    icu4x::capi::GraphemeClusterBreakIteratorUtf16* icu4x_GraphemeClusterSegmenter_segment_utf16_mv1(const icu4x::capi::GraphemeClusterSegmenter* self, icu4x::diplomat::capi::DiplomatString16View input);

    icu4x::capi::GraphemeClusterBreakIteratorLatin1* icu4x_GraphemeClusterSegmenter_segment_latin1_mv1(const icu4x::capi::GraphemeClusterSegmenter* self, icu4x::diplomat::capi::DiplomatU8View input);

    void icu4x_GraphemeClusterSegmenter_destroy_mv1(GraphemeClusterSegmenter* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::GraphemeClusterSegmenter> icu4x::GraphemeClusterSegmenter::create() {
    auto result = icu4x::capi::icu4x_GraphemeClusterSegmenter_create_mv1();
    return std::unique_ptr<icu4x::GraphemeClusterSegmenter>(icu4x::GraphemeClusterSegmenter::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::GraphemeClusterSegmenter>, icu4x::DataError> icu4x::GraphemeClusterSegmenter::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_GraphemeClusterSegmenter_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::GraphemeClusterSegmenter>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::GraphemeClusterSegmenter>>(std::unique_ptr<icu4x::GraphemeClusterSegmenter>(icu4x::GraphemeClusterSegmenter::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::GraphemeClusterSegmenter>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf8> icu4x::GraphemeClusterSegmenter::segment(std::string_view input) const {
    auto result = icu4x::capi::icu4x_GraphemeClusterSegmenter_segment_utf8_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf8>(icu4x::GraphemeClusterBreakIteratorUtf8::FromFFI(result));
}

inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf16> icu4x::GraphemeClusterSegmenter::segment16(std::u16string_view input) const {
    auto result = icu4x::capi::icu4x_GraphemeClusterSegmenter_segment_utf16_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::GraphemeClusterBreakIteratorUtf16>(icu4x::GraphemeClusterBreakIteratorUtf16::FromFFI(result));
}

inline std::unique_ptr<icu4x::GraphemeClusterBreakIteratorLatin1> icu4x::GraphemeClusterSegmenter::segment_latin1(icu4x::diplomat::span<const uint8_t> input) const {
    auto result = icu4x::capi::icu4x_GraphemeClusterSegmenter_segment_latin1_mv1(this->AsFFI(),
        {input.data(), input.size()});
    return std::unique_ptr<icu4x::GraphemeClusterBreakIteratorLatin1>(icu4x::GraphemeClusterBreakIteratorLatin1::FromFFI(result));
}

inline const icu4x::capi::GraphemeClusterSegmenter* icu4x::GraphemeClusterSegmenter::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::GraphemeClusterSegmenter*>(this);
}

inline icu4x::capi::GraphemeClusterSegmenter* icu4x::GraphemeClusterSegmenter::AsFFI() {
    return reinterpret_cast<icu4x::capi::GraphemeClusterSegmenter*>(this);
}

inline const icu4x::GraphemeClusterSegmenter* icu4x::GraphemeClusterSegmenter::FromFFI(const icu4x::capi::GraphemeClusterSegmenter* ptr) {
    return reinterpret_cast<const icu4x::GraphemeClusterSegmenter*>(ptr);
}

inline icu4x::GraphemeClusterSegmenter* icu4x::GraphemeClusterSegmenter::FromFFI(icu4x::capi::GraphemeClusterSegmenter* ptr) {
    return reinterpret_cast<icu4x::GraphemeClusterSegmenter*>(ptr);
}

inline void icu4x::GraphemeClusterSegmenter::operator delete(void* ptr) {
    icu4x::capi::icu4x_GraphemeClusterSegmenter_destroy_mv1(reinterpret_cast<icu4x::capi::GraphemeClusterSegmenter*>(ptr));
}


#endif // ICU4X_GraphemeClusterSegmenter_HPP
