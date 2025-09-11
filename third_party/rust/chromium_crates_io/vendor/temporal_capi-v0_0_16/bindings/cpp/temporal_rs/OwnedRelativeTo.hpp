#ifndef temporal_rs_OwnedRelativeTo_HPP
#define temporal_rs_OwnedRelativeTo_HPP

#include "OwnedRelativeTo.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "../diplomat_runtime.hpp"
#include "PlainDate.hpp"
#include "Provider.hpp"
#include "TemporalError.hpp"
#include "ZonedDateTime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_OwnedRelativeTo_from_utf8_result {union {temporal_rs::capi::OwnedRelativeTo ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf8_result;
    temporal_rs_OwnedRelativeTo_from_utf8_result temporal_rs_OwnedRelativeTo_from_utf8(diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_OwnedRelativeTo_from_utf8_with_provider_result {union {temporal_rs::capi::OwnedRelativeTo ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf8_with_provider_result;
    temporal_rs_OwnedRelativeTo_from_utf8_with_provider_result temporal_rs_OwnedRelativeTo_from_utf8_with_provider(diplomat::capi::DiplomatStringView s, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_OwnedRelativeTo_from_utf16_result {union {temporal_rs::capi::OwnedRelativeTo ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf16_result;
    temporal_rs_OwnedRelativeTo_from_utf16_result temporal_rs_OwnedRelativeTo_from_utf16(diplomat::capi::DiplomatString16View s);

    typedef struct temporal_rs_OwnedRelativeTo_from_utf16_with_provider_result {union {temporal_rs::capi::OwnedRelativeTo ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf16_with_provider_result;
    temporal_rs_OwnedRelativeTo_from_utf16_with_provider_result temporal_rs_OwnedRelativeTo_from_utf16_with_provider(diplomat::capi::DiplomatString16View s, const temporal_rs::capi::Provider* p);

    temporal_rs::capi::OwnedRelativeTo temporal_rs_OwnedRelativeTo_empty(void);

    } // extern "C"
} // namespace capi
} // namespace

inline diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> temporal_rs::OwnedRelativeTo::from_utf8(std::string_view s) {
  auto result = temporal_rs::capi::temporal_rs_OwnedRelativeTo_from_utf8({s.data(), s.size()});
  return result.is_ok ? diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Ok<temporal_rs::OwnedRelativeTo>(temporal_rs::OwnedRelativeTo::FromFFI(result.ok))) : diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> temporal_rs::OwnedRelativeTo::from_utf8_with_provider(std::string_view s, const temporal_rs::Provider& p) {
  auto result = temporal_rs::capi::temporal_rs_OwnedRelativeTo_from_utf8_with_provider({s.data(), s.size()},
    p.AsFFI());
  return result.is_ok ? diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Ok<temporal_rs::OwnedRelativeTo>(temporal_rs::OwnedRelativeTo::FromFFI(result.ok))) : diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> temporal_rs::OwnedRelativeTo::from_utf16(std::u16string_view s) {
  auto result = temporal_rs::capi::temporal_rs_OwnedRelativeTo_from_utf16({s.data(), s.size()});
  return result.is_ok ? diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Ok<temporal_rs::OwnedRelativeTo>(temporal_rs::OwnedRelativeTo::FromFFI(result.ok))) : diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError> temporal_rs::OwnedRelativeTo::from_utf16_with_provider(std::u16string_view s, const temporal_rs::Provider& p) {
  auto result = temporal_rs::capi::temporal_rs_OwnedRelativeTo_from_utf16_with_provider({s.data(), s.size()},
    p.AsFFI());
  return result.is_ok ? diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Ok<temporal_rs::OwnedRelativeTo>(temporal_rs::OwnedRelativeTo::FromFFI(result.ok))) : diplomat::result<temporal_rs::OwnedRelativeTo, temporal_rs::TemporalError>(diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::OwnedRelativeTo temporal_rs::OwnedRelativeTo::empty() {
  auto result = temporal_rs::capi::temporal_rs_OwnedRelativeTo_empty();
  return temporal_rs::OwnedRelativeTo::FromFFI(result);
}


inline temporal_rs::capi::OwnedRelativeTo temporal_rs::OwnedRelativeTo::AsFFI() const {
  return temporal_rs::capi::OwnedRelativeTo {
    /* .date = */ date ? date->AsFFI() : nullptr,
    /* .zoned = */ zoned ? zoned->AsFFI() : nullptr,
  };
}

inline temporal_rs::OwnedRelativeTo temporal_rs::OwnedRelativeTo::FromFFI(temporal_rs::capi::OwnedRelativeTo c_struct) {
  return temporal_rs::OwnedRelativeTo {
    /* .date = */ std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(c_struct.date)),
    /* .zoned = */ std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(c_struct.zoned)),
  };
}


#endif // temporal_rs_OwnedRelativeTo_HPP
