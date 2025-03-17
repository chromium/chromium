#ifndef icu4x_MeasureUnitParser_HPP
#define icu4x_MeasureUnitParser_HPP

#include "MeasureUnitParser.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "MeasureUnit.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    icu4x::capi::MeasureUnit* icu4x_MeasureUnitParser_parse_mv1(const icu4x::capi::MeasureUnitParser* self, diplomat::capi::DiplomatStringView unit_id);
    
    
    void icu4x_MeasureUnitParser_destroy_mv1(MeasureUnitParser* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::MeasureUnit> icu4x::MeasureUnitParser::parse(std::string_view unit_id) const {
  auto result = icu4x::capi::icu4x_MeasureUnitParser_parse_mv1(this->AsFFI(),
    {unit_id.data(), unit_id.size()});
  return std::unique_ptr<icu4x::MeasureUnit>(icu4x::MeasureUnit::FromFFI(result));
}

inline const icu4x::capi::MeasureUnitParser* icu4x::MeasureUnitParser::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::MeasureUnitParser*>(this);
}

inline icu4x::capi::MeasureUnitParser* icu4x::MeasureUnitParser::AsFFI() {
  return reinterpret_cast<icu4x::capi::MeasureUnitParser*>(this);
}

inline const icu4x::MeasureUnitParser* icu4x::MeasureUnitParser::FromFFI(const icu4x::capi::MeasureUnitParser* ptr) {
  return reinterpret_cast<const icu4x::MeasureUnitParser*>(ptr);
}

inline icu4x::MeasureUnitParser* icu4x::MeasureUnitParser::FromFFI(icu4x::capi::MeasureUnitParser* ptr) {
  return reinterpret_cast<icu4x::MeasureUnitParser*>(ptr);
}

inline void icu4x::MeasureUnitParser::operator delete(void* ptr) {
  icu4x::capi::icu4x_MeasureUnitParser_destroy_mv1(reinterpret_cast<icu4x::capi::MeasureUnitParser*>(ptr));
}


#endif // icu4x_MeasureUnitParser_HPP
