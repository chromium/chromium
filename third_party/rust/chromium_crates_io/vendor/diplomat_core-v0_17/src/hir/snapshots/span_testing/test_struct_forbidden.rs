#[diplomat::bridge]
mod ffi {
    struct Crimes<'a> {
        slice1: &'a str,
        slice1: &'a DiplomatStr,
        slice2: &'a [u8],
        slice3: Box<str>,
        slice3: Box<DiplomatStr>,
        slice4: Box<[u8]>,
    }
}