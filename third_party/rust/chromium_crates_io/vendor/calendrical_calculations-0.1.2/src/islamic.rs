use crate::astronomy::*;
use crate::helpers::{i64_to_saturated_i32, next};
use crate::rata_die::{Moment, RataDie};
#[allow(unused_imports)]
use core_maths::*;

// Different islamic calendars use different epochs (Thursday vs Friday) due to disagreement on the exact date of Mohammed's migration to Mecca.
/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2066>
const FIXED_ISLAMIC_EPOCH_FRIDAY: RataDie = crate::julian::fixed_from_julian(622, 7, 16);
const FIXED_ISLAMIC_EPOCH_THURSDAY: RataDie = crate::julian::fixed_from_julian(622, 7, 15);

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L6898>
const CAIRO: Location = Location {
    latitude: 30.1,
    longitude: 31.3,
    elevation: 200.0,
    zone: (1_f64 / 12_f64),
};

/// Common abstraction over islamic-style calendars
pub trait IslamicBasedMarker {
    /// The epoch of the calendar. Different calendars use a different epoch (Thu or Fri) due to disagreement on the exact date of Mohammed's migration to Mecca.
    const EPOCH: RataDie;
    /// The name of the calendar for debugging.
    const DEBUG_NAME: &'static str;
    /// Whether this calendar is known to have 353-day years.
    /// This is probably a bug; see <https://github.com/unicode-org/icu4x/issues/4930>
    const HAS_353_DAY_YEARS: bool;
    /// Given the extended year, calculate the approximate new year using the mean synodic month
    fn mean_synodic_ny(extended_year: i32) -> RataDie {
        Self::EPOCH + (f64::from((extended_year - 1) * 12) * MEAN_SYNODIC_MONTH).floor() as i64
    }
    /// Given an iso date, calculate the *approximate* islamic year it corresponds to (for quick cache lookup)
    fn approximate_islamic_from_fixed(date: RataDie) -> i32 {
        let diff = date - Self::EPOCH;
        let months = diff as f64 / MEAN_SYNODIC_MONTH;
        let years = months / 12.;
        (years + 1.).floor() as i32
    }
    /// Convert an islamic date in this calendar to a R.D.
    fn fixed_from_islamic(year: i32, month: u8, day: u8) -> RataDie;
    /// Convert an R.D. To an islamic date in this calendar
    fn islamic_from_fixed(date: RataDie) -> (i32, u8, u8);

    /// Given an extended year, calculate whether each month is 29 or 30 days long
    fn month_lengths_for_year(extended_year: i32, ny: RataDie) -> [bool; 12] {
        let next_ny = Self::fixed_from_islamic(extended_year + 1, 1, 1);
        match next_ny - ny {
            355 | 354 => (),
            353 if Self::HAS_353_DAY_YEARS => {
                #[cfg(feature = "logging")]
                log::trace!(
                    "({}) Found year {extended_year} AH with length {}. See <https://github.com/unicode-org/icu4x/issues/4930>",
                    Self::DEBUG_NAME,
                    next_ny - ny
                );
            }
            other => {
                debug_assert!(
                    false,
                    "({}) Found year {extended_year} AH with length {}!",
                    Self::DEBUG_NAME,
                    other
                )
            }
        }
        let mut prev_rd = ny;
        let mut excess_days = 0;
        let mut lengths = core::array::from_fn(|month_idx| {
            let month_idx = month_idx as u8;
            let new_rd = if month_idx < 11 {
                Self::fixed_from_islamic(extended_year, month_idx + 2, 1)
            } else {
                next_ny
            };
            let diff = new_rd - prev_rd;
            prev_rd = new_rd;
            match diff {
                29 => false,
                30 => true,
                31 => {
                    #[cfg(feature = "logging")]
                    log::trace!(
                        "({}) Found year {extended_year} AH with month length {diff} for month {}.",
                        Self::DEBUG_NAME,
                        month_idx + 1
                    );
                    excess_days += 1;
                    true
                }
                _ => {
                    debug_assert!(
                        false,
                        "({}) Found year {extended_year} AH with month length {diff} for month {}!",
                        Self::DEBUG_NAME,
                        month_idx + 1
                    );
                    false
                }
            }
        });
        // To maintain invariants for calendar arithmetic, if astronomy finds
        // a 31-day month, "move" the day to the first 29-day month in the
        // same year to maintain all months at 29 or 30 days.
        if excess_days != 0 {
            debug_assert_eq!(
                excess_days,
                1,
                "({}) Found year {extended_year} AH with more than one excess day!",
                Self::DEBUG_NAME
            );
            if let Some(l) = lengths.iter_mut().find(|l| !(**l)) {
                *l = true;
            }
        }
        lengths
    }
}

