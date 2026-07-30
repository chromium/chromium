#ifndef ICU4X_CaseMapLocales_HPP
#define ICU4X_CaseMapLocales_HPP

#include "CaseMapLocales.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Locale.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    const icu4x::capi::Locale* icu4x_CaseMapLocales_az_mv1(void);

    const icu4x::capi::Locale* icu4x_CaseMapLocales_el_mv1(void);

    const icu4x::capi::Locale* icu4x_CaseMapLocales_hy_mv1(void);

    const icu4x::capi::Locale* icu4x_CaseMapLocales_lt_mv1(void);

    const icu4x::capi::Locale* icu4x_CaseMapLocales_nl_mv1(void);

    const icu4x::capi::Locale* icu4x_CaseMapLocales_tr_mv1(void);

    void icu4x_CaseMapLocales_destroy_mv1(CaseMapLocales* self);

    } // extern "C"
} // namespace capi
} // namespace

inline const icu4x::Locale& icu4x::CaseMapLocales::az() {
    auto result = icu4x::capi::icu4x_CaseMapLocales_az_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline const icu4x::Locale& icu4x::CaseMapLocales::el() {
    auto result = icu4x::capi::icu4x_CaseMapLocales_el_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline const icu4x::Locale& icu4x::CaseMapLocales::hy() {
    auto result = icu4x::capi::icu4x_CaseMapLocales_hy_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline const icu4x::Locale& icu4x::CaseMapLocales::lt() {
    auto result = icu4x::capi::icu4x_CaseMapLocales_lt_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline const icu4x::Locale& icu4x::CaseMapLocales::nl() {
    auto result = icu4x::capi::icu4x_CaseMapLocales_nl_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline const icu4x::Locale& icu4x::CaseMapLocales::tr() {
    auto result = icu4x::capi::icu4x_CaseMapLocales_tr_mv1();
    return *icu4x::Locale::FromFFI(result);
}

inline const icu4x::capi::CaseMapLocales* icu4x::CaseMapLocales::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::CaseMapLocales*>(this);
}

inline icu4x::capi::CaseMapLocales* icu4x::CaseMapLocales::AsFFI() {
    return reinterpret_cast<icu4x::capi::CaseMapLocales*>(this);
}

inline const icu4x::CaseMapLocales* icu4x::CaseMapLocales::FromFFI(const icu4x::capi::CaseMapLocales* ptr) {
    return reinterpret_cast<const icu4x::CaseMapLocales*>(ptr);
}

inline icu4x::CaseMapLocales* icu4x::CaseMapLocales::FromFFI(icu4x::capi::CaseMapLocales* ptr) {
    return reinterpret_cast<icu4x::CaseMapLocales*>(ptr);
}

inline void icu4x::CaseMapLocales::operator delete(void* ptr) {
    icu4x::capi::icu4x_CaseMapLocales_destroy_mv1(reinterpret_cast<icu4x::capi::CaseMapLocales*>(ptr));
}


#endif // ICU4X_CaseMapLocales_HPP
