use bitbybit::bitenum;

#[bitenum(u2, exhaustive = true)]
pub enum Exhaustive {
    A = 0b00,
    B = 0b01,
}

// This variant is const specified here to verify that we the enum still exists,
// even if the bitbybit bitenum code generation fails.
const VARIANT: Exhaustive = Exhaustive::A;

fn main() {
    // Similar purpose as the constant above: Check that any const API used exists.
    let _const_test: u8 = VARIANT.raw_value();
}
