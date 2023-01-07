use rstest::{rstest, fixture};

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
pub fn f<T: Tr>() -> T {
    T::get()
}

#[fixture]
pub fn fu32(f: u32) -> u32 {
    f
}

#[fixture]
pub fn fi32(f: i32) -> i32 {
    f
}

#[rstest]
fn test_u32(fu32: u32) {
    assert_eq!(fu32, 42)
}

#[rstest]
fn test_i32(fi32: i32) {
    assert_eq!(fi32, 42)
}
