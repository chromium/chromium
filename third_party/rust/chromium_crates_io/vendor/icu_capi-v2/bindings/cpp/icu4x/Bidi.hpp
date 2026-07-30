#ifndef ICU4X_Bidi_HPP
#define ICU4X_Bidi_HPP

#include "Bidi.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "BidiInfo.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "ReorderedIndexMap.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::Bidi* icu4x_Bidi_create_mv1(void);

    typedef struct icu4x_Bidi_create_with_provider_mv1_result {union {icu4x::capi::Bidi* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_Bidi_create_with_provider_mv1_result;
    icu4x_Bidi_create_with_provider_mv1_result icu4x_Bidi_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    icu4x::capi::BidiInfo* icu4x_Bidi_for_text_utf8_mv1(const icu4x::capi::Bidi* self, icu4x::diplomat::capi::DiplomatStringView text, icu4x::diplomat::capi::OptionU8 default_level);

    icu4x::capi::ReorderedIndexMap* icu4x_Bidi_reorder_visual_mv1(const icu4x::capi::Bidi* self, icu4x::diplomat::capi::DiplomatU8View levels);

    bool icu4x_Bidi_level_is_rtl_mv1(uint8_t level);

    bool icu4x_Bidi_level_is_ltr_mv1(uint8_t level);

    uint8_t icu4x_Bidi_level_rtl_mv1(void);

    uint8_t icu4x_Bidi_level_ltr_mv1(void);

    void icu4x_Bidi_destroy_mv1(Bidi* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::Bidi> icu4x::Bidi::create() {
    auto result = icu4x::capi::icu4x_Bidi_create_mv1();
    return std::unique_ptr<icu4x::Bidi>(icu4x::Bidi::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Bidi>, icu4x::DataError> icu4x::Bidi::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_Bidi_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Bidi>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Bidi>>(std::unique_ptr<icu4x::Bidi>(icu4x::Bidi::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Bidi>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::BidiInfo> icu4x::Bidi::for_text(std::string_view text, std::optional<uint8_t> default_level) const {
    auto result = icu4x::capi::icu4x_Bidi_for_text_utf8_mv1(this->AsFFI(),
        {text.data(), text.size()},
        default_level.has_value() ? (icu4x::diplomat::capi::OptionU8{ { default_level.value() }, true }) : (icu4x::diplomat::capi::OptionU8{ {}, false }));
    return std::unique_ptr<icu4x::BidiInfo>(icu4x::BidiInfo::FromFFI(result));
}

inline std::unique_ptr<icu4x::ReorderedIndexMap> icu4x::Bidi::reorder_visual(icu4x::diplomat::span<const uint8_t> levels) const {
    auto result = icu4x::capi::icu4x_Bidi_reorder_visual_mv1(this->AsFFI(),
        {levels.data(), levels.size()});
    return std::unique_ptr<icu4x::ReorderedIndexMap>(icu4x::ReorderedIndexMap::FromFFI(result));
}

inline bool icu4x::Bidi::level_is_rtl(uint8_t level) {
    auto result = icu4x::capi::icu4x_Bidi_level_is_rtl_mv1(level);
    return result;
}

inline bool icu4x::Bidi::level_is_ltr(uint8_t level) {
    auto result = icu4x::capi::icu4x_Bidi_level_is_ltr_mv1(level);
    return result;
}

inline uint8_t icu4x::Bidi::level_rtl() {
    auto result = icu4x::capi::icu4x_Bidi_level_rtl_mv1();
    return result;
}

inline uint8_t icu4x::Bidi::level_ltr() {
    auto result = icu4x::capi::icu4x_Bidi_level_ltr_mv1();
    return result;
}

inline const icu4x::capi::Bidi* icu4x::Bidi::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Bidi*>(this);
}

inline icu4x::capi::Bidi* icu4x::Bidi::AsFFI() {
    return reinterpret_cast<icu4x::capi::Bidi*>(this);
}

inline const icu4x::Bidi* icu4x::Bidi::FromFFI(const icu4x::capi::Bidi* ptr) {
    return reinterpret_cast<const icu4x::Bidi*>(ptr);
}

inline icu4x::Bidi* icu4x::Bidi::FromFFI(icu4x::capi::Bidi* ptr) {
    return reinterpret_cast<icu4x::Bidi*>(ptr);
}

inline void icu4x::Bidi::operator delete(void* ptr) {
    icu4x::capi::icu4x_Bidi_destroy_mv1(reinterpret_cast<icu4x::capi::Bidi*>(ptr));
}


#endif // ICU4X_Bidi_HPP
