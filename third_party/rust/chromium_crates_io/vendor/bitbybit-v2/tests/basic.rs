use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {}

fn main() {
    let t = Test::DEFAULT;
    assert_eq!(0, t.raw_value);

    let t = Test::new_with_raw_value(45);
    assert_eq!(45, t.raw_value);
}
