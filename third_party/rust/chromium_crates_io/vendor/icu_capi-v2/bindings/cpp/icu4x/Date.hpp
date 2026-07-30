#ifndef ICU4X_Date_HPP
#define ICU4X_Date_HPP

#include "Date.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Calendar.hpp"
#include "CalendarDateAddError.hpp"
#include "CalendarDateFromFieldsError.hpp"
#include "CalendarError.hpp"
#include "CalendarMismatchedCalendarError.hpp"
#include "DateAddOptions.hpp"
#include "DateDifferenceOptions.hpp"
#include "DateDuration.hpp"
#include "DateFields.hpp"
#include "DateFromFieldsOptions.hpp"
#include "IsoDate.hpp"
#include "Rfc9557ParseError.hpp"
#include "Weekday.hpp"
#include "diplomat_runtime.hpp"


namespace icu4x {
namespace capi {
    extern "C" {

    typedef struct icu4x_Date_from_iso_in_calendar_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Date_from_iso_in_calendar_mv1_result;
    icu4x_Date_from_iso_in_calendar_mv1_result icu4x_Date_from_iso_in_calendar_mv1(int32_t iso_year, uint8_t iso_month, uint8_t iso_day, const icu4x::capi::Calendar* calendar);

    typedef struct icu4x_Date_from_fields_in_calendar_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarDateFromFieldsError err;}; bool is_ok;} icu4x_Date_from_fields_in_calendar_mv1_result;
    icu4x_Date_from_fields_in_calendar_mv1_result icu4x_Date_from_fields_in_calendar_mv1(icu4x::capi::DateFields fields, icu4x::capi::DateFromFieldsOptions options, const icu4x::capi::Calendar* calendar);

    typedef struct icu4x_Date_from_codes_in_calendar_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Date_from_codes_in_calendar_mv1_result;
    icu4x_Date_from_codes_in_calendar_mv1_result icu4x_Date_from_codes_in_calendar_mv1(icu4x::diplomat::capi::DiplomatStringView era_code, int32_t year, icu4x::diplomat::capi::DiplomatStringView month_code, uint8_t day, const icu4x::capi::Calendar* calendar);