/// Marker type for observational islamic calendar, for use with [`IslamicBasedMarker`]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
#[allow(clippy::exhaustive_structs)] // marker
pub struct ObservationalIslamicMarker;

/// Marker type for Saudi islamic calendar, for use with [`IslamicBasedMarker`]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
#[allow(clippy::exhaustive_structs)] // marker
pub struct SaudiIslamicMarker;

/// Marker type for civil islamic calendar, for use with [`IslamicBasedMarker`]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
#[allow(clippy::exhaustive_structs)] // marker
pub struct CivilIslamicMarker;

/// Marker type for observational islamic calendar, for use with [`IslamicBasedMarker`]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
#[allow(clippy::exhaustive_structs)] // marker
pub struct TabularIslamicMarker;

impl IslamicBasedMarker for ObservationalIslamicMarker {
    const EPOCH: RataDie = FIXED_ISLAMIC_EPOCH_FRIDAY;
    const DEBUG_NAME: &'static str = "ObservationalIslamic";
    const HAS_353_DAY_YEARS: bool = true;
    fn fixed_from_islamic(year: i32, month: u8, day: u8) -> RataDie {
        fixed_from_islamic_observational(year, month, day)
    }
    fn islamic_from_fixed(date: RataDie) -> (i32, u8, u8) {
        observational_islamic_from_fixed(date)
    }
}

impl IslamicBasedMarker for SaudiIslamicMarker {
    const EPOCH: RataDie = FIXED_ISLAMIC_EPOCH_FRIDAY;
    const DEBUG_NAME: &'static str = "SaudiIslamic";
    const HAS_353_DAY_YEARS: bool = true;
    fn fixed_from_islamic(year: i32, month: u8, day: u8) -> RataDie {
        fixed_from_saudi_islamic(year, month, day)
    }
    fn islamic_from_fixed(date: RataDie) -> (i32, u8, u8) {
        saudi_islamic_from_fixed(date)
    }
}

impl IslamicBasedMarker for CivilIslamicMarker {
    const EPOCH: RataDie = FIXED_ISLAMIC_EPOCH_FRIDAY;
    const DEBUG_NAME: &'static str = "CivilIslamic";
    const HAS_353_DAY_YEARS: bool = false;
    fn fixed_from_islamic(year: i32, month: u8, day: u8) -> RataDie {
        fixed_from_islamic_civil(year, month, day)
    }
    fn islamic_from_fixed(date: RataDie) -> (i32, u8, u8) {
        islamic_civil_from_fixed(date)
    }
}

