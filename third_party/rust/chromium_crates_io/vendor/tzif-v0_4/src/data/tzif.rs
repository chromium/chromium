// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::time::Seconds;
use crate::data::posix::PosixTzString;

/// A `TZif` file header.
/// See <https://datatracker.ietf.org/doc/html/rfc8536> for more information.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TzifHeader {
    /// An byte identifying the version of the file's format.
    /// The value MUST be one of the following:
    ///
    /// NUL (0x00)  Version 1 - The file contains only the version 1
    /// header and data block.  Version 1 files MUST NOT contain a
    /// version-2+ header, data block, or footer.
    ///
    /// '2' (0x32)  Version 2 - The file MUST contain the version 1 header
    /// and data block, a version-2+ header and data block, and a
    /// footer.  The TZ string in the footer (Section 3.3), if
    /// nonempty, MUST strictly adhere to the requirements for the TZ
    /// environment variable as defined in Section 8.3 of the "Base
    /// Definitions" volume of \[POSIX\] and MUST encode the POSIX
    /// portable character set as ASCII.
    ///
    /// '3' (0x33)  Version 3 - The file MUST contain the version 1 header
    /// and data block, a version-2+ header and data block, and a
    /// footer.  The TZ string in the footer (Section 3.3), if
    /// nonempty, MUST conform to POSIX requirements with ASCII
    /// encoding, except that it MAY use the TZ string extensions
    /// described below (Section 3.3.1).
    pub version: usize,

    /// A four-byte unsigned integer specifying the number of UT/
    /// local indicators contained in the data block -- MUST either be
    /// zero or equal to "typecnt".
    pub isutcnt: usize,

    /// A four-byte unsigned integer specifying the number of
    /// standard/wall indicators contained in the data block -- MUST
    /// either be zero or equal to "typecnt".
    pub isstdcnt: usize,

    /// A four-byte unsigned integer specifying the number of
    /// leap-second records contained in the data block.
    pub leapcnt: usize,

    /// A four-byte unsigned integer specifying the number of
    /// transition times contained in the data block.
    pub timecnt: usize,

    /// A four-byte unsigned integer specifying the number of
    /// local time type records contained in the data block -- MUST NOT be
    /// zero.  (Although local time type records convey no useful
    /// information in files that have nonempty TZ strings but no
    /// transitions, at least one such record is nevertheless required
    /// because many `TZif` readers reject files that have zero time types.)
    pub typecnt: usize,

    /// A four-byte unsigned integer specifying the total number
    /// of bytes used by the set of time zone designations contained in
    /// the data block - MUST NOT be zero.  The count includes the
    /// trailing NUL (0x00) byte at the end of the last time zone
    /// designation.
    pub charcnt: usize,
}

impl TzifHeader {
    /// Returns the version number of the `TZif` header.
    pub fn version(&self) -> usize {
        self.version
    }

    /// Returns the number of bytes per time object based on the version number.
    pub fn time_size<const V: usize>() -> usize {
        match V {
            1 => 4,
            _ => 8,
        }
    }

    /// Returns the exact size of the data block in bytes based on the header.
    pub fn block_size<const V: usize>(&self) -> usize {
        let time_size = Self::time_size::<V>();
        self.timecnt * time_size
            + self.timecnt
            + self.typecnt * 6
            + self.charcnt
            + self.leapcnt * (time_size + 4)
            + self.isstdcnt
            + self.isutcnt
    }
}

