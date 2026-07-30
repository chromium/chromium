#ifndef ICU4X_Time_HPP
#define ICU4X_Time_HPP

#include "Time.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "CalendarError.hpp"
#include "Rfc9557ParseError.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_Time_create_mv1_result {union {icu4x::capi::Time* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Time_create_mv1_result;
    icu4x_Time_create_mv1_result icu4x_Time_create_mv1(uint8_t hour, uint8_t minute, uint8_t second, uint32_t subsecond);

    typedef struct icu4x_Time_from_string_mv1_result {union {icu4x::capi::Time* ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_Time_from_string_mv1_result;
    icu4x_Time_from_string_mv1_result icu4x_Time_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v);

    typedef struct icu4x_Time_start_of_day_mv1_result {union {icu4x::capi::Time* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Time_start_of_day_mv1_result;
    icu4x_Time_start_of_day_mv1_result icu4x_Time_start_of_day_mv1(void);

    typedef struct icu4x_Time_noon_mv1_result {union {icu4x::capi::Time* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Time_noon_mv1_result;
    icu4x_Time_noon_mv1_result icu4x_Time_noon_mv1(void);

    uint8_t icu4x_Time_hour_mv1(const icu4x::capi::Time* self);

    uint8_t icu4x_Time_minute_mv1(const icu4x::capi::Time* self);

    uint8_t icu4x_Time_second_mv1(const icu4x::capi::Time* self);

    uint32_t icu4x_Time_subsecond_mv1(const icu4x::capi::Time* self);

    void icu4x_Time_destroy_mv1(Time* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> icu4x::Time::create(uint8_t hour, uint8_t minute, uint8_t second, uint32_t subsecond) {
    auto result = icu4x::capi::icu4x_Time_create_mv1(hour,
        minute,
        second,
        subsecond);
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Time>>(std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError>(icu4x::diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::Rfc9557ParseError> icu4x::Time::from_string(std::string_view v) {
    auto result = icu4x::capi::icu4x_Time_from_string_mv1({v.data(), v.size()});
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Time>>(std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> icu4x::Time::start_of_day() {
    auto result = icu4x::capi::icu4x_Time_start_of_day_mv1();
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Time>>(std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError>(icu4x::diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError> icu4x::Time::noon() {
    auto result = icu4x::capi::icu4x_Time_noon_mv1();
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Time>>(std::unique_ptr<icu4x::Time>(icu4x::Time::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Time>, icu4x::CalendarError>(icu4x::diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline uint8_t icu4x::Time::hour() const {
    auto result = icu4x::capi::icu4x_Time_hour_mv1(this->AsFFI());
    return result;
}

inline uint8_t icu4x::Time::minute() const {
    auto result = icu4x::capi::icu4x_Time_minute_mv1(this->AsFFI());
    return result;
}

inline uint8_t icu4x::Time::second() const {
    auto result = icu4x::capi::icu4x_Time_second_mv1(this->AsFFI());
    return result;
}

inline uint32_t icu4x::Time::subsecond() const {
    auto result = icu4x::capi::icu4x_Time_subsecond_mv1(this->AsFFI());
    return result;
}

inline const icu4x::capi::Time* icu4x::Time::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Time*>(this);
}

inline icu4x::capi::Time* icu4x::Time::AsFFI() {
    return reinterpret_cast<icu4x::capi::Time*>(this);
}

inline const icu4x::Time* icu4x::Time::FromFFI(const icu4x::capi::Time* ptr) {
    return reinterpret_cast<const icu4x::Time*>(ptr);
}

inline icu4x::Time* icu4x::Time::FromFFI(icu4x::capi::Time* ptr) {
    return reinterpret_cast<icu4x::Time*>(ptr);
}

inline void icu4x::Time::operator delete(void* ptr) {
    icu4x::capi::icu4x_Time_destroy_mv1(reinterpret_cast<icu4x::capi::Time*>(ptr));
}


#endif // ICU4X_Time_HPP
