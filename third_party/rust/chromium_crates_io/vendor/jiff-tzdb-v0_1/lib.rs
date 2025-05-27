/*!
A crate that embeds data from the [IANA Time Zone Database].

This crate is meant to be a "raw data" library. That is, it primarily exposes
one routine that permits looking up the raw [TZif] data given a time zone name.
The data returned is embedded into the compiled library. In order to actually
use the data, you'll need a TZif parser, such as the one found in [Jiff] via
`TimeZone::tzif`.

This crate also exposes another routine, [`available`], for iterating over the
names of all time zones embedded into this crate.

# Should I use this crate?

In general, no. It's first and foremost an implementation detail of Jiff, but
if you 1) need raw access to the TZif data and 2) need to bundle it in your
binary, then it's plausible that using this crate is appropriate.

With that said, the _preferred_ way to read TZif data is from your system's
copy of the Time Zone Database. On macOS and most Linux installations, a copy
of this data can be found at `/usr/share/zoneinfo`. Indeed, Jiff will use this
system copy whenever possible, and not use this crate at all. The system copy
is preferred because the Time Zone Database is occasionally updated (perhaps a
few times per year), and it is usually better to rely on your system updates
for such things than some random Rust library.

However, some popular environments, like Windows, do not have a standard
system copy of the Time Zone Database. In those circumstances, Jiff will depend
on this crate and bundle the time zone data into the binary. This is not an
ideal solution, but it makes Most Things Just Work Most of the Time on all
major platforms.

# Data generation

The data in this crate comes from the [IANA Time Zone Database] "data only"
distribution. [`jiff-cli`] is used to first compile the release into binary
TZif data using the `zic` compiler, and secondly, converts the binary data into
a flattened and de-duplicated representation that is embedded into this crate's
source code.

The conversion into the TZif binary data uses the following settings:

* The "rearguard" data is used (see below).
* The binary data itself is compiled using the "slim" format. Which
  effectively means that the TZif data primarily only uses explicit
  time zone transitions for historical data and POSIX time zones for
  current time zone transition rules. This doesn't have any impact
  on the actual results. The reason that there are "slim" and "fat"
  formats is to support legacy applications that can't deal with
  POSIX time zones. For example, `/usr/share/zoneinfo` on my modern
  Archlinux installation (2025-02-27) is in the "fat" format.

The reason that rearguard data is used is a bit more subtle and has to do with
a difference in how the IANA Time Zone Database treats its internal "daylight
saving time" flag and what people in the "real world" consider "daylight
saving time." For example, in the standard distribution of the IANA Time Zone
Database, `Europe/Dublin` has its daylight saving time flag set to _true_
during Winter and set to _false_ during Summer. The actual time shifts are the
same as, e.g., `Europe/London`, but which one is actually labeled "daylight
saving time" is not.

The IANA Time Zone Database does this for `Europe/Dublin`, presumably, because
_legally_, time during the Summer in Ireland is called `Irish Standard Time`,
and time during the Winter is called `Greenwich Mean Time`. These legal names
are reversed from what is typically the case, where "standard" time is during
the Winter and daylight saving time is during the Summer. The IANA Time Zone
Database implements this tweak in legal language via a "negative daylight
saving time offset." This is somewhat odd, and some consumers of the IANA Time
Zone Database cannot handle it. Thus, the rearguard format was born for,
seemingly, legacy programs.

Jiff can handle negative daylight saving time offsets just fine, but we use the
rearguard format anyway so that the underlying data more accurately reflects
on-the-ground reality for humans living in `Europe/Dublin`. In particular,
using the rearguard data enables [localization of time zone names] to be done
correctly.

[IANA Time Zone Database]: https://www.iana.org/time-zones
[TZif]: https://datatracker.ietf.org/doc/html/rfc8536
[Jiff]: https://docs.rs/jiff
[`jiff-cli`]: https://github.com/BurntSushi/jiff/tree/master/crates/jiff-cli
[localization of time zone names]: https://github.com/BurntSushi/jiff/issues/258
*/

#![no_std]

mod tzname;

static TZIF_DATA: &[u8] = include_bytes!("concatenated-zoneinfo.dat");

