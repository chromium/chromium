#ifndef ICU4X_Decomposed_HPP
#define ICU4X_Decomposed_HPP

#include "Decomposed.d.hpp"

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

} // namespace capi
} // namespace


inline icu4x::capi::Decomposed icu4x::Decomposed::AsFFI() const {
    return icu4x::capi::Decomposed {
        /* .first = */ first,
        /* .second = */ second,
    };
}

inline icu4x::Decomposed icu4x::Decomposed::FromFFI(icu4x::capi::Decomposed c_struct) {
    return icu4x::Decomposed {
        /* .first = */ c_struct.first,
        /* .second = */ c_struct.second,
    };
}


#endif // ICU4X_Decomposed_HPP
