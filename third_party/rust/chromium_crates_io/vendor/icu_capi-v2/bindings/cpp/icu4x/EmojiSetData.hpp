#ifndef ICU4X_EmojiSetData_HPP
#define ICU4X_EmojiSetData_HPP

#include "EmojiSetData.d.hpp"

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

    bool icu4x_EmojiSetData_contains_str_mv1(const icu4x::capi::EmojiSetData* self, icu4x::diplomat::capi::DiplomatStringView s);

    bool icu4x_EmojiSetData_contains_mv1(const icu4x::capi::EmojiSetData* self, char32_t cp);

    icu4x::capi::EmojiSetData* icu4x_EmojiSetData_create_basic_mv1(void);

    typedef struct icu4x_EmojiSetData_create_basic_with_provider_mv1_result {union {icu4x::capi::EmojiSetData* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_EmojiSetData_create_basic_with_provider_mv1_result;
    icu4x_EmojiSetData_create_basic_with_provider_mv1_result icu4x_EmojiSetData_create_basic_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    bool icu4x_EmojiSetData_basic_emoji_for_char_mv1(char32_t ch);

    bool icu4x_EmojiSetData_basic_emoji_for_str_mv1(icu4x::diplomat::capi::DiplomatStringView s);

    void icu4x_EmojiSetData_destroy_mv1(EmojiSetData* self);

    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::EmojiSetData::contains(std::string_view s) const {
    auto result = icu4x::capi::icu4x_EmojiSetData_contains_str_mv1(this->AsFFI(),
        {s.data(), s.size()});
    return result;
}

inline bool icu4x::EmojiSetData::contains(char32_t cp) const {
    auto result = icu4x::capi::icu4x_EmojiSetData_contains_mv1(this->AsFFI(),
        cp);
    return result;
}

inline std::unique_ptr<icu4x::EmojiSetData> icu4x::EmojiSetData::create_basic() {
    auto result = icu4x::capi::icu4x_EmojiSetData_create_basic_mv1();
    return std::unique_ptr<icu4x::EmojiSetData>(icu4x::EmojiSetData::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError> icu4x::EmojiSetData::create_basic_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_EmojiSetData_create_basic_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::EmojiSetData>>(std::unique_ptr<icu4x::EmojiSetData>(icu4x::EmojiSetData::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::EmojiSetData>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline bool icu4x::EmojiSetData::basic_emoji_for_char(char32_t ch) {
    auto result = icu4x::capi::icu4x_EmojiSetData_basic_emoji_for_char_mv1(ch);
    return result;
}

inline bool icu4x::EmojiSetData::basic_emoji_for_str(std::string_view s) {
    auto result = icu4x::capi::icu4x_EmojiSetData_basic_emoji_for_str_mv1({s.data(), s.size()});
    return result;
}

inline const icu4x::capi::EmojiSetData* icu4x::EmojiSetData::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::EmojiSetData*>(this);
}

inline icu4x::capi::EmojiSetData* icu4x::EmojiSetData::AsFFI() {
    return reinterpret_cast<icu4x::capi::EmojiSetData*>(this);
}

inline const icu4x::EmojiSetData* icu4x::EmojiSetData::FromFFI(const icu4x::capi::EmojiSetData* ptr) {
    return reinterpret_cast<const icu4x::EmojiSetData*>(ptr);
}

inline icu4x::EmojiSetData* icu4x::EmojiSetData::FromFFI(icu4x::capi::EmojiSetData* ptr) {
    return reinterpret_cast<icu4x::EmojiSetData*>(ptr);
}

inline void icu4x::EmojiSetData::operator delete(void* ptr) {
    icu4x::capi::icu4x_EmojiSetData_destroy_mv1(reinterpret_cast<icu4x::capi::EmojiSetData*>(ptr));
}


#endif // ICU4X_EmojiSetData_HPP
