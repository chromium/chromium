#ifndef icu4x_PluralOperands_D_HPP
#define icu4x_PluralOperands_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Decimal; }
class Decimal;
namespace capi { struct PluralOperands; }
class PluralOperands;
class FixedDecimalParseError;
}


namespace icu4x {
namespace capi {
    struct PluralOperands;
} // namespace capi
} // namespace

namespace icu4x {
class PluralOperands {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::PluralOperands>, icu4x::FixedDecimalParseError> from_string(std::string_view s);

  inline static std::unique_ptr<icu4x::PluralOperands> from(int64_t i);

  inline static std::unique_ptr<icu4x::PluralOperands> from_fixed_decimal(const icu4x::Decimal& x);

  inline const icu4x::capi::PluralOperands* AsFFI() const;
  inline icu4x::capi::PluralOperands* AsFFI();
  inline static const icu4x::PluralOperands* FromFFI(const icu4x::capi::PluralOperands* ptr);
  inline static icu4x::PluralOperands* FromFFI(icu4x::capi::PluralOperands* ptr);
  inline static void operator delete(void* ptr);
private:
  PluralOperands() = delete;
  PluralOperands(const icu4x::PluralOperands&) = delete;
  PluralOperands(icu4x::PluralOperands&&) noexcept = delete;
  PluralOperands operator=(const icu4x::PluralOperands&) = delete;
  PluralOperands operator=(icu4x::PluralOperands&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_PluralOperands_D_HPP
