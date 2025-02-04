#ifndef icu4x_Collator_D_HPP
#define icu4x_Collator_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Collator; }
class Collator;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct Locale; }
class Locale;
struct CollatorOptionsV1;
struct CollatorResolvedOptionsV1;
class DataError;
}


namespace icu4x {
namespace capi {
    struct Collator;
} // namespace capi
} // namespace

namespace icu4x {
class Collator {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::Collator>, icu4x::DataError> create_v1(const icu4x::DataProvider& provider, const icu4x::Locale& locale, icu4x::CollatorOptionsV1 options);

  inline int8_t compare(std::string_view left, std::string_view right) const;

  inline int8_t compare16(std::u16string_view left, std::u16string_view right) const;

  inline icu4x::CollatorResolvedOptionsV1 resolved_options_v1() const;

  inline const icu4x::capi::Collator* AsFFI() const;
  inline icu4x::capi::Collator* AsFFI();
  inline static const icu4x::Collator* FromFFI(const icu4x::capi::Collator* ptr);
  inline static icu4x::Collator* FromFFI(icu4x::capi::Collator* ptr);
  inline static void operator delete(void* ptr);
private:
  Collator() = delete;
  Collator(const icu4x::Collator&) = delete;
  Collator(icu4x::Collator&&) noexcept = delete;
  Collator operator=(const icu4x::Collator&) = delete;
  Collator operator=(icu4x::Collator&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Collator_D_HPP
