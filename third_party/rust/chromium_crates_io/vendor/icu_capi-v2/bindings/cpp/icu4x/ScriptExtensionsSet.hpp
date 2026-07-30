#ifndef ICU4X_ScriptExtensionsSet_HPP
#define ICU4X_ScriptExtensionsSet_HPP

#include "ScriptExtensionsSet.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    bool icu4x_ScriptExtensionsSet_contains_mv1(const icu4x::capi::ScriptExtensionsSet* self, uint16_t script);

    size_t icu4x_ScriptExtensionsSet_count_mv1(const icu4x::capi::ScriptExtensionsSet* self);

    typedef struct icu4x_ScriptExtensionsSet_script_at_mv1_result {union {uint16_t ok; }; bool is_ok;} icu4x_ScriptExtensionsSet_script_at_mv1_result;
    icu4x_ScriptExtensionsSet_script_at_mv1_result icu4x_ScriptExtensionsSet_script_at_mv1(const icu4x::capi::ScriptExtensionsSet* self, size_t index);

    void icu4x_ScriptExtensionsSet_destroy_mv1(ScriptExtensionsSet* self);

    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::ScriptExtensionsSet::contains(uint16_t script) const {
    auto result = icu4x::capi::icu4x_ScriptExtensionsSet_contains_mv1(this->AsFFI(),
        script);
    return result;
}

inline size_t icu4x::ScriptExtensionsSet::count() const {
    auto result = icu4x::capi::icu4x_ScriptExtensionsSet_count_mv1(this->AsFFI());
    return result;
}

inline std::optional<uint16_t> icu4x::ScriptExtensionsSet::script_at(size_t index) const {
    auto result = icu4x::capi::icu4x_ScriptExtensionsSet_script_at_mv1(this->AsFFI(),
        index);
    return result.is_ok ? std::optional<uint16_t>(result.ok) : std::nullopt;
}

inline const icu4x::capi::ScriptExtensionsSet* icu4x::ScriptExtensionsSet::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::ScriptExtensionsSet*>(this);
}

inline icu4x::capi::ScriptExtensionsSet* icu4x::ScriptExtensionsSet::AsFFI() {
    return reinterpret_cast<icu4x::capi::ScriptExtensionsSet*>(this);
}

inline const icu4x::ScriptExtensionsSet* icu4x::ScriptExtensionsSet::FromFFI(const icu4x::capi::ScriptExtensionsSet* ptr) {
    return reinterpret_cast<const icu4x::ScriptExtensionsSet*>(ptr);
}

inline icu4x::ScriptExtensionsSet* icu4x::ScriptExtensionsSet::FromFFI(icu4x::capi::ScriptExtensionsSet* ptr) {
    return reinterpret_cast<icu4x::ScriptExtensionsSet*>(ptr);
}

inline void icu4x::ScriptExtensionsSet::operator delete(void* ptr) {
    icu4x::capi::icu4x_ScriptExtensionsSet_destroy_mv1(reinterpret_cast<icu4x::capi::ScriptExtensionsSet*>(ptr));
}


#endif // ICU4X_ScriptExtensionsSet_HPP
