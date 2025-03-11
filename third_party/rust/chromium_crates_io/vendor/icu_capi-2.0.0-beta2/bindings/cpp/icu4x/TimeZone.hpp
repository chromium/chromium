#ifndef icu4x_TimeZone_HPP
#define icu4x_TimeZone_HPP

#include "TimeZone.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "TimeZoneInfo.hpp"
#include "UtcOffset.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    icu4x::capi::TimeZone* icu4x_TimeZone_unknown_mv1(void);
    
    icu4x::capi::TimeZone* icu4x_TimeZone_create_from_bcp47_mv1(diplomat::capi::DiplomatStringView id);
    
    icu4x::capi::TimeZoneInfo* icu4x_TimeZone_with_offset_mv1(const icu4x::capi::TimeZone* self, const icu4x::capi::UtcOffset* offset);
    
    icu4x::capi::TimeZoneInfo* icu4x_TimeZone_without_offset_mv1(const icu4x::capi::TimeZone* self);
    
    
    void icu4x_TimeZone_destroy_mv1(TimeZone* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::unknown() {
  auto result = icu4x::capi::icu4x_TimeZone_unknown_mv1();
  return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZone> icu4x::TimeZone::create_from_bcp47(std::string_view id) {
  auto result = icu4x::capi::icu4x_TimeZone_create_from_bcp47_mv1({id.data(), id.size()});
  return std::unique_ptr<icu4x::TimeZone>(icu4x::TimeZone::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZone::with_offset(const icu4x::UtcOffset& offset) const {
  auto result = icu4x::capi::icu4x_TimeZone_with_offset_mv1(this->AsFFI(),
    offset.AsFFI());
  return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZone::without_offset() const {
  auto result = icu4x::capi::icu4x_TimeZone_without_offset_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline const icu4x::capi::TimeZone* icu4x::TimeZone::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::TimeZone*>(this);
}

inline icu4x::capi::TimeZone* icu4x::TimeZone::AsFFI() {
  return reinterpret_cast<icu4x::capi::TimeZone*>(this);
}

inline const icu4x::TimeZone* icu4x::TimeZone::FromFFI(const icu4x::capi::TimeZone* ptr) {
  return reinterpret_cast<const icu4x::TimeZone*>(ptr);
}

inline icu4x::TimeZone* icu4x::TimeZone::FromFFI(icu4x::capi::TimeZone* ptr) {
  return reinterpret_cast<icu4x::TimeZone*>(ptr);
}

inline void icu4x::TimeZone::operator delete(void* ptr) {
  icu4x::capi::icu4x_TimeZone_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZone*>(ptr));
}


#endif // icu4x_TimeZone_HPP
