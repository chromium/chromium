#ifndef temporal_rs_ParsedDateTime_HPP
#define temporal_rs_ParsedDateTime_HPP

#include "ParsedDateTime.d.hpp"

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

    typedef struct temporal_rs_ParsedDateTime_from_utf8_result {union {temporal_rs::capi::ParsedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDateTime_from_utf8_result;
    temporal_rs_ParsedDateTime_from_utf8_result temporal_rs_ParsedDateTime_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_ParsedDateTime_from_utf16_result {union {temporal_rs::capi::ParsedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDateTime_from_utf16_result;
    temporal_rs_ParsedDateTime_from_utf16_result temporal_rs_ParsedDateTime_from_utf16(diplomat::capi::DiplomatString16View s);

    void temporal_rs_ParsedDateTime_destroy(ParsedDateTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError> temporal_rs::ParsedDateTime::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_ParsedDateTime_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDateTime>>(std::unique_ptr<temporal_rs::ParsedDateTime>(temporal_rs::ParsedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError> temporal_rs::ParsedDateTime::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_ParsedDateTime_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError>(diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDateTime>>(std::unique_ptr<temporal_rs::ParsedDateTime>(temporal_rs::ParsedDateTime::FromFFI(result.ok)))) : diplomat::result<std::unique_ptr<temporal_rs::ParsedDateTime>, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::ParsedDateTime* temporal_rs::ParsedDateTime::AsFFI() const {
  return reinterpret_cast<const temporal_rs::capi::ParsedDateTime*>(this);
}

inline temporal_rs::capi::ParsedDateTime* temporal_rs::ParsedDateTime::AsFFI() {
  return reinterpret_cast<temporal_rs::capi::ParsedDateTime*>(this);
}

inline const temporal_rs::ParsedDateTime* temporal_rs::ParsedDateTime::FromFFI(const temporal_rs::capi::ParsedDateTime* ptr) {
  return reinterpret_cast<const temporal_rs::ParsedDateTime*>(ptr);
}

inline temporal_rs::ParsedDateTime* temporal_rs::ParsedDateTime::FromFFI(temporal_rs::capi::ParsedDateTime* ptr) {
  return reinterpret_cast<temporal_rs::ParsedDateTime*>(ptr);
}

inline void temporal_rs::ParsedDateTime::operator delete(void* ptr) {
  temporal_rs::capi::temporal_rs_ParsedDateTime_destroy(reinterpret_cast<temporal_rs::capi::ParsedDateTime*>(ptr));
}


#endif // temporal_rs_ParsedDateTime_HPP
