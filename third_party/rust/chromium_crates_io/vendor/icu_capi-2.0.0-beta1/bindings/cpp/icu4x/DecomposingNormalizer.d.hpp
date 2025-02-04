#ifndef icu4x_DecomposingNormalizer_D_HPP
#define icu4x_DecomposingNormalizer_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct DecomposingNormalizer; }
class DecomposingNormalizer;
class DataError;
}


namespace icu4x {
namespace capi {
    struct DecomposingNormalizer;
} // namespace capi
} // namespace

namespace icu4x {
class DecomposingNormalizer {
public:

  inline static diplomat::result<std::unique_ptr<icu4x::DecomposingNormalizer>, icu4x::DataError> create_nfd(const icu4x::DataProvider& provider);

  inline static diplomat::result<std::unique_ptr<icu4x::DecomposingNormalizer>, icu4x::DataError> create_nfkd(const icu4x::DataProvider& provider);

  inline std::string normalize(std::string_view s) const;

  inline bool is_normalized(std::string_view s) const;

  inline bool is_normalized_utf16(std::u16string_view s) const;

  inline size_t is_normalized_up_to(std::string_view s) const;

  inline size_t is_normalized_utf16_up_to(std::u16string_view s) const;

  inline const icu4x::capi::DecomposingNormalizer* AsFFI() const;
  inline icu4x::capi::DecomposingNormalizer* AsFFI();
  inline static const icu4x::DecomposingNormalizer* FromFFI(const icu4x::capi::DecomposingNormalizer* ptr);
  inline static icu4x::DecomposingNormalizer* FromFFI(icu4x::capi::DecomposingNormalizer* ptr);
  inline static void operator delete(void* ptr);
private:
  DecomposingNormalizer() = delete;
  DecomposingNormalizer(const icu4x::DecomposingNormalizer&) = delete;
  DecomposingNormalizer(icu4x::DecomposingNormalizer&&) noexcept = delete;
  DecomposingNormalizer operator=(const icu4x::DecomposingNormalizer&) = delete;
  DecomposingNormalizer operator=(icu4x::DecomposingNormalizer&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_DecomposingNormalizer_D_HPP
