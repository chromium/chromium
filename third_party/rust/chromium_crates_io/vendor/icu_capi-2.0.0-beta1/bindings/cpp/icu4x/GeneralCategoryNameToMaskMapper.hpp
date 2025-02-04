#ifndef icu4x_GeneralCategoryNameToMaskMapper_HPP
#define icu4x_GeneralCategoryNameToMaskMapper_HPP

#include "GeneralCategoryNameToMaskMapper.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "DataError.hpp"
#include "DataProvider.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    uint32_t icu4x_GeneralCategoryNameToMaskMapper_get_strict_mv1(const icu4x::capi::GeneralCategoryNameToMaskMapper* self, diplomat::capi::DiplomatStringView name);
    
    uint32_t icu4x_GeneralCategoryNameToMaskMapper_get_loose_mv1(const icu4x::capi::GeneralCategoryNameToMaskMapper* self, diplomat::capi::DiplomatStringView name);
    
    typedef struct icu4x_GeneralCategoryNameToMaskMapper_load_mv1_result {union {icu4x::capi::GeneralCategoryNameToMaskMapper* ok; icu4x::capi::DataError err;}; bool is_ok;} icu4x_GeneralCategoryNameToMaskMapper_load_mv1_result;
    icu4x_GeneralCategoryNameToMaskMapper_load_mv1_result icu4x_GeneralCategoryNameToMaskMapper_load_mv1(const icu4x::capi::DataProvider* provider);
    
    
    void icu4x_GeneralCategoryNameToMaskMapper_destroy_mv1(GeneralCategoryNameToMaskMapper* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline uint32_t icu4x::GeneralCategoryNameToMaskMapper::get_strict(std::string_view name) const {
  auto result = icu4x::capi::icu4x_GeneralCategoryNameToMaskMapper_get_strict_mv1(this->AsFFI(),
    {name.data(), name.size()});
  return result;
}

inline uint32_t icu4x::GeneralCategoryNameToMaskMapper::get_loose(std::string_view name) const {
  auto result = icu4x::capi::icu4x_GeneralCategoryNameToMaskMapper_get_loose_mv1(this->AsFFI(),
    {name.data(), name.size()});
  return result;
}

inline diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToMaskMapper>, icu4x::DataError> icu4x::GeneralCategoryNameToMaskMapper::load(const icu4x::DataProvider& provider) {
  auto result = icu4x::capi::icu4x_GeneralCategoryNameToMaskMapper_load_mv1(provider.AsFFI());
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToMaskMapper>, icu4x::DataError>(diplomat::Ok<std::unique_ptr<icu4x::GeneralCategoryNameToMaskMapper>>(std::unique_ptr<icu4x::GeneralCategoryNameToMaskMapper>(icu4x::GeneralCategoryNameToMaskMapper::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::GeneralCategoryNameToMaskMapper>, icu4x::DataError>(diplomat::Err<icu4x::DataError>(icu4x::DataError::FromFFI(result.err)));
}

inline const icu4x::capi::GeneralCategoryNameToMaskMapper* icu4x::GeneralCategoryNameToMaskMapper::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::GeneralCategoryNameToMaskMapper*>(this);
}

inline icu4x::capi::GeneralCategoryNameToMaskMapper* icu4x::GeneralCategoryNameToMaskMapper::AsFFI() {
  return reinterpret_cast<icu4x::capi::GeneralCategoryNameToMaskMapper*>(this);
}

inline const icu4x::GeneralCategoryNameToMaskMapper* icu4x::GeneralCategoryNameToMaskMapper::FromFFI(const icu4x::capi::GeneralCategoryNameToMaskMapper* ptr) {
  return reinterpret_cast<const icu4x::GeneralCategoryNameToMaskMapper*>(ptr);
}

inline icu4x::GeneralCategoryNameToMaskMapper* icu4x::GeneralCategoryNameToMaskMapper::FromFFI(icu4x::capi::GeneralCategoryNameToMaskMapper* ptr) {
  return reinterpret_cast<icu4x::GeneralCategoryNameToMaskMapper*>(ptr);
}

inline void icu4x::GeneralCategoryNameToMaskMapper::operator delete(void* ptr) {
  icu4x::capi::icu4x_GeneralCategoryNameToMaskMapper_destroy_mv1(reinterpret_cast<icu4x::capi::GeneralCategoryNameToMaskMapper*>(ptr));
}


#endif // icu4x_GeneralCategoryNameToMaskMapper_HPP
