///
#[cfg_attr(feature = "derive", derive(crate::PanicFmt))]
pub enum Direction {
    ///
    Up,
    ///
    Down,
}

///
pub struct GenericStruct<'a> {
    x: &'a [u8],
    y: u8,
    z: char,
}

impl_panicfmt! {
    struct GenericStruct<'a> {
        x: &'a [u8],
        y: u8,
        z: char,
    }
}

///
pub struct ConcreteStruct {
    x: [u8; 4],
    y: u8,
    z: char,
}

impl_panicfmt! {
    struct ConcreteStruct {
        x: [u8; 4],
        y: u8,
        z: char,
    }
}