/// The version of the IANA Time Zone Database that was bundled.
///
/// If this bundled database was generated from a pre-existing system copy
/// of the Time Zone Database, then it's possible no version information was
/// available.
pub static VERSION: Option<&str> = tzname::VERSION;

/// Returns the binary TZif data for the time zone name given.
///
/// This also returns the canonical name for the time zone. Namely, since this
/// lookup is performed without regard to ASCII case, the given name may not be
/// the canonical capitalization of the time zone.
///
/// If no matching time zone data exists, then `None` is returned.
///
/// In order to use the data returned, it must be fed to a TZif parser. For
/// example, if you're using [`jiff`](https://docs.rs/jiff), then this would
/// be the `TimeZone::tzif` constructor.
///
/// # Example
///
/// Some basic examples of time zones that exist:
///
/// ```
/// assert!(jiff_tzdb::get("America/New_York").is_some());
/// assert!(jiff_tzdb::get("america/new_york").is_some());
/// assert!(jiff_tzdb::get("America/NewYork").is_none());
/// ```
///
/// And an example of how the canonical name might differ from the name given:
///
/// ```
/// let (canonical_name, data) = jiff_tzdb::get("america/new_york").unwrap();
/// assert_eq!(canonical_name, "America/New_York");
/// // All TZif data starts with the `TZif` header.
/// assert_eq!(&data[..4], b"TZif");
/// ```
pub fn get(name: &str) -> Option<(&'static str, &'static [u8])> {
    let index = index(name)?;
    let (canonical_name, ref range) = tzname::TZNAME_TO_OFFSET[index];
    Some((canonical_name, &TZIF_DATA[range.clone()]))
}

/// Returns a list of all available time zone names bundled into this crate.
///
/// There are no API guarantees on the order of the sequence returned.
///
/// # Example
///
/// This example shows how to determine the total number of time zone names
/// available:
///
/// ```
/// assert_eq!(jiff_tzdb::available().count(), 598);
/// ```
///
/// Note that this number may change in subsequent releases of the Time Zone
/// Database.
pub fn available() -> TimeZoneNameIter {
    TimeZoneNameIter { it: tzname::TZNAME_TO_OFFSET.iter() }
}

/// An iterator over all time zone names embedded into this crate.
///
/// There are no API guarantees on the order of this iterator.
///
/// This iterator is created by the [`available`] function.
#[derive(Clone, Debug)]
pub struct TimeZoneNameIter {
    it: core::slice::Iter<'static, (&'static str, core::ops::Range<usize>)>,
}

impl Iterator for TimeZoneNameIter {
    type Item = &'static str;

    fn next(&mut self) -> Option<&'static str> {
        self.it.next().map(|&(name, _)| name)
    }
}

/// Finds the index of a matching entry in `TZNAME_TO_OFFSET`.
///
/// If the given time zone doesn't exist, then `None` is returned.
fn index(query_name: &str) -> Option<usize> {
    tzname::TZNAME_TO_OFFSET
        .binary_search_by(|(name, _)| cmp_ignore_ascii_case(name, query_name))
        .ok()
}

/// Like std's `eq_ignore_ascii_case`, but returns a full `Ordering`.
fn cmp_ignore_ascii_case(s1: &str, s2: &str) -> core::cmp::Ordering {
    let it1 = s1.as_bytes().iter().map(|&b| b.to_ascii_lowercase());
    let it2 = s2.as_bytes().iter().map(|&b| b.to_ascii_lowercase());
    it1.cmp(it2)
}

#[cfg(test)]
mod tests {
    use core::cmp::Ordering;

    use crate::tzname::TZNAME_TO_OFFSET;

    use super::*;

    /// This is a regression test where TZ names were sorted lexicographically
    /// but case sensitively, and this could subtly break binary search.
    #[test]
    fn sorted_ascii_case_insensitive() {
        for window in TZNAME_TO_OFFSET.windows(2) {
            let (name1, _) = window[0];
            let (name2, _) = window[1];
            assert_eq!(
                Ordering::Less,
                cmp_ignore_ascii_case(name1, name2),
                "{name1} should be less than {name2}",
            );
        }
    }
}
