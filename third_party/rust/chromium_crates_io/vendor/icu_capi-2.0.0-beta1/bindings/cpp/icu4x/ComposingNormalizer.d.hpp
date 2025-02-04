#ifndef icu4x_ComposingNormalizer_D_HPP
#define icu4x_ComposingNormalizer_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct ComposingNormalizer; }
class ComposingNormalizer;
namespace capi { struct DataProvider; }
class DataProvider;
class DataError;
}


namespace icu4x {
namespace capi {
    struct ComposingNormalizer;
} // namespace capi
} // namespace

namespace icu4x {
class ComposingNormalizer {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError> create_nfc(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::ComposingNormalizer>, icu4x::DataError> create_nfkc(const icu4x::DataProvider& provider);

  inline std::string normalize(std::string_view s) const;

  inline bool is_normalized(std::string_view s) const;

  inline bool is_normalized16(std::u16string_view s) const;

  inline size_t is_normalized_up_to(std::string_view s) const;

  inline size_t is_normalized16_up_to(std::u16string_view s) const;

  inline const icu4x::capi::ComposingNormalizer* AsFFI() const;
  inline icu4x::capi::ComposingNormalizer* AsFFI();
  inline static const icu4x::ComposingNormalizer* FromFFI(const icu4x::capi::ComposingNormalizer* ptr);
  inline static icu4x::ComposingNormalizer* FromFFI(icu4x::capi::ComposingNormalizer* ptr);
  inline static void operator delete(void* ptr);
private:
  ComposingNormalizer() = delete;
  ComposingNormalizer(const icu4x::ComposingNormalizer&) = delete;
  ComposingNormalizer(icu4x::ComposingNormalizer&&) noexcept = delete;
  ComposingNormalizer operator=(const icu4x::ComposingNormalizer&) = delete;
  ComposingNormalizer operator=(icu4x::ComposingNormalizer&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_ComposingNormalizer_D_HPP
