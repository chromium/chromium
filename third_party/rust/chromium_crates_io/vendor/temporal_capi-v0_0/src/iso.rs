use temporal_rs::iso;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
#[diplomat::attr(auto, namespace = "temporal_rs")]
pub mod ffi {
    pub struct IsoDate {
        pub year: i32,
        pub month: u8,
        pub day: u8,
    }

    pub struct IsoDateTime {
        pub date: IsoDate,
        pub time: IsoTime,
    }

    pub struct IsoTime {
        pub hour: u8,
        pub minute: u8,
        pub second: u8,
        pub millisecond: u16,
        pub microsecond: u16,
        pub nanosecond: u16,
    }
}

impl From<ffi::IsoDate> for iso::IsoDate {
    fn from(other: ffi::IsoDate) -> Self {
        let mut this = Self::default();
        this.year = other.year;
        this.month = other.month;
        this.day = other.day;
        this
    }
}

impl From<ffi::IsoDateTime> for iso::IsoDateTime {
    fn from(other: ffi::IsoDateTime) -> Self {
        let mut this = Self::default();
        this.date = other.date.into();
        this.time = other.time.into();
        this
    }
}

impl From<ffi::IsoTime> for iso::IsoTime {
    fn from(other: ffi::IsoTime) -> Self {
        let mut this = Self::default();
        this.hour = other.hour;
        this.minute = other.minute;
        this.second = other.second;
        this.millisecond = other.millisecond;
        this.microsecond = other.microsecond;
        this.nanosecond = other.nanosecond;
        this
    }
}
