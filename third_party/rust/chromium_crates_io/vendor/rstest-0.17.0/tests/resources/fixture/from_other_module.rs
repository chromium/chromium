
mod my_mod {
    use rstest::{fixture};

    #[fixture]
    pub fn mod_fixture() -> u32 { 42 }
}

use my_mod::mod_fixture;

#[test]
fn struct_access() {
    assert_eq!(42, mod_fixture::default());
}
