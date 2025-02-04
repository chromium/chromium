#ifndef icu4x_Logger_D_HPP
#define icu4x_Logger_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    struct Logger;
} // namespace capi
} // namespace

namespace icu4x {
class Logger {
public:

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
#endif // icu4x_Logger_D_HPP
