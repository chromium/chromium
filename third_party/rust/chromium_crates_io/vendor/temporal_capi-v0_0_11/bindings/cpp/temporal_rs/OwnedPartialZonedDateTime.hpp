#ifndef temporal_rs_OwnedPartialZonedDateTime_HPP
#define temporal_rs_OwnedPartialZonedDateTime_HPP

#include "OwnedPartialZonedDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "TemporalError.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_OwnedPartialZonedDateTime_from_utf8_result {union {temporal_rs::capi::OwnedPartialZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_OwnedPartialZonedDateTime_from_utf8_result;
    temporal_rs_OwnedPartialZonedDateTime_from_utf8_result temporal_rs_OwnedPartialZonedDateTime_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_OwnedPartialZonedDateTime_from_utf16_result {union {temporal_rs::capi::OwnedPartialZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_OwnedPartialZonedDateTime_from_utf16_result;
    temporal_rs_OwnedPartialZonedDateTime_from_utf16_result temporal_rs_OwnedPartialZonedDateTime_from_utf16(diplomat::capi::DiplomatString16View s);

    void temporal_rs_OwnedPartialZonedDateTime_destroy(OwnedPartialZonedDateTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError> temporal_rs::OwnedPartialZonedDateTime::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_OwnedPartialZonedDateTime_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>>(std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>(temporal_rs::OwnedPartialZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError> temporal_rs::OwnedPartialZonedDateTime::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_OwnedPartialZonedDateTime_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>>(std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>(temporal_rs::OwnedPartialZonedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::OwnedPartialZonedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::OwnedPartialZonedDateTime* temporal_rs::OwnedPartialZonedDateTime::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::OwnedPartialZonedDateTime*>(this);
}

inline temporal_rs::capi::OwnedPartialZonedDateTime* temporal_rs::OwnedPartialZonedDateTime::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::OwnedPartialZonedDateTime*>(this);
}

inline const temporal_rs::OwnedPartialZonedDateTime* temporal_rs::OwnedPartialZonedDateTime::FromFFI(const temporal_rs::capi::OwnedPartialZonedDateTime* ptr) {
  return reinterpret_cast<const temporal_rs::OwnedPartialZonedDateTime*>(ptr);
}

inline temporal_rs::OwnedPartialZonedDateTime* temporal_rs::OwnedPartialZonedDateTime::FromFFI(temporal_rs::capi::OwnedPartialZonedDateTime* ptr) {
  return reinterpret_cast<temporal_rs::OwnedPartialZonedDateTime*>(ptr);
}

inline void temporal_rs::OwnedPartialZonedDateTime::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_OwnedPartialZonedDateTime_destroy(reinterpret_cast<temporal_rs::capi::OwnedPartialZonedDateTime*>(ptr));
}


#endif // temporal_rs_OwnedPartialZonedDateTime_HPP
