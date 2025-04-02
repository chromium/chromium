#ifndef icu4x_UnitsConverterFactory_HPP
#define icu4x_UnitsConverterFactory_HPP

#include "UnitsConverterFactory.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"
#include "MeasureUnit.hpp"
#include "MeasureUnitParser.hpp"
#include "UnitsConverter.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    icu4x::capi::UnitsConverterFactory* icu4x_UnitsConverterFactory_create_mv1(void);
    
    typedef struct icu4x_UnitsConverterFactory_create_with_provider_mv1_result {union {icu4x::capi::UnitsConverterFactory* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_UnitsConverterFactory_create_with_provider_mv1_result;
    icu4x_UnitsConverterFactory_create_with_provider_mv1_result icu4x_UnitsConverterFactory_create_with_provider_mv1(const icu4x::capi::DataProvider* provider);
    
    icu4x::capi::UnitsConverter* icu4x_UnitsConverterFactory_converter_mv1(const icu4x::capi::UnitsConverterFactory* self, const icu4x::capi::MeasureUnit* from, const icu4x::capi::MeasureUnit* to);
    
    icu4x::capi::MeasureUnitParser* icu4x_UnitsConverterFactory_parser_mv1(const icu4x::capi::UnitsConverterFactory* self);
    
    
    void icu4x_UnitsConverterFactory_destroy_mv1(UnitsConverterFactory* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::UnitsConverterFactory> icu4x::UnitsConverterFactory::create() {
  auto result = icu4x::capi::icu4x_UnitsConverterFactory_create_mv1();
  return std::unique_ptr<icu4x::UnitsConverterFactory>(icu4x::UnitsConverterFactory::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::UnitsConverterFactory>, icu4x::DataError> icu4x::UnitsConverterFactory::create_with_provider(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_UnitsConverterFactory_create_with_provider_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::UnitsConverterFactory>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::UnitsConverterFactory>>(std::unique_ptr<icu4x::UnitsConverterFactory>(icu4x::UnitsConverterFactory::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::UnitsConverterFactory>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::UnitsConverter> icu4x::UnitsConverterFactory::converter(const icu4x::MeasureUnit& from, const icu4x::MeasureUnit& to) const {
  auto result = icu4x::capi::icu4x_UnitsConverterFactory_converter_mv1(this->AsFFI(),
    from.AsFFI(),
    to.AsFFI());
  return std::unique_ptr<icu4x::UnitsConverter>(icu4x::UnitsConverter::FromFFI(result));
}

inline std::unique_ptr<icu4x::MeasureUnitParser> icu4x::UnitsConverterFactory::parser() const {
  auto result = icu4x::capi::icu4x_UnitsConverterFactory_parser_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::MeasureUnitParser>(icu4x::MeasureUnitParser::FromFFI(result));
}

inline const icu4x::capi::UnitsConverterFactory* icu4x::UnitsConverterFactory::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::UnitsConverterFactory*>(this);
}

inline icu4x::capi::UnitsConverterFactory* icu4x::UnitsConverterFactory::AsFFI() {
  return reinterpret_cast<icu4x::capi::UnitsConverterFactory*>(this);
}

inline const icu4x::UnitsConverterFactory* icu4x::UnitsConverterFactory::FromFFI(const icu4x::capi::UnitsConverterFactory* ptr) {
  return reinterpret_cast<const icu4x::UnitsConverterFactory*>(ptr);
}

inline icu4x::UnitsConverterFactory* icu4x::UnitsConverterFactory::FromFFI(icu4x::capi::UnitsConverterFactory* ptr) {
  return reinterpret_cast<icu4x::UnitsConverterFactory*>(ptr);
}

inline void icu4x::UnitsConverterFactory::operator delete(void* ptr) {
  icu4x::capi::icu4x_UnitsConverterFactory_destroy_mv1(reinterpret_cast<icu4x::capi::UnitsConverterFactory*>(ptr));
}


#endif // icu4x_UnitsConverterFactory_HPP
