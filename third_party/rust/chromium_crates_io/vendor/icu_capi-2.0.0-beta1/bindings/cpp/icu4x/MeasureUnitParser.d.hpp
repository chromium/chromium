#ifndef icu4x_MeasureUnitParser_D_HPP
#define icu4x_MeasureUnitParser_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct MeasureUnit; }
class MeasureUnit;
}


namespace icu4x {
namespace capi {
    struct MeasureUnitParser;
} // namespace capi
} // namespace

namespace icu4x {
class MeasureUnitParser {
public:

  inline std::unique_ptr<icu4x::MeasureUnit> parse(std::string_view unit_id) const;

  inline const icu4x::capi::MeasureUnitParser* AsFFI() const;
  inline icu4x::capi::MeasureUnitParser* AsFFI();
  inline static const icu4x::MeasureUnitParser* FromFFI(const icu4x::capi::MeasureUnitParser* ptr);
  inline static icu4x::MeasureUnitParser* FromFFI(icu4x::capi::MeasureUnitParser* ptr);
  inline static void operator delete(void* ptr);
private:
  MeasureUnitParser() = delete;
  MeasureUnitParser(const icu4x::MeasureUnitParser&) = delete;
  MeasureUnitParser(icu4x::MeasureUnitParser&&) noexcept = delete;
  MeasureUnitParser operator=(const icu4x::MeasureUnitParser&) = delete;
  MeasureUnitParser operator=(icu4x::MeasureUnitParser&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_MeasureUnitParser_D_HPP
