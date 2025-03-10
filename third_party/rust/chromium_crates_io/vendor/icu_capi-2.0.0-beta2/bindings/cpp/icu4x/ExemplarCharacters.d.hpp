#ifndef icu4x_ExemplarCharacters_D_HPP
#define icu4x_ExemplarCharacters_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"

namespace icu4x {
namespace capi { struct DataProvider; }
class DataProvider;
namespace capi { struct ExemplarCharacters; }
class ExemplarCharacters;
namespace capi { struct Locale; }
class Locale;
class DataError;
}


namespace icu4x {
namespace capi {
    struct ExemplarCharacters;
} // namespace capi
} // namespace

namespace icu4x {
class ExemplarCharacters {
public:

  inline bool contains(std::string_view s) const;

  inline bool contains(char32_t cp) const;

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_main(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_main_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_auxiliary(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_auxiliary_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_punctuation(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_punctuation_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_numbers(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_numbers_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_index(const icu4x::Locale& locale);

  inline static diplomat::result<std::unique_ptr<icu4x::ExemplarCharacters>, icu4x::DataError> create_index_with_provider(const icu4x::DataProvider& provider, const icu4x::Locale& locale);

  inline const icu4x::capi::ExemplarCharacters* AsFFI() const;
  inline icu4x::capi::ExemplarCharacters* AsFFI();
  inline static const icu4x::ExemplarCharacters* FromFFI(const icu4x::capi::ExemplarCharacters* ptr);
  inline static icu4x::ExemplarCharacters* FromFFI(icu4x::capi::ExemplarCharacters* ptr);
  inline static void operator delete(void* ptr);
private:
  ExemplarCharacters() = delete;
  ExemplarCharacters(const icu4x::ExemplarCharacters&) = delete;
  ExemplarCharacters(icu4x::ExemplarCharacters&&) noexcept = delete;
  ExemplarCharacters operator=(const icu4x::ExemplarCharacters&) = delete;
  ExemplarCharacters operator=(icu4x::ExemplarCharacters&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_ExemplarCharacters_D_HPP
