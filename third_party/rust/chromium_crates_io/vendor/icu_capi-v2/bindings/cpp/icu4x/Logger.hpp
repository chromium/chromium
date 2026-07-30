#ifndef ICU4X_Logger_HPP
#define ICU4X_Logger_HPP

#include "Logger.d.hpp"

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

    bool icu4x_Logger_init_simple_logger_mv1(void);

    void icu4x_Logger_destroy_mv1(Logger* self);

    } // extern "C"
} // namespace capi
} // namespace

inline bool icu4x::Logger::init_simple_logger() {
    auto result = icu4x::capi::icu4x_Logger_init_simple_logger_mv1();
    return result;
}

inline const icu4x::capi::Logger* icu4x::Logger::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Logger*>(this);
}

inline icu4x::capi::Logger* icu4x::Logger::AsFFI() {
    return reinterpret_cast<icu4x::capi::Logger*>(this);
}

inline const icu4x::Logger* icu4x::Logger::FromFFI(const icu4x::capi::Logger* ptr) {
    return reinterpret_cast<const icu4x::Logger*>(ptr);
}

inline icu4x::Logger* icu4x::Logger::FromFFI(icu4x::capi::Logger* ptr) {
    return reinterpret_cast<icu4x::Logger*>(ptr);
}

inline void icu4x::Logger::operator delete(void* ptr) {
    icu4x::capi::icu4x_Logger_destroy_mv1(reinterpret_cast<icu4x::capi::Logger*>(ptr));
}


#endif // ICU4X_Logger_HPP
