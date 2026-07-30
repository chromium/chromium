#ifndef ICU4X_ScriptWithExtensions_HPP
#define ICU4X_ScriptWithExtensions_HPP

#include "ScriptWithExtensions.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointRangeIterator.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "ScriptWithExtensionsBorrowed.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    icu4x::capi::ScriptWithExtensions* icu4x_ScriptWithExtensions_create_mv1(void);

    typedef struct icu4x_ScriptWithExtensions_create_with_provider_mv1_result {union {icu4x::capi::ScriptWithExtensions* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_ScriptWithExtensions_create_with_provider_mv1_result;
    icu4x_ScriptWithExtensions_create_with_provider_mv1_result icu4x_ScriptWithExtensions_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);

    uint16_t icu4x_ScriptWithExtensions_get_script_val_mv1(const icu4x::capi::ScriptWithExtensions* self, char32_t ch);

    bool icu4x_ScriptWithExtensions_has_script_mv1(const icu4x::capi::ScriptWithExtensions* self, char32_t ch, uint16_t script);

    icu4x::capi::ScriptWithExtensionsBorrowed* icu4x_ScriptWithExtensions_as_borrowed_mv1(const icu4x::capi::ScriptWithExtensions* self);

    icu4x::capi::CodePointRangeIterator* icu4x_ScriptWithExtensions_iter_ranges_for_script_mv1(const icu4x::capi::ScriptWithExtensions* self, uint16_t script);

    void icu4x_ScriptWithExtensions_destroy_mv1(ScriptWithExtensions* self);

    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::ScriptWithExtensions> icu4x::ScriptWithExtensions::create() {
    auto result = icu4x::capi::icu4x_ScriptWithExtensions_create_mv1();
    return std::unique_ptr<icu4x::ScriptWithExtensions>(icu4x::ScriptWithExtensions::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::ScriptWithExtensions>, icu4x::DataError> icu4x::ScriptWithExtensions::create_with_provider(const icu4x::DataProvider& provider) {
    auto result = icu4x::capi::icu4x_ScriptWithExtensions_create_with_provider_mv1(provider.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::ScriptWithExtensions>, icu4x::DataError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::ScriptWithExtensions>>(std::unique_ptr<icu4x::ScriptWithExtensions>(icu4x::ScriptWithExtensions::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::ScriptWithExtensions>, icu4x::DataError>(icu4x::diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline uint16_t icu4x::ScriptWithExtensions::get_script_val(char32_t ch) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensions_get_script_val_mv1(this->AsFFI(),
        ch);
    return result;
}

inline bool icu4x::ScriptWithExtensions::has_script(char32_t ch, uint16_t script) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensions_has_script_mv1(this->AsFFI(),
        ch,
        script);
    return result;
}

inline std::unique_ptr<icu4x::ScriptWithExtensionsBorrowed> icu4x::ScriptWithExtensions::as_borrowed() const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensions_as_borrowed_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::ScriptWithExtensionsBorrowed>(icu4x::ScriptWithExtensionsBorrowed::FromFFI(result));
}

inline std::unique_ptr<icu4x::CodePointRangeIterator> icu4x::ScriptWithExtensions::iter_ranges_for_script(uint16_t script) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensions_iter_ranges_for_script_mv1(this->AsFFI(),
        script);
    return std::unique_ptr<icu4x::CodePointRangeIterator>(icu4x::CodePointRangeIterator::FromFFI(result));
}

inline const icu4x::capi::ScriptWithExtensions* icu4x::ScriptWithExtensions::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ScriptWithExtensions*>(this);
}

inline icu4x::capi::ScriptWithExtensions* icu4x::ScriptWithExtensions::AsFFI() {
    return reinterpret_cast<icu4x::capi::ScriptWithExtensions*>(this);
}

inline const icu4x::ScriptWithExtensions* icu4x::ScriptWithExtensions::FromFFI(const icu4x::capi::ScriptWithExtensions* ptr) {
    return reinterpret_cast<const icu4x::ScriptWithExtensions*>(ptr);
}

inline icu4x::ScriptWithExtensions* icu4x::ScriptWithExtensions::FromFFI(icu4x::capi::ScriptWithExtensions* ptr) {
    return reinterpret_cast<icu4x::ScriptWithExtensions*>(ptr);
}

inline void icu4x::ScriptWithExtensions::operator delete(void* ptr) {
    icu4x::capi::icu4x_ScriptWithExtensions_destroy_mv1(reinterpret_cast<icu4x::capi::ScriptWithExtensions*>(ptr));
}


#endif // ICU4X_ScriptWithExtensions_HPP
