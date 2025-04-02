#ifndef icu4x_Bidi_D_HPP
#define icu4x_Bidi_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct Bidi; }
class Bidi;
namespace capi { struct BidiInfo; }
class BidiInfo;
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct ReorderedIndexMap; }
class ReorderedIndexMap;
class DataError;
}


namespace icu4x {
namespace capi {
    struct Bidi;
} // namespace capi
} // namespace

namespace icu4x {
class Bidi {
public:

  inline static std::unique_ptr<icu4x::Bidi> create();

  inline static diplomat::result<std::unique_ptr<icu4x::Bidi>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline std::unique_ptr<icu4x::BidiInfo> for_text(std::string_view text, std::optional<uint8_t> default_level) const;

  inline std::unique_ptr<icu4x::ReorderedIndexMap> reorder_visual(diplomat::span<const uint8_t> levels) const;

  inline static bool level_is_rtl(uint8_t level);

  inline static bool level_is_ltr(uint8_t level);

  inline static uint8_t level_rtl();

  inline static uint8_t level_ltr();

  inline const icu4x::capi::Bidi* AsFFI() const;
  inline icu4x::capi::Bidi* AsFFI();
  inline static const icu4x::Bidi* FromFFI(const icu4x::capi::Bidi* ptr);
  inline static icu4x::Bidi* FromFFI(icu4x::capi::Bidi* ptr);
  inline static void operator delete(void* ptr);
private:
  Bidi() = delete;
  Bidi(const icu4x::Bidi&) = delete;
  Bidi(icu4x::Bidi&&) noexcept = delete;
  Bidi operator=(const icu4x::Bidi&) = delete;
  Bidi operator=(icu4x::Bidi&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_Bidi_D_HPP
