#ifndef ICU4X_CalendarKind_D_HPP
#define ICU4X_CalendarKind_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
namespace capi { struct Locale; }
class Locale;
class CalendarKind;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum CalendarKind {
      CalendarKind_Iso = 0,
      CalendarKind_Gregorian = 1,
      CalendarKind_Buddhist = 2,
      CalendarKind_Japanese = 3,
      CalendarKind_JapaneseExtended = 4,
      CalendarKind_Ethiopian = 5,
      CalendarKind_EthiopianAmeteAlem = 6,
      CalendarKind_Indian = 7,
      CalendarKind_Coptic = 8,
      CalendarKind_Dangi = 9,
      CalendarKind_Chinese = 10,
      CalendarKind_Hebrew = 11,
      CalendarKind_HijriTabularTypeIIFriday = 12,
      CalendarKind_HijriSimulatedMecca = 18,
      CalendarKind_HijriTabularTypeIIThursday = 14,
      CalendarKind_HijriUmmAlQura = 15,
      CalendarKind_Persian = 16,
      CalendarKind_Roc = 17,
    };

    typedef struct CalendarKind_option {union { CalendarKind ok; }; bool is_ok; } CalendarKind_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * The various calendar types currently supported by {@link Calendar}
 *
 * See the [Rust documentation for `AnyCalendarKind`](https://docs.rs/icu/2.2.0/icu/calendar/enum.AnyCalendarKind.html) for more information.
 */
class CalendarKind {
public:
    enum Value {
        /**
         * The kind of an Iso calendar
         */
        Iso = 0,
        /**
         * The kind of a Gregorian calendar
         */
        Gregorian = 1,
        /**
         * The kind of a Buddhist calendar
         */
        Buddhist = 2,
        /**
         * The kind of a Japanese calendar
         */
        Japanese = 3,
        /**
         * Deprecated, use `Japanese`
         *
         * \deprecated use `Japanese`
         */
        JapaneseExtended [[deprecated("use `Japanese`")]] = 4,
        /**
         * The kind of an Ethiopian calendar, with Amete Mihret era
         */
        Ethiopian = 5,
        /**
         * The kind of an Ethiopian calendar, with Amete Alem era
         */
        EthiopianAmeteAlem = 6,
        /**
         * The kind of a Indian calendar
         */
        Indian = 7,
        /**
         * The kind of a Coptic calendar
         */
        Coptic = 8,
        /**
         * The kind of a Dangi calendar
         */
        Dangi = 9,
        /**
         * The kind of a Chinese calendar
         */
        Chinese = 10,
        /**
         * The kind of a Hebrew calendar
         */
        Hebrew = 11,
        /**
         * The kind of a Hijri tabular, type II leap years, Friday epoch, calendar
         */
        HijriTabularTypeIIFriday = 12,
        /**
         * The kind of a Hijri simulated, Mecca calendar
         */
        HijriSimulatedMecca = 18,
        /**
         * The kind of a Hijri tabular, type II leap years, Thursday epoch, calendar
         */
        HijriTabularTypeIIThursday = 14,
        /**
         * The kind of a Hijri Umm al-Qura calendar
         */
        HijriUmmAlQura = 15,
        /**
         * The kind of a Persian calendar
         */
        Persian = 16,
        /**
         * The kind of a Roc calendar
         */
        Roc = 17,
    };

    CalendarKind(): value(Value::Iso) {}

    // Implicit conversions between enum and ::Value
    constexpr CalendarKind(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * Creates a new {@link CalendarKind} for the specified locale, using compiled data.
   *
   * See the [Rust documentation for `new`](https://docs.rs/icu/2.2.0/icu/calendar/enum.AnyCalendarKind.html#method.new) for more information.
   */
  inline static icu4x::CalendarKind create(const icu4x::Locale& locale);

    inline icu4x::capi::CalendarKind AsFFI() const;
    inline static icu4x::CalendarKind FromFFI(icu4x::capi::CalendarKind c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_CalendarKind_D_HPP
