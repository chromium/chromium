#ifndef ICU4X_CodePointMapData16_HPP
#define ICU4X_CodePointMapData16_HPP

#include "CodePointMapData16.d.hpp"

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
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    uint16_t icu4x_CodePointMapData16_get_mv1(const icu4x::capi::CodePointMapData16* self, char32_t cp);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData16_iter_ranges_for_value_mv1(const icu4x::capi::CodePointMapData16* self, uint16_t value);

    icu4x::capi::CodePointRangeIterator* icu4x_CodePointMapData16_iter_ranges_for_value_complemented_mv1(const icu4x::capi::CodePointMapData16* self, uint16_t value);

    icu4x::capi::CodePointSetData* icu4x_CodePointMapData16_get_set_for_value_mv1(const icu4x::capi::CodePointMapData16* self, uint16_t value);

    icu4x::capi::CodePointMapData16* icu4x_CodePointMapData16_create_script_mv1(void);

    typedef struct icu4x_CodePointMapData16_create_script_with_provider_mv1_result {union {icu4x::capi::CodePointMapData16* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_CodePointMapData16_create_script_with_provider_mv1_result;
    icu4x_CodePointMapData16_create_script_with_provider_mv1_result icu4x_CodePointMapData16_create_script_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    void icu4x_CodePointMapData16_destroy_mv1(CodePointMapData16* self);

    } // extern "C"
} // namespace capi
} // namespace

inline uint16_t icu4x::CodePointMapData16::operator[](char32_t cp) const {
    auto result = icu4x::capi::icu4x_CodePointMapData16_get_mv1(this->AsFFI(),
        cp);
    return result;
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointMapData16::iter_ranges_for_value(uint16_t value) const {
    auto result = icu4x::capi::icu4x_CodePointMapData16_iter_ranges_for_value_mv1(this->AsFFI(),
        value);
    return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::CodePointMapData16::iter_ranges_for_value_complemented(uint16_t value) const {
    auto result = icu4x::capi::icu4x_CodePointMapData16_iter_ranges_for_value_complemented_mv1(this->AsFFI(),
        value);
    return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::CodePointMapData16::get_set_for_value(uint16_t value) const {
    auto result = icu4x::capi::icu4x_CodePointMapData16_get_set_for_value_mv1(this->AsFFI(),
        value);
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointMapData16> icu4x::CodePointMapData16::create_script() {
    auto result = icu4x::capi::icu4x_CodePointMapData16_create_script_mv1();
    return std::unique_ptr<icu4x::CodePointMapData16>(icu4x::CodePointMapData16::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData16>, icu4x::DataError> icu4x::CodePointMapData16::create_script_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_CodePointMapData16_create_script_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData16>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::CodePointMapData16>>(std::unique_ptr<icu4x::CodePointMapData16>(icu4x::CodePointMapData16::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::CodePointMapData16>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::CodePointMapData16* icu4x::CodePointMapData16::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CodePointMapData16*>(this);
}

inline icu4x::capi::CodePointMapData16* icu4x::CodePointMapData16::AsFFI() {
    return reinterpret_cast<icu4x::capi::CodePointMapData16*>(this);
}

inline const icu4x::CodePointMapData16* icu4x::CodePointMapData16::FromFFI(const icu4x::capi::CodePointMapData16* ptr) {
    return reinterpret_cast<const icu4x::CodePointMapData16*>(ptr);
}

inline icu4x::CodePointMapData16* icu4x::CodePointMapData16::FromFFI(icu4x::capi::CodePointMapData16* ptr) {
    return reinterpret_cast<icu4x::CodePointMapData16*>(ptr);
}

inline void icu4x::CodePointMapData16::operator delete(void* ptr) {
    icu4x::capi::icu4x_CodePointMapData16_destroy_mv1(reinterpret_cast<icu4x::capi::CodePointMapData16*>(ptr));
}


#endif // ICU4X_CodePointMapData16_HPP