impl IslamicBasedMarker for TabularIslamicMarker {
    const EPOCH: RataDie = FIXED_ISLAMIC_EPOCH_THURSDAY;
    const DEBUG_NAME: &'static str = "TabularIslamic";
    const HAS_353_DAY_YEARS: bool = false;
    fn fixed_from_islamic(year: i32, month: u8, day: u8) -> RataDie {
        fixed_from_islamic_tabular(year, month, day)
    }
    fn islamic_from_fixed(date: RataDie) -> (i32, u8, u8) {
        islamic_tabular_from_fixed(date)
    }
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L6904>
pub fn fixed_from_islamic_observational(year: i32, month: u8, day: u8) -> RataDie {
    let year = i64::from(year);
    let month = i64::from(month);
    let day = i64::from(day);
    let midmonth = FIXED_ISLAMIC_EPOCH_FRIDAY.to_f64_date()
        + (((year - 1) as f64) * 12.0 + month as f64 - 0.5) * MEAN_SYNODIC_MONTH;
    let lunar_phase = Astronomical::calculate_new_moon_at_or_before(RataDie::new(midmonth as i64));
    Astronomical::phasis_on_or_before(RataDie::new(midmonth as i64), CAIRO, Some(lunar_phase)) + day
        - 1
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/1ee51ecfaae6f856b0d7de3e36e9042100b4f424/calendar.l#L6983-L6995>
pub fn observational_islamic_from_fixed(date: RataDie) -> (i32, u8, u8) {
    let lunar_phase = Astronomical::calculate_new_moon_at_or_before(date);
    let crescent = Astronomical::phasis_on_or_before(date, CAIRO, Some(lunar_phase));
    let elapsed_months =
        ((crescent - FIXED_ISLAMIC_EPOCH_FRIDAY) as f64 / MEAN_SYNODIC_MONTH).round() as i32;
    let year = elapsed_months.div_euclid(12) + 1;
    let month = elapsed_months.rem_euclid(12) + 1;
    let day = (date - crescent + 1) as u8;

    (year, month as u8, day)
}

// Saudi visibility criterion on eve of fixed date in Mecca.
// The start of the new month only happens if both of these criterias are met: The moon is a waxing crescent at sunset of the previous day
// and the moon sets after the sun on that same evening.
/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L6957>
fn saudi_criterion(date: RataDie) -> Option<bool> {
    let sunset = Astronomical::sunset((date - 1).as_moment(), MECCA)?;
    let tee = Location::universal_from_standard(sunset, MECCA);
    let phase = Astronomical::lunar_phase(tee, Astronomical::julian_centuries(tee));
    let moonlag = Astronomical::moonlag((date - 1).as_moment(), MECCA)?;

    Some(phase > 0.0 && phase < 90.0 && moonlag > 0.0)
}

pub(crate) fn adjusted_saudi_criterion(date: RataDie) -> bool {
    saudi_criterion(date).unwrap_or_default()
}

// Closest fixed date on or before date when Saudi visibility criterion is held.
/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L6966>
pub fn saudi_new_month_on_or_before(date: RataDie) -> RataDie {
    let last_new_moon = (Astronomical::lunar_phase_at_or_before(0.0, date.as_moment()))
        .inner()
        .floor(); // Gets the R.D Date of the prior new moon
    let age = date.to_f64_date() - last_new_moon;
    // Explanation of why the value 3.0 is chosen: https://github.com/unicode-org/icu4x/pull/3673/files#r1267460916
    let tau = if age <= 3.0 && !adjusted_saudi_criterion(date) {
        // Checks if the criterion is not yet visible on the evening of date
        last_new_moon - 30.0 // Goes back a month
    } else {
        last_new_moon
    };

    next(RataDie::new(tau as i64), adjusted_saudi_criterion) // Loop that increments the day and checks if the criterion is now visible
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L6996>
pub fn saudi_islamic_from_fixed(date: RataDie) -> (i32, u8, u8) {
    let crescent = saudi_new_month_on_or_before(date);
    let elapsed_months =
        ((crescent - FIXED_ISLAMIC_EPOCH_FRIDAY) as f64 / MEAN_SYNODIC_MONTH).round() as i64;
    let year = i64_to_saturated_i32(elapsed_months.div_euclid(12) + 1);
    let month = (elapsed_months.rem_euclid(12) + 1) as u8;
    let day = ((date - crescent) + 1) as u8;

    (year, month, day)
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L6981>
pub fn fixed_from_saudi_islamic(year: i32, month: u8, day: u8) -> RataDie {
    let midmonth = RataDie::new(
        FIXED_ISLAMIC_EPOCH_FRIDAY.to_i64_date()
            + (((year as f64 - 1.0) * 12.0 + month as f64 - 0.5) * MEAN_SYNODIC_MONTH).floor()
                as i64,
    );
    let first_day_of_month = saudi_new_month_on_or_before(midmonth).to_i64_date();

    RataDie::new(first_day_of_month + day as i64 - 1)
}

/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2076>
pub fn fixed_from_islamic_civil(year: i32, month: u8, day: u8) -> RataDie {
    let year = i64::from(year);
    let month = i64::from(month);
    let day = i64::from(day);

    RataDie::new(
        (FIXED_ISLAMIC_EPOCH_FRIDAY.to_i64_date() - 1)
            + (year - 1) * 354
            + (3 + year * 11).div_euclid(30)
            + 29 * (month - 1)
            + month.div_euclid(2)
            + day,
    )
}
/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2090>
pub fn islamic_civil_from_fixed(date: RataDie) -> (i32, u8, u8) {
    let year =
        i64_to_saturated_i32(((date - FIXED_ISLAMIC_EPOCH_FRIDAY) * 30 + 10646).div_euclid(10631));
    let prior_days = date.to_f64_date() - fixed_from_islamic_civil(year, 1, 1).to_f64_date();
    debug_assert!(prior_days >= 0.0);
    debug_assert!(prior_days <= 354.);
    let month = (((prior_days * 11.0) + 330.0) / 325.0) as u8; // Prior days is maximum 354 (when year length is 355), making the value always less than 12
    debug_assert!(month <= 12);
    let day =
        (date.to_f64_date() - fixed_from_islamic_civil(year, month, 1).to_f64_date() + 1.0) as u8; // The value will always be number between 1-30 because of the difference between the date and lunar ordinals function.

    (year, month, day)
}

/// Lisp code reference:https: //github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2076
pub fn fixed_from_islamic_tabular(year: i32, month: u8, day: u8) -> RataDie {
    let year = i64::from(year);
    let month = i64::from(month);
    let day = i64::from(day);
    RataDie::new(
        (FIXED_ISLAMIC_EPOCH_THURSDAY.to_i64_date() - 1)
            + (year - 1) * 354
            + (3 + year * 11).div_euclid(30)
            + 29 * (month - 1)
            + month.div_euclid(2)
            + day,
    )
}
/// Lisp code reference: <https://github.com/EdReingold/calendar-code2/blob/main/calendar.l#L2090>
pub fn islamic_tabular_from_fixed(date: RataDie) -> (i32, u8, u8) {
    let year = i64_to_saturated_i32(
        ((date - FIXED_ISLAMIC_EPOCH_THURSDAY) * 30 + 10646).div_euclid(10631),
    );
    let prior_days = date.to_f64_date() - fixed_from_islamic_tabular(year, 1, 1).to_f64_date();
    debug_assert!(prior_days >= 0.0);
    debug_assert!(prior_days <= 354.);
    let month = (((prior_days * 11.0) + 330.0) / 325.0) as u8; // Prior days is maximum 354 (when year length is 355), making the value always less than 12
    debug_assert!(month <= 12);
    let day =
        (date.to_f64_date() - fixed_from_islamic_tabular(year, month, 1).to_f64_date() + 1.0) as u8; // The value will always be number between 1-30 because of the difference between the date and lunar ordinals function.

    (year, month, day)
}

/// The number of days in a month for the observational islamic calendar
pub fn observational_islamic_month_days(year: i32, month: u8) -> u8 {
    let midmonth = FIXED_ISLAMIC_EPOCH_FRIDAY.to_f64_date()
        + (((year - 1) as f64) * 12.0 + month as f64 - 0.5) * MEAN_SYNODIC_MONTH;

    let lunar_phase: f64 =
        Astronomical::calculate_new_moon_at_or_before(RataDie::new(midmonth as i64));
    let f_date =
        Astronomical::phasis_on_or_before(RataDie::new(midmonth as i64), CAIRO, Some(lunar_phase));

    Astronomical::month_length(f_date, CAIRO)
}

/// The number of days in a month for the Saudi (Umm Al-Qura) calendar
pub fn saudi_islamic_month_days(year: i32, month: u8) -> u8 {
    // We cannot use month_days from the book here, that is for the observational calendar
    //
    // Instead we subtract the two new months calculated using the saudi criterion
    let midmonth = Moment::new(
        FIXED_ISLAMIC_EPOCH_FRIDAY.to_f64_date()
            + (((year - 1) as f64) * 12.0 + month as f64 - 0.5) * MEAN_SYNODIC_MONTH,
    );
    let midmonth_next = midmonth + MEAN_SYNODIC_MONTH;

    let month_start = saudi_new_month_on_or_before(midmonth.as_rata_die());
    let next_month_start = saudi_new_month_on_or_before(midmonth_next.as_rata_die());

    let diff = next_month_start - month_start;
    debug_assert!(
        diff <= 30,
        "umm-al-qura months must not be more than 30 days"
    );
    u8::try_from(diff).unwrap_or(30)
}

#[cfg(test)]
mod tests {
    use super::*;

    static TEST_FIXED_DATE: [i64; 33] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 601716, 613424, 626596, 645554,
        664224, 671401, 694799, 704424, 708842, 709409, 709580, 727274, 728714, 744313, 764652,
    ];
    // Removed: 601716 and 727274 fixed dates
    static TEST_FIXED_DATE_UMMALQURA: [i64; 31] = [
        -214193, -61387, 25469, 49217, 171307, 210155, 253427, 369740, 400085, 434355, 452605,
        470160, 473837, 507850, 524156, 544676, 567118, 569477, 613424, 626596, 645554, 664224,
        671401, 694799, 704424, 708842, 709409, 709580, 728714, 744313, 764652,
    ];
    // Values from lisp code
    static SAUDI_CRITERION_EXPECTED: [bool; 33] = [
        false, false, true, false, false, true, false, true, false, false, true, false, false,
        true, true, true, true, false, false, true, true, true, false, false, false, false, false,
        false, true, false, true, false, true,
    ];
    // Values from lisp code, removed two expected months.
    static SAUDI_NEW_MONTH_OR_BEFORE_EXPECTED: [f64; 31] = [
        -214203.0, -61412.0, 25467.0, 49210.0, 171290.0, 210152.0, 253414.0, 369735.0, 400063.0,
        434348.0, 452598.0, 470139.0, 473830.0, 507850.0, 524150.0, 544674.0, 567118.0, 569450.0,
        613421.0, 626592.0, 645551.0, 664214.0, 671391.0, 694779.0, 704405.0, 708835.0, 709396.0,
        709573.0, 728709.0, 744301.0, 764647.0,
    ];
    #[test]
    fn test_islamic_epoch_friday() {
        let epoch = FIXED_ISLAMIC_EPOCH_FRIDAY.to_i64_date();
        // Iso year of Islamic Epoch
        let epoch_year_from_fixed = crate::iso::iso_year_from_fixed(RataDie::new(epoch));
        // 622 is the correct ISO year for the Islamic Epoch
        assert_eq!(epoch_year_from_fixed, 622);
    }

    #[test]
    fn test_islamic_epoch_thursday() {
        let epoch = FIXED_ISLAMIC_EPOCH_THURSDAY.to_i64_date();
        // Iso year of Islamic Epoch
        let epoch_year_from_fixed = crate::iso::iso_year_from_fixed(RataDie::new(epoch));
        // 622 is the correct ISO year for the Islamic Epoch
        assert_eq!(epoch_year_from_fixed, 622);
    }

    #[test]
    fn test_saudi_criterion() {
        for (boolean, f_date) in SAUDI_CRITERION_EXPECTED.iter().zip(TEST_FIXED_DATE.iter()) {
            let bool_result = saudi_criterion(RataDie::new(*f_date)).unwrap();
            assert_eq!(*boolean, bool_result, "{f_date:?}");
        }
    }

    #[test]
    fn test_saudi_new_month_or_before() {
        for (date, f_date) in SAUDI_NEW_MONTH_OR_BEFORE_EXPECTED
            .iter()
            .zip(TEST_FIXED_DATE_UMMALQURA.iter())
        {
            let date_result = saudi_new_month_on_or_before(RataDie::new(*f_date)).to_f64_date();
            assert_eq!(*date, date_result, "{f_date:?}");
        }
    }
}
