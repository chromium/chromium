#ifndef icu4x_UnitsConverterFactory_D_HPP
#define icu4x_UnitsConverterFactory_D_HPP

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
namespace capi { struct MeasureUnit; }
class MeasureUnit;
namespace capi { struct MeasureUnitParser; }
class MeasureUnitParser;
namespace capi { struct UnitsConverter; }
class UnitsConverter;
namespace capi { struct UnitsConverterFactory; }
class UnitsConverterFactory;
class DataError;
}


namespace icu4x {
namespace capi {
    struct UnitsConverterFactory;
} // namespace capi
} // namespace

namespace icu4x {
class UnitsConverterFactory {
public:

  inline static std::unique_ptr<icu4x::UnitsConverterFactory> create();

  inline static diplomat::result<std::unique_ptr<icu4x::UnitsConverterFactory>, icu4x::DataError> create_with_provider(const icu4x::DataProvider& provider);

  inline std::unique_ptr<icu4x::UnitsConverter> converter(const icu4x::MeasureUnit& from, const icu4x::MeasureUnit& to) const;

  inline std::unique_ptr<icu4x::MeasureUnitParser> parser() const;

  inline const icu4x::capi::UnitsConverterFactory* AsFFI() const;
  inline icu4x::capi::UnitsConverterFactory* AsFFI();
  inline static const icu4x::UnitsConverterFactory* FromFFI(const icu4x::capi::UnitsConverterFactory* ptr);
  inline static icu4x::UnitsConverterFactory* FromFFI(icu4x::capi::UnitsConverterFactory* ptr);
  inline static void operator delete(void* ptr);
private:
  UnitsConverterFactory() = delete;
  UnitsConverterFactory(const icu4x::UnitsConverterFactory&) = delete;
  UnitsConverterFactory(icu4x::UnitsConverterFactory&&) noexcept = delete;
  UnitsConverterFactory operator=(const icu4x::UnitsConverterFactory&) = delete;
  UnitsConverterFactory operator=(icu4x::UnitsConverterFactory&&) noexcept = delete;
  static void operator delete[](void*, size_t) = delete;
};

} // namespace
#endif // icu4x_UnitsConverterFactory_D_HPP
