#ifndef ICU4X_Logger_D_HPP
#define ICU4X_Logger_D_HPP

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
    struct Logger;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * An object allowing control over the logging used
 */
class Logger {
public:

  /**
   * Initialize the logger using `simple_logger`
   *
   * Requires the `simple_logger` Cargo feature.
   *
   * Returns `false` if there was already a logger set.
   */
  inline static bool init_simple_logger();

    inline const icu4x::capi::Logger* AsFFI() const;
    inline icu4x::capi::Logger* AsFFI();
    inline static const icu4x::Logger* FromFFI(const icu4x::capi::Logger* ptr);
    inline static icu4x::Logger* FromFFI(icu4x::capi::Logger* ptr);
    inline static void operator delete(void* ptr);
private:
    Logger() = delete;
    Logger(const icu4x::Logger&) = delete;
    Logger(icu4x::Logger&&) noexcept = delete;
    Logger operator=(const icu4x::Logger&) = delete;
    Logger operator=(icu4x::Logger&&) noexcept = delete;
    static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // ICU4X_Logger_D_HPP
