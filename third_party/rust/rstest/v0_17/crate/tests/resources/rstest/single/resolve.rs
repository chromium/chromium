use rstest::*;

pub trait Tr {
    fn get() -> Self;
}

impl Tr for i32 {
    fn get() -> Self {
        42
    }
}

impl Tr for u32 {
    fn get() -> Self {
        42
    }
}

#[fixture]
pub fn fgen<T: Tr>() -> T {
    T::get()
}

#[rstest]
fn generics_u32(fgen: u32) {
    assert_eq!(fgen, 42u32);
}

#[rstest]
fn generics_i32(fgen: i32) {
    assert_eq!(fgen, 42i32);
}
