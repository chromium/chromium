#ifndef icu4x_UtcOffset_HPP
#define icu4x_UtcOffset_HPP

#include "UtcOffset.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "TimeZoneInvalidOffsetError.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    typedef struct icu4x_UtcOffset_from_seconds_mv1_result {union {icu4x::capi::UtcOffset* ok; }; bool is_ok;} icu4x_UtcOffset_from_seconds_mv1_result;
    icu4x_UtcOffset_from_seconds_mv1_result icu4x_UtcOffset_from_seconds_mv1(int32_t seconds);
    
    icu4x::capi::UtcOffset* icu4x_UtcOffset_from_eighths_of_hour_mv1(int8_t eighths_of_hour);
    
    typedef struct icu4x_UtcOffset_from_string_mv1_result {union {icu4x::capi::UtcOffset* ok; }; bool is_ok;} icu4x_UtcOffset_from_string_mv1_result;
    icu4x_UtcOffset_from_string_mv1_result icu4x_UtcOffset_from_string_mv1(diplomat::capi::DiplomatStringView offset);
    
    int8_t icu4x_UtcOffset_eighths_of_hour_mv1(const icu4x::capi::UtcOffset* self);
    
    int32_t icu4x_UtcOffset_seconds_mv1(const icu4x::capi::UtcOffset* self);
    
    bool icu4x_UtcOffset_is_non_negative_mv1(const icu4x::capi::UtcOffset* self);
    
    bool icu4x_UtcOffset_is_zero_mv1(const icu4x::capi::UtcOffset* self);
    
    int32_t icu4x_UtcOffset_hours_part_mv1(const icu4x::capi::UtcOffset* self);
    
    uint32_t icu4x_UtcOffset_minutes_part_mv1(const icu4x::capi::UtcOffset* self);
    
    uint32_t icu4x_UtcOffset_seconds_part_mv1(const icu4x::capi::UtcOffset* self);
    
    
    void icu4x_UtcOffset_destroy_mv1(UtcOffset* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError> icu4x::UtcOffset::from_seconds(int32_t seconds) {
  auto result = icu4x::capi::icu4x_UtcOffset_from_seconds_mv1(seconds);
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError>(diplomat::Ok<std::unique_ptr<icu4x::UtcOffset>>(std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError>(diplomat::Err<icu4x::TimeZoneInvalidOffsetError>(icu4x::TimeZoneInvalidOffsetError {}));
}

inline std::unique_ptr<icu4x::UtcOffset> icu4x::UtcOffset::from_eighths_of_hour(int8_t eighths_of_hour) {
  auto result = icu4x::capi::icu4x_UtcOffset_from_eighths_of_hour_mv1(eighths_of_hour);
  return std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(result));
}

inline diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError> icu4x::UtcOffset::from_string(std::string_view offset) {
  auto result = icu4x::capi::icu4x_UtcOffset_from_string_mv1({offset.data(), offset.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError>(diplomat::Ok<std::unique_ptr<icu4x::UtcOffset>>(std::unique_ptr<icu4x::UtcOffset>(icu4x::UtcOffset::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<icu4x::UtcOffset>, icu4x::TimeZoneInvalidOffsetError>(diplomat::Err<icu4x::TimeZoneInvalidOffsetError>(icu4x::TimeZoneInvalidOffsetError {}));
}

inline int8_t icu4x::UtcOffset::eighths_of_hour() const {
  auto result = icu4x::capi::icu4x_UtcOffset_eighths_of_hour_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::UtcOffset::seconds() const {
  auto result = icu4x::capi::icu4x_UtcOffset_seconds_mv1(this->AsFFI());
  return result;
}

inline bool icu4x::UtcOffset::is_non_negative() const {
  auto result = icu4x::capi::icu4x_UtcOffset_is_non_negative_mv1(this->AsFFI());
  return result;
}

inline bool icu4x::UtcOffset::is_zero() const {
  auto result = icu4x::capi::icu4x_UtcOffset_is_zero_mv1(this->AsFFI());
  return result;
}

inline int32_t icu4x::UtcOffset::hours_part() const {
  auto result = icu4x::capi::icu4x_UtcOffset_hours_part_mv1(this->AsFFI());
  return result;
}

inline uint32_t icu4x::UtcOffset::minutes_part() const {
  auto result = icu4x::capi::icu4x_UtcOffset_minutes_part_mv1(this->AsFFI());
  return result;
}

inline uint32_t icu4x::UtcOffset::seconds_part() const {
  auto result = icu4x::capi::icu4x_UtcOffset_seconds_part_mv1(this->AsFFI());
  return result;
}

inline const icu4x::capi::UtcOffset* icu4x::UtcOffset::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::UtcOffset*>(this);
}

inline icu4x::capi::UtcOffset* icu4x::UtcOffset::AsFFI() {
  return reinterpret_cast<icu4x::capi::UtcOffset*>(this);
}

inline const icu4x::UtcOffset* icu4x::UtcOffset::FromFFI(const icu4x::capi::UtcOffset* ptr) {
  return reinterpret_cast<const icu4x::UtcOffset*>(ptr);
}

inline icu4x::UtcOffset* icu4x::UtcOffset::FromFFI(icu4x::capi::UtcOffset* ptr) {
  return reinterpret_cast<icu4x::UtcOffset*>(ptr);
}

inline void icu4x::UtcOffset::operator delete(void* ptr) {
  icu4x::capi::icu4x_UtcOffset_destroy_mv1(reinterpret_cast<icu4x::capi::UtcOffset*>(ptr));
}


#endif // icu4x_UtcOffset_HPP
