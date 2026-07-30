// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
pub mod ffi {
    #[diplomat::enum_convert(icu_datetime::options::Length, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::options::Length, Enum)]
    #[non_exhaustive]
    pub enum DateTimeLength {
        Long,
        #[diplomat::attr(auto, default)]
        Medium,
        Short,
    }

    #[diplomat::enum_convert(icu_datetime::options::Alignment, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::options::Alignment, Enum)]
    #[non_exhaustive]
    pub enum DateTimeAlignment {
        #[diplomat::attr(auto, default)]
        Auto,
        Column,
    }

    #[diplomat::enum_convert(icu_datetime::options::YearStyle, needs_wildcard)]
    #[diplomat::rust_link(icu::datetime::options::YearStyle, Enum)]
    #[non_exhaustive]
    pub enum YearStyle {
        #[diplomat::attr(auto, default)]
        Auto,
        Full,
        WithEra,
        NoEra,
    }

    #[diplomat::rust_link(icu::datetime::options::TimePrecision, Enum)]
    #[diplomat::rust_link(icu::datetime::options::SubsecondDigits, Enum)]
    #[non_exhaustive]
    pub enum TimePrecision {
        Hour,
        Minute,
        MinuteOptional,
        #[diplomat::attr(auto, default)]
        Second,
        Subsecond1,
        Subsecond2,
        Subsecond3,
        Subsecond4,
        Subsecond5,
        Subsecond6,
        Subsecond7,
        Subsecond8,
        Subsecond9,
    }

    impl TimePrecision {
        #[diplomat::rust_link(icu::datetime::options::SubsecondDigits::try_from_int, FnInEnum)]
        pub fn from_subsecond_digits(digits: u8) -> Option<Self> {
            icu_datetime::options::SubsecondDigits::try_from_int(digits)
                .map(icu_datetime::options::TimePrecision::Subsecond)
                .map(Into::into)
        }
    }
}

impl From<ffi::TimePrecision> for icu_datetime::options::TimePrecision {
    fn from(time_precision: ffi::TimePrecision) -> Self {
        use icu_datetime::options::SubsecondDigits;
        use icu_datetime::options::TimePrecision;
        match time_precision {
            ffi::TimePrecision::Hour => TimePrecision::Hour,
            ffi::TimePrecision::Minute => TimePrecision::Minute,
            ffi::TimePrecision::MinuteOptional => TimePrecision::MinuteOptional,
            ffi::TimePrecision::Second => TimePrecision::Second,
            ffi::TimePrecision::Subsecond1 => TimePrecision::Subsecond(SubsecondDigits::S1),
            ffi::TimePrecision::Subsecond2 => TimePrecision::Subsecond(SubsecondDigits::S2),
            ffi::TimePrecision::Subsecond3 => TimePrecision::Subsecond(SubsecondDigits::S3),
            ffi::TimePrecision::Subsecond4 => TimePrecision::Subsecond(SubsecondDigits::S4),
            ffi::TimePrecision::Subsecond5 => TimePrecision::Subsecond(SubsecondDigits::S5),
            ffi::TimePrecision::Subsecond6 => TimePrecision::Subsecond(SubsecondDigits::S6),
            ffi::TimePrecision::Subsecond7 => TimePrecision::Subsecond(SubsecondDigits::S7),
            ffi::TimePrecision::Subsecond8 => TimePrecision::Subsecond(SubsecondDigits::S8),
            ffi::TimePrecision::Subsecond9 => TimePrecision::Subsecond(SubsecondDigits::S9),
        }
    }
}

impl From<icu_datetime::options::TimePrecision> for ffi::TimePrecision {
    fn from(time_precision: icu_datetime::options::TimePrecision) -> Self {
        use icu_datetime::options::SubsecondDigits;
        use icu_datetime::options::TimePrecision;
        match time_precision {
            TimePrecision::Hour => ffi::TimePrecision::Hour,
            TimePrecision::Minute => ffi::TimePrecision::Minute,
            TimePrecision::MinuteOptional => ffi::TimePrecision::MinuteOptional,
            TimePrecision::Second => ffi::TimePrecision::Second,
            TimePrecision::Subsecond(SubsecondDigits::S1) => ffi::TimePrecision::Subsecond1,
            TimePrecision::Subsecond(SubsecondDigits::S2) => ffi::TimePrecision::Subsecond2,
            TimePrecision::Subsecond(SubsecondDigits::S3) => ffi::TimePrecision::Subsecond3,
            TimePrecision::Subsecond(SubsecondDigits::S4) => ffi::TimePrecision::Subsecond4,
            TimePrecision::Subsecond(SubsecondDigits::S5) => ffi::TimePrecision::Subsecond5,
            TimePrecision::Subsecond(SubsecondDigits::S6) => ffi::TimePrecision::Subsecond6,
            TimePrecision::Subsecond(SubsecondDigits::S7) => ffi::TimePrecision::Subsecond7,
            TimePrecision::Subsecond(SubsecondDigits::S8) => ffi::TimePrecision::Subsecond8,
            TimePrecision::Subsecond(SubsecondDigits::S9) => ffi::TimePrecision::Subsecond9,
            _ => {
                debug_assert!(false, "cross-crate exhaustive match");
                ffi::TimePrecision::Second
            }
        }
    }
}