    typedef struct icu4x_Date_from_rata_die_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarError err;}; bool is_ok;} icu4x_Date_from_rata_die_mv1_result;
    icu4x_Date_from_rata_die_mv1_result icu4x_Date_from_rata_die_mv1(int64_t rd, const icu4x::capi::Calendar* calendar);

    typedef struct icu4x_Date_from_string_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::Rfc9557ParseError err;}; bool is_ok;} icu4x_Date_from_string_mv1_result;
    icu4x_Date_from_string_mv1_result icu4x_Date_from_string_mv1(icu4x::diplomat::capi::DiplomatStringView v, const icu4x::capi::Calendar* calendar);

    icu4x::capi::Date* icu4x_Date_to_calendar_mv1(const icu4x::capi::Date* self, const icu4x::capi::Calendar* calendar);

    icu4x::capi::IsoDate* icu4x_Date_to_iso_mv1(const icu4x::capi::Date* self);

    int64_t icu4x_Date_to_rata_die_mv1(const icu4x::capi::Date* self);

    uint16_t icu4x_Date_day_of_year_mv1(const icu4x::capi::Date* self);

    uint8_t icu4x_Date_day_of_month_mv1(const icu4x::capi::Date* self);

    icu4x::capi::Weekday icu4x_Date_day_of_week_mv1(const icu4x::capi::Date* self);

    icu4x::capi::Weekday icu4x_Date_weekday_mv1(const icu4x::capi::Date* self);

    uint8_t icu4x_Date_ordinal_month_mv1(const icu4x::capi::Date* self);

    void icu4x_Date_month_code_mv1(const icu4x::capi::Date* self, icu4x::diplomat::capi::DiplomatWrite* write);

    uint8_t icu4x_Date_month_number_mv1(const icu4x::capi::Date* self);

    bool icu4x_Date_month_is_leap_mv1(const icu4x::capi::Date* self);

    int32_t icu4x_Date_era_year_or_related_iso_mv1(const icu4x::capi::Date* self);

    int32_t icu4x_Date_extended_year_mv1(const icu4x::capi::Date* self);

    void icu4x_Date_era_mv1(const icu4x::capi::Date* self, icu4x::diplomat::capi::DiplomatWrite* write);

    uint8_t icu4x_Date_months_in_year_mv1(const icu4x::capi::Date* self);

    uint8_t icu4x_Date_days_in_month_mv1(const icu4x::capi::Date* self);

    uint16_t icu4x_Date_days_in_year_mv1(const icu4x::capi::Date* self);

    bool icu4x_Date_is_in_leap_year_mv1(const icu4x::capi::Date* self);

    icu4x::capi::Calendar* icu4x_Date_calendar_mv1(const icu4x::capi::Date* self);

    typedef struct icu4x_Date_try_add_with_options_mv1_result {union {icu4x::capi::Date* ok; icu4x::capi::CalendarDateAddError err;}; bool is_ok;} icu4x_Date_try_add_with_options_mv1_result;
    icu4x_Date_try_add_with_options_mv1_result icu4x_Date_try_add_with_options_mv1(const icu4x::capi::Date* self, icu4x::capi::DateDuration duration, icu4x::capi::DateAddOptions options);

    typedef struct icu4x_Date_try_until_with_options_mv1_result {union {icu4x::capi::DateDuration ok; }; bool is_ok;} icu4x_Date_try_until_with_options_mv1_result;
    icu4x_Date_try_until_with_options_mv1_result icu4x_Date_try_until_with_options_mv1(const icu4x::capi::Date* self, const icu4x::capi::Date* other, icu4x::capi::DateDifferenceOptions options);

    void icu4x_Date_destroy_mv1(Date* self);

    } // extern "C"
} // namespace capi
} // namespace

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> icu4x::Date::from_iso_in_calendar(int32_t iso_year, uint8_t iso_month, uint8_t iso_day, const icu4x::Calendar& calendar) {
    auto result = icu4x::capi::icu4x_Date_from_iso_in_calendar_mv1(iso_year,
        iso_month,
        iso_day,
        calendar.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(icu4x::diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateFromFieldsError> icu4x::Date::from_fields_in_calendar(icu4x::DateFields fields, icu4x::DateFromFieldsOptions options, const icu4x::Calendar& calendar) {
    auto result = icu4x::capi::icu4x_Date_from_fields_in_calendar_mv1(fields.AsFFI(),
        options.AsFFI(),
        calendar.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateFromFieldsError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateFromFieldsError>(icu4x::diplomat::Err<icu4x::CalendarDateFromFieldsError>(icu4x::CalendarDateFromFieldsError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> icu4x::Date::from_codes_in_calendar(std::string_view era_code, int32_t year, std::string_view month_code, uint8_t day, const icu4x::Calendar& calendar) {
    auto result = icu4x::capi::icu4x_Date_from_codes_in_calendar_mv1({era_code.data(), era_code.size()},
        year,
        {month_code.data(), month_code.size()},
        day,
        calendar.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(icu4x::diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError> icu4x::Date::from_rata_die(int64_t rd, const icu4x::Calendar& calendar) {
    auto result = icu4x::capi::icu4x_Date_from_rata_die_mv1(rd,
        calendar.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarError>(icu4x::diplomat::Err<icu4x::CalendarError>(icu4x::CalendarError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::Rfc9557ParseError> icu4x::Date::from_string(std::string_view v, const icu4x::Calendar& calendar) {
    auto result = icu4x::capi::icu4x_Date_from_string_mv1({v.data(), v.size()},
        calendar.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::Rfc9557ParseError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::Rfc9557ParseError>(icu4x::diplomat::Err<icu4x::Rfc9557ParseError>(icu4x::Rfc9557ParseError::FromFFI(result.err)));
}

inline std::unique_ptr<icu4x::Date> icu4x::Date::to_calendar(const icu4x::Calendar& calendar) const {
    auto result = icu4x::capi::icu4x_Date_to_calendar_mv1(this->AsFFI(),
        calendar.AsFFI());
    return std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result));
}

inline std::unique_ptr<icu4x::IsoDate> icu4x::Date::to_iso() const {
    auto result = icu4x::capi::icu4x_Date_to_iso_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::IsoDate>(icu4x::IsoDate::FromFFI(result));
}

inline int64_t icu4x::Date::to_rata_die() const {
    auto result = icu4x::capi::icu4x_Date_to_rata_die_mv1(this->AsFFI());
    return result;
}

inline uint16_t icu4x::Date::day_of_year() const {
    auto result = icu4x::capi::icu4x_Date_day_of_year_mv1(this->AsFFI());
    return result;
}

inline uint8_t icu4x::Date::day_of_month() const {
    auto result = icu4x::capi::icu4x_Date_day_of_month_mv1(this->AsFFI());
    return result;
}

inline icu4x::Weekday icu4x::Date::day_of_week() const {
    auto result = icu4x::capi::icu4x_Date_day_of_week_mv1(this->AsFFI());
    return icu4x::Weekday::FromFFI(result);
}

inline icu4x::Weekday icu4x::Date::weekday() const {
    auto result = icu4x::capi::icu4x_Date_weekday_mv1(this->AsFFI());
    return icu4x::Weekday::FromFFI(result);
}

inline uint8_t icu4x::Date::ordinal_month() const {
    auto result = icu4x::capi::icu4x_Date_ordinal_month_mv1(this->AsFFI());
    return result;
}

inline std::string icu4x::Date::month_code() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Date_month_code_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Date::month_code_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Date_month_code_mv1(this->AsFFI(),
        &write);
}

inline uint8_t icu4x::Date::month_number() const {
    auto result = icu4x::capi::icu4x_Date_month_number_mv1(this->AsFFI());
    return result;
}

inline bool icu4x::Date::month_is_leap() const {
    auto result = icu4x::capi::icu4x_Date_month_is_leap_mv1(this->AsFFI());
    return result;
}

inline int32_t icu4x::Date::era_year_or_related_iso() const {
    auto result = icu4x::capi::icu4x_Date_era_year_or_related_iso_mv1(this->AsFFI());
    return result;
}

inline int32_t icu4x::Date::extended_year() const {
    auto result = icu4x::capi::icu4x_Date_extended_year_mv1(this->AsFFI());
    return result;
}

inline std::string icu4x::Date::era() const {
    std::string output;
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteFromString(output);
    icu4x::capi::icu4x_Date_era_mv1(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void icu4x::Date::era_write(W& writeable) const {
    icu4x::diplomat::capi::DiplomatWrite write = icu4x::diplomat::WriteTrait<W>::Construct(writeable);
    icu4x::capi::icu4x_Date_era_mv1(this->AsFFI(),
        &write);
}

inline uint8_t icu4x::Date::months_in_year() const {
    auto result = icu4x::capi::icu4x_Date_months_in_year_mv1(this->AsFFI());
    return result;
}

inline uint8_t icu4x::Date::days_in_month() const {
    auto result = icu4x::capi::icu4x_Date_days_in_month_mv1(this->AsFFI());
    return result;
}

inline uint16_t icu4x::Date::days_in_year() const {
    auto result = icu4x::capi::icu4x_Date_days_in_year_mv1(this->AsFFI());
    return result;
}

inline bool icu4x::Date::is_in_leap_year() const {
    auto result = icu4x::capi::icu4x_Date_is_in_leap_year_mv1(this->AsFFI());
    return result;
}

inline std::unique_ptr<icu4x::Calendar> icu4x::Date::calendar() const {
    auto result = icu4x::capi::icu4x_Date_calendar_mv1(this->AsFFI());
    return std::unique_ptr<icu4x::Calendar>(icu4x::Calendar::FromFFI(result));
}

inline icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateAddError> icu4x::Date::try_add_with_options(icu4x::DateDuration duration, icu4x::DateAddOptions options) const {
    auto result = icu4x::capi::icu4x_Date_try_add_with_options_mv1(this->AsFFI(),
        duration.AsFFI(),
        options.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateAddError>(icu4x::diplomat::Ok<std::unique_ptr<icu4x::Date>>(std::unique_ptr<icu4x::Date>(icu4x::Date::FromFFI(result.ok)))) : icu4x::diplomat::result<std::unique_ptr<icu4x::Date>, icu4x::CalendarDateAddError>(icu4x::diplomat::Err<icu4x::CalendarDateAddError>(icu4x::CalendarDateAddError::FromFFI(result.err)));
}

inline icu4x::diplomat::result<icu4x::DateDuration, icu4x::CalendarMismatchedCalendarError> icu4x::Date::try_until_with_options(const icu4x::Date& other, icu4x::DateDifferenceOptions options) const {
    auto result = icu4x::capi::icu4x_Date_try_until_with_options_mv1(this->AsFFI(),
        other.AsFFI(),
        options.AsFFI());
    return result.is_ok ? icu4x::diplomat::result<icu4x::DateDuration, icu4x::CalendarMismatchedCalendarError>(icu4x::diplomat::Ok<icu4x::DateDuration>(icu4x::DateDuration::FromFFI(result.ok))) : icu4x::diplomat::result<icu4x::DateDuration, icu4x::CalendarMismatchedCalendarError>(icu4x::diplomat::Err<icu4x::CalendarMismatchedCalendarError>(icu4x::CalendarMismatchedCalendarError {}));
}

inline const icu4x::capi::Date* icu4x::Date::AsFFI() const {
    return reinterpret_cast<const icu4x::capi::Date*>(this);
}

inline icu4x::capi::Date* icu4x::Date::AsFFI() {
    return reinterpret_cast<icu4x::capi::Date*>(this);
}

inline const icu4x::Date* icu4x::Date::FromFFI(const icu4x::capi::Date* ptr) {
    return reinterpret_cast<const icu4x::Date*>(ptr);
}

inline icu4x::Date* icu4x::Date::FromFFI(icu4x::capi::Date* ptr) {
    return reinterpret_cast<icu4x::Date*>(ptr);
}

inline void icu4x::Date::operator delete(void* ptr) {
    icu4x::capi::icu4x_Date_destroy_mv1(reinterpret_cast<icu4x::capi::Date*>(ptr));
}


#endif // ICU4X_Date_HPP
