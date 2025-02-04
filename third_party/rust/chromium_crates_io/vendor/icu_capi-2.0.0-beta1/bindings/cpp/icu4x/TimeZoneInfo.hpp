#ifndef icu4x_TimeZoneInfo_HPP
#define icu4x_TimeZoneInfo_HPP

#include "TimeZoneInfo.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <optional>
#include "../diplomat_runtime.hpp"
#include "IsoDateTime.hpp"
#include "TimeZoneIdMapper.hpp"
#include "TimeZoneInvalidOffsetError.hpp"


namespace icu4x {
namespace capi {
    extern "C" {
    
    icu4x::capi::TimeZoneInfo* icu4x_TimeZoneInfo_unknown_mv1(void);
    
    icu4x::capi::TimeZoneInfo* icu4x_TimeZoneInfo_utc_mv1(void);
    
    icu4x::capi::TimeZoneInfo* icu4x_TimeZoneInfo_from_parts_mv1(diplomat::capi::DiplomatStringView bcp47_id, int32_t offset_seconds, bool dst);
    
    typedef struct icu4x_TimeZoneInfo_try_set_offset_seconds_mv1_result { bool is_ok;} icu4x_TimeZoneInfo_try_set_offset_seconds_mv1_result;
    icu4x_TimeZoneInfo_try_set_offset_seconds_mv1_result icu4x_TimeZoneInfo_try_set_offset_seconds_mv1(icu4x::capi::TimeZoneInfo* self, int32_t offset_seconds);
    
    void icu4x_TimeZoneInfo_set_offset_eighths_of_hour_mv1(icu4x::capi::TimeZoneInfo* self, int8_t offset_eighths_of_hour);
    
    typedef struct icu4x_TimeZoneInfo_try_set_offset_str_mv1_result { bool is_ok;} icu4x_TimeZoneInfo_try_set_offset_str_mv1_result;
    icu4x_TimeZoneInfo_try_set_offset_str_mv1_result icu4x_TimeZoneInfo_try_set_offset_str_mv1(icu4x::capi::TimeZoneInfo* self, diplomat::capi::DiplomatStringView offset);
    
    typedef struct icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1_result {union {int8_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1_result;
    icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1_result icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    void icu4x_TimeZoneInfo_clear_offset_mv1(icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_offset_seconds_mv1_result {union {int32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_seconds_mv1_result;
    icu4x_TimeZoneInfo_offset_seconds_mv1_result icu4x_TimeZoneInfo_offset_seconds_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_is_offset_non_negative_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_offset_non_negative_mv1_result;
    icu4x_TimeZoneInfo_is_offset_non_negative_mv1_result icu4x_TimeZoneInfo_is_offset_non_negative_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_is_offset_zero_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_offset_zero_mv1_result;
    icu4x_TimeZoneInfo_is_offset_zero_mv1_result icu4x_TimeZoneInfo_is_offset_zero_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_offset_hours_part_mv1_result {union {int32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_hours_part_mv1_result;
    icu4x_TimeZoneInfo_offset_hours_part_mv1_result icu4x_TimeZoneInfo_offset_hours_part_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_offset_minutes_part_mv1_result {union {uint32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_minutes_part_mv1_result;
    icu4x_TimeZoneInfo_offset_minutes_part_mv1_result icu4x_TimeZoneInfo_offset_minutes_part_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_offset_seconds_part_mv1_result {union {uint32_t ok; }; bool is_ok;} icu4x_TimeZoneInfo_offset_seconds_part_mv1_result;
    icu4x_TimeZoneInfo_offset_seconds_part_mv1_result icu4x_TimeZoneInfo_offset_seconds_part_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    void icu4x_TimeZoneInfo_set_time_zone_id_mv1(icu4x::capi::TimeZoneInfo* self, diplomat::capi::DiplomatStringView id);
    
    void icu4x_TimeZoneInfo_set_iana_time_zone_id_mv1(icu4x::capi::TimeZoneInfo* self, const icu4x::capi::TimeZoneIdMapper* mapper, diplomat::capi::DiplomatStringView id);
    
    void icu4x_TimeZoneInfo_time_zone_id_mv1(const icu4x::capi::TimeZoneInfo* self, diplomat::capi::DiplomatWrite* write);
    
    void icu4x_TimeZoneInfo_clear_zone_variant_mv1(icu4x::capi::TimeZoneInfo* self);
    
    void icu4x_TimeZoneInfo_set_standard_time_mv1(icu4x::capi::TimeZoneInfo* self);
    
    void icu4x_TimeZoneInfo_set_daylight_time_mv1(icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_is_standard_time_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_standard_time_mv1_result;
    icu4x_TimeZoneInfo_is_standard_time_mv1_result icu4x_TimeZoneInfo_is_standard_time_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    typedef struct icu4x_TimeZoneInfo_is_daylight_time_mv1_result {union {bool ok; }; bool is_ok;} icu4x_TimeZoneInfo_is_daylight_time_mv1_result;
    icu4x_TimeZoneInfo_is_daylight_time_mv1_result icu4x_TimeZoneInfo_is_daylight_time_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    void icu4x_TimeZoneInfo_set_local_time_mv1(icu4x::capi::TimeZoneInfo* self, const icu4x::capi::IsoDateTime* datetime);
    
    void icu4x_TimeZoneInfo_clear_local_time_mv1(icu4x::capi::TimeZoneInfo* self);
    
    icu4x::capi::IsoDateTime* icu4x_TimeZoneInfo_get_local_time_mv1(const icu4x::capi::TimeZoneInfo* self);
    
    
    void icu4x_TimeZoneInfo_destroy_mv1(TimeZoneInfo* self);
    
    } // extern "C"
} // namespace capi
} // namespace

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZoneInfo::unknown() {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_unknown_mv1();
  return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZoneInfo::utc() {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_utc_mv1();
  return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline std::unique_ptr<icu4x::TimeZoneInfo> icu4x::TimeZoneInfo::from_parts(std::string_view bcp47_id, int32_t offset_seconds, bool dst) {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_from_parts_mv1({bcp47_id.data(), bcp47_id.size()},
    offset_seconds,
    dst);
  return std::unique_ptr<icu4x::TimeZoneInfo>(icu4x::TimeZoneInfo::FromFFI(result));
}

inline diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError> icu4x::TimeZoneInfo::try_set_offset_seconds(int32_t offset_seconds) {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_try_set_offset_seconds_mv1(this->AsFFI(),
    offset_seconds);
  return result.is_ok ? diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError>(diplomat::Err<icu4x::TimeZoneInvalidOffsetError>(icu4x::TimeZoneInvalidOffsetError {}));
}

inline void icu4x::TimeZoneInfo::set_offset_eighths_of_hour(int8_t offset_eighths_of_hour) {
  icu4x::capi::icu4x_TimeZoneInfo_set_offset_eighths_of_hour_mv1(this->AsFFI(),
    offset_eighths_of_hour);
}

inline diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError> icu4x::TimeZoneInfo::try_set_offset_str(std::string_view offset) {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_try_set_offset_str_mv1(this->AsFFI(),
    {offset.data(), offset.size()});
  return result.is_ok ? diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError>(diplomat::Ok<std::monostate>()) : diplomat::result<std::monostate, icu4x::TimeZoneInvalidOffsetError>(diplomat::Err<icu4x::TimeZoneInvalidOffsetError>(icu4x::TimeZoneInvalidOffsetError {}));
}

inline std::optional<int8_t> icu4x::TimeZoneInfo::offset_eighths_of_hour() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_offset_eighths_of_hour_mv1(this->AsFFI());
  return result.is_ok ? std::optional<int8_t>(result.ok) : std::nullopt;
}

inline void icu4x::TimeZoneInfo::clear_offset() {
  icu4x::capi::icu4x_TimeZoneInfo_clear_offset_mv1(this->AsFFI());
}

inline std::optional<int32_t> icu4x::TimeZoneInfo::offset_seconds() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_offset_seconds_mv1(this->AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline std::optional<bool> icu4x::TimeZoneInfo::is_offset_non_negative() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_is_offset_non_negative_mv1(this->AsFFI());
  return result.is_ok ? std::optional<bool>(result.ok) : std::nullopt;
}

inline std::optional<bool> icu4x::TimeZoneInfo::is_offset_zero() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_is_offset_zero_mv1(this->AsFFI());
  return result.is_ok ? std::optional<bool>(result.ok) : std::nullopt;
}

inline std::optional<int32_t> icu4x::TimeZoneInfo::offset_hours_part() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_offset_hours_part_mv1(this->AsFFI());
  return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline std::optional<uint32_t> icu4x::TimeZoneInfo::offset_minutes_part() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_offset_minutes_part_mv1(this->AsFFI());
  return result.is_ok ? std::optional<uint32_t>(result.ok) : std::nullopt;
}

inline std::optional<uint32_t> icu4x::TimeZoneInfo::offset_seconds_part() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_offset_seconds_part_mv1(this->AsFFI());
  return result.is_ok ? std::optional<uint32_t>(result.ok) : std::nullopt;
}

inline void icu4x::TimeZoneInfo::set_time_zone_id(std::string_view id) {
  icu4x::capi::icu4x_TimeZoneInfo_set_time_zone_id_mv1(this->AsFFI(),
    {id.data(), id.size()});
}

inline void icu4x::TimeZoneInfo::set_iana_time_zone_id(const icu4x::TimeZoneIdMapper& mapper, std::string_view id) {
  icu4x::capi::icu4x_TimeZoneInfo_set_iana_time_zone_id_mv1(this->AsFFI(),
    mapper.AsFFI(),
    {id.data(), id.size()});
}

inline std::string icu4x::TimeZoneInfo::time_zone_id() const {
  std::string output;
  diplomat::capi::DiplomatWrite write = diplomat::WriteFromString(output);
  icu4x::capi::icu4x_TimeZoneInfo_time_zone_id_mv1(this->AsFFI(),
    &write);
  return output;
}

inline void icu4x::TimeZoneInfo::clear_zone_variant() {
  icu4x::capi::icu4x_TimeZoneInfo_clear_zone_variant_mv1(this->AsFFI());
}

inline void icu4x::TimeZoneInfo::set_standard_time() {
  icu4x::capi::icu4x_TimeZoneInfo_set_standard_time_mv1(this->AsFFI());
}

inline void icu4x::TimeZoneInfo::set_daylight_time() {
  icu4x::capi::icu4x_TimeZoneInfo_set_daylight_time_mv1(this->AsFFI());
}

inline std::optional<bool> icu4x::TimeZoneInfo::is_standard_time() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_is_standard_time_mv1(this->AsFFI());
  return result.is_ok ? std::optional<bool>(result.ok) : std::nullopt;
}

inline std::optional<bool> icu4x::TimeZoneInfo::is_daylight_time() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_is_daylight_time_mv1(this->AsFFI());
  return result.is_ok ? std::optional<bool>(result.ok) : std::nullopt;
}

inline void icu4x::TimeZoneInfo::set_local_time(const icu4x::IsoDateTime& datetime) {
  icu4x::capi::icu4x_TimeZoneInfo_set_local_time_mv1(this->AsFFI(),
    datetime.AsFFI());
}

inline void icu4x::TimeZoneInfo::clear_local_time() {
  icu4x::capi::icu4x_TimeZoneInfo_clear_local_time_mv1(this->AsFFI());
}

inline std::unique_ptr<icu4x::IsoDateTime> icu4x::TimeZoneInfo::get_local_time() const {
  auto result = icu4x::capi::icu4x_TimeZoneInfo_get_local_time_mv1(this->AsFFI());
  return std::unique_ptr<icu4x::IsoDateTime>(icu4x::IsoDateTime::FromFFI(result));
}

inline const icu4x::capi::TimeZoneInfo* icu4x::TimeZoneInfo::AsFFI() const {
  return reinterpret_cast<const icu4x::capi::TimeZoneInfo*>(this);
}

inline icu4x::capi::TimeZoneInfo* icu4x::TimeZoneInfo::AsFFI() {
  return reinterpret_cast<icu4x::capi::TimeZoneInfo*>(this);
}

inline const icu4x::TimeZoneInfo* icu4x::TimeZoneInfo::FromFFI(const icu4x::capi::TimeZoneInfo* ptr) {
  return reinterpret_cast<const icu4x::TimeZoneInfo*>(ptr);
}

inline icu4x::TimeZoneInfo* icu4x::TimeZoneInfo::FromFFI(icu4x::capi::TimeZoneInfo* ptr) {
  return reinterpret_cast<icu4x::TimeZoneInfo*>(ptr);
}

inline void icu4x::TimeZoneInfo::operator delete(void* ptr) {
  icu4x::capi::icu4x_TimeZoneInfo_destroy_mv1(reinterpret_cast<icu4x::capi::TimeZoneInfo*>(ptr));
}


#endif // icu4x_TimeZoneInfo_HPP