/// A struct containing the data of a `TZif` file.
/// > A `TZif` file is structured as follows:
/// > ```text
/// >      Version 1       Versions 2 & 3
/// >   +-------------+   +-------------+
/// >   |  Version 1  |   |  Version 1  |
/// >   |   Header    |   |   Header    |
/// >   +-------------+   +-------------+
/// >   |  Version 1  |   |  Version 1  |
/// >   |  Data Block |   |  Data Block |
/// >   +-------------+   +-------------+
/// >                     |  Version 2+ |
/// >                     |   Header    |
/// >                     +-------------+
/// >                     |  Version 2+ |
/// >                     |  Data Block |
/// >                     +-------------+
/// >                     |   Footer    |
/// >                     +-------------+
/// > ```
#[derive(Debug)]
pub struct TzifData {
    /// The version-1 header, which is always present.
    pub header1: TzifHeader,
    /// The version-1 data block, which is always present.
    pub data_block1: DataBlock,
    /// The version-2+ header, which is present only in version 2 and 3 `TZif` files.
    pub header2: Option<TzifHeader>,
    /// The vesrion-2+ data block, which is present only in version 2 and 3 `TZif` files.
    pub data_block2: Option<DataBlock>,
    /// The version-2+ footer, which is present only in version 2 and 3 `TZif` files.
    pub footer: Option<PosixTzString>,
}

impl TzifData {
    /// Returns the version number of this `TZif` data.
    pub fn version_number(&self) -> usize {
        self.header2
            .as_ref()
            .map_or(self.header1.version(), TzifHeader::version)
    }

    /// Returns the number of bytes per time object based on the version number.
    pub fn time_size(&self) -> usize {
        match self.version_number() {
            1 => 4,
            _ => 8,
        }
    }

    /// Returns the exact size of the data block in bytes based on the header.
    pub fn block_size<const V: usize>(&self) -> Option<usize> {
        match V {
            1 => Some(self.header1.block_size::<V>()),
            _ => self.header2.as_ref().map(TzifHeader::block_size::<V>),
        }
    }
}

/// A record specifying a local time type.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct LocalTimeTypeRecord {
    /// A signed integer specifying the number of
    /// seconds to be added to UT in order to determine local time.
    /// The value MUST NOT be -2**31 and SHOULD be in the range
    /// [-89999, 93599] (i.e., its value SHOULD be more than -25 hours
    /// and less than 26 hours).  Avoiding -2**31 allows 32-bit clients
    /// to negate the value without overflow.  Restricting it to
    /// [-89999, 93599] allows easy support by implementations that
    /// already support the POSIX-required range [-24:59:59, 25:59:59].
    pub utoff: Seconds,

    /// A value indicating whether local time should
    /// be considered Daylight Saving Time (DST).  The value MUST be 0
    /// A value of [`true`] indicates that this type of time is DST.
    /// A value of [`false`] indicates that this time type is standard time.
    pub is_dst: bool,

    /// An unsigned integer specifying a zero-based
    /// index into the series of time zone designation bytes, thereby
    /// selecting a particular designation string.  Each index MUST be
    /// in the range [0, "charcnt" - 1]; it designates the
    /// NUL-terminated string of bytes starting at position "idx" in
    /// the time zone designations.  (This string MAY be empty.)  A NUL
    /// byte MUST exist in the time zone designations at or after
    /// position "idx".
    pub idx: usize,
}

/// A record specifying the corrections that need to be applied to the UTC in
/// in order to determine TAI.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct LeapSecondRecord {
    /// A UNIX leap time value
    /// specifying the time at which a leap-second correction occurs.
    /// The first value, if present, MUST be nonnegative, and each
    /// later value MUST be at least 2419199 greater than the previous
    /// value.  (This is 28 days' worth of seconds, minus a potential
    /// negative leap second.)
    pub occurrence: Seconds,

    /// A signed integer specifying the value of
    /// LEAPCORR on or after the occurrence.  The correction value in
    /// the first leap-second record, if present, MUST be either one
    /// (1) or minus one (-1).  The correction values in adjacent leap-
    /// second records MUST differ by exactly one (1).  The value of
    /// LEAPCORR is zero for timestamps that occur before the
    /// occurrence time in the first leap-second record (or for all
    /// timestamps if there are no leap-second records).
    pub correction: i32,
}

/// Indicates whether the transition times associated with local time types were
/// specified as standard time or wall-clock time.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StandardWallIndicator {
    /// Standard time
    Standard,
    /// Wall-clock time
    Wall,
}

