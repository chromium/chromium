#ifndef ICU4X_ScriptWithExtensionsBorrowed_HPP
#define ICU4X_ScriptWithExtensionsBorrowed_HPP

#include "ScriptWithExtensionsBorrowed.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CodePointSetData.hpp"
#include "ScriptExtensionsSet.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    uint16_t icu4x_ScriptWithExtensionsBorrowed_get_script_val_mv1(const icu4x::capi::ScriptWithExtensionsBorrowed* self, char32_t ch);

    icu4x::capi::ScriptExtensionsSet* icu4x_ScriptWithExtensionsBorrowed_get_script_extensions_val_mv1(const icu4x::capi::ScriptWithExtensionsBorrowed* self, char32_t ch);

    bool icu4x_ScriptWithExtensionsBorrowed_has_script_mv1(const icu4x::capi::ScriptWithExtensionsBorrowed* self, char32_t ch, uint16_t script);

    icu4x::capi::CodePointSetData* icu4x_ScriptWithExtensionsBorrowed_get_script_extensions_set_mv1(const icu4x::capi::ScriptWithExtensionsBorrowed* self, uint16_t script);

    void icu4x_ScriptWithExtensionsBorrowed_destroy_mv1(ScriptWithExtensionsBorrowed* self);

    } // extern "C"
} // namespace capi
} // namespace

inline uint16_t icu4x::ScriptWithExtensionsBorrowed::get_script_val(char32_t ch) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensionsBorrowed_get_script_val_mv1(this->AsFFI(),
        ch);
    return result;
}

inline std::unique_ptr<icu4x::ScriptExtensionsSet> icu4x::ScriptWithExtensionsBorrowed::get_script_extensions_val(char32_t ch) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensionsBorrowed_get_script_extensions_val_mv1(this->AsFFI(),
        ch);
    return std::unique_ptr<icu4x::ScriptExtensionsSet>(icu4x::ScriptExtensionsSet::FromFFI(result));
}

inline bool icu4x::ScriptWithExtensionsBorrowed::has_script(char32_t ch, uint16_t script) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensionsBorrowed_has_script_mv1(this->AsFFI(),
        ch,
        script);
    return result;
}

inline std::unique_ptr<icu4x::CodePointSetData> icu4x::ScriptWithExtensionsBorrowed::get_script_extensions_set(uint16_t script) const {
    auto result = icu4x::capi::icu4x_ScriptWithExtensionsBorrowed_get_script_extensions_set_mv1(this->AsFFI(),
        script);
    return std::unique_ptr<icu4x::CodePointSetData>(icu4x::CodePointSetData::FromFFI(result));
}

inline const icu4x::capi::ScriptWithExtensionsBorrowed* icu4x::ScriptWithExtensionsBorrowed::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ScriptWithExtensionsBorrowed*>(this);
}

inline icu4x::capi::ScriptWithExtensionsBorrowed* icu4x::ScriptWithExtensionsBorrowed::AsFFI() {
    return reinterpret_cast<icu4x::capi::ScriptWithExtensionsBorrowed*>(this);
}

inline const icu4x::ScriptWithExtensionsBorrowed* icu4x::ScriptWithExtensionsBorrowed::FromFFI(const icu4x::capi::ScriptWithExtensionsBorrowed* ptr) {
    return reinterpret_cast<const icu4x::ScriptWithExtensionsBorrowed*>(ptr);
}

inline icu4x::ScriptWithExtensionsBorrowed* icu4x::ScriptWithExtensionsBorrowed::FromFFI(icu4x::capi::ScriptWithExtensionsBorrowed* ptr) {
    return reinterpret_cast<icu4x::ScriptWithExtensionsBorrowed*>(ptr);
}

inline void icu4x::ScriptWithExtensionsBorrowed::operator delete(void* ptr) {
    icu4x::capi::icu4x_ScriptWithExtensionsBorrowed_destroy_mv1(reinterpret_cast<icu4x::capi::ScriptWithExtensionsBorrowed*>(ptr));
}


#endif // ICU4X_ScriptWithExtensionsBorrowed_HPP