/// Indicates whether the transition times associated with local time types were
/// specified as UT or local time.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UtLocalIndicator {
    /// UT time
    Ut,
    /// Local time
    Local,
}

/// A `TZif` data block.
///
/// A `TZif` data block consists of seven variable-length elements, each of
/// which is a series of items.  The number of items in each series is
/// determined by the corresponding count field in the header.  The total
/// length of each element is calculated by multiplying the number of
/// items by the size of each item.  Therefore, implementations that do
/// not wish to parse or use the version 1 data block can calculate its
/// total length and skip directly to the header of the version-2+ data
/// block.
///
/// In the version 1 data block, time values are 32 bits (`TIME_SIZE` = 4
/// bytes).  In the version-2+ data block, present only in version 2 and
/// 3 files, time values are 64 bits (`TIME_SIZE` = 8 bytes).
///
/// For consistency, this struct stores all time values in 64-bit integers
/// even for version 1 data blocks.
///
/// The data block is structured as follows (the lengths of multi-byte
/// fields are shown in parentheses):
///
/// > ```text
/// >      +---------------------------------------------------------+
/// >      |  transition times          (timecnt x TIME_SIZE)        |
/// >      +---------------------------------------------------------+
/// >      |  transition types          (timecnt)                    |
/// >      +---------------------------------------------------------+
/// >      |  local time type records   (typecnt x 6)                |
/// >      +---------------------------------------------------------+
/// >      |  time zone designations    (charcnt)                    |
/// >      +---------------------------------------------------------+
/// >      |  leap-second records       (leapcnt x (TIME_SIZE + 4))  |
/// >      +---------------------------------------------------------+
/// >      |  standard/wall indicators  (isstdcnt)                   |
/// >      +---------------------------------------------------------+
/// >      |  UT/local indicators       (isutcnt)                    |
/// >      +---------------------------------------------------------+
/// > ```
#[derive(Debug, Clone, Default)]
pub struct DataBlock {
    /// A series of four- or eight-byte UNIX leap-time
    /// values sorted in strictly ascending order.  Each value is used as
    /// a transition time at which the rules for computing local time may
    /// change.  The number of time values is specified by the "timecnt"
    /// field in the header.  Each time value SHOULD be at least -2**59.
    ///
    /// (-2**59 is the greatest negated power of 2 that predates the Big
    /// Bang, and avoiding earlier timestamps works around known TZif
    /// reader bugs relating to outlandishly negative timestamps.)
    pub transition_times: Vec<Seconds>,

    /// A series of one-byte unsigned integers specifying
    /// the type of local time of the corresponding transition time.
    /// These values serve as zero-based indices into the array of local
    /// time type records.  The number of type indices is specified by the
    /// "timecnt" field in the header.  Each type index MUST be in the
    /// range [0, "typecnt" - 1].
    pub transition_types: Vec<usize>,

    /// A series of [`LocalTimeTypeRecord`] objects.
    pub local_time_type_records: Vec<LocalTimeTypeRecord>,

    /// The string representations for a time-zone desigation, such as "PST" or "PDT".
    pub time_zone_designations: Vec<String>,

    /// A series of [`LeapSecondRecord`] objects.
    pub leap_second_records: Vec<LeapSecondRecord>,

    /// A series of [`StandardWallIndicator`] objects.
    pub standard_wall_indicators: Vec<StandardWallIndicator>,

    /// A series of [`UtLocalIndicator`] objects.
    pub ut_local_indicators: Vec<UtLocalIndicator>,
}

impl DataBlock {
    /// Retrieves the timezone designation at index `idx`.
    pub fn time_zone_designation(&self, mut idx: usize) -> Option<&str> {
        self.time_zone_designations.iter().find_map(|d| {
            if idx <= d.len() {
                Some(&d[idx..])
            } else {
                idx -= d.len() + 1;
                None
            }
        })
    }
}
