#[cfg(feature = "examples")]
pub fn main() {
    use arbitrary_int::u4;
    use bitbybit::bitfield;
    #[bitfield(u32, defmt_fields)]
    pub struct BitfieldU32 {
        #[bits(28..=31, rw)]
        val3: u4,
        #[bits(24..=27, rw)]
        val2: u4,
        #[bits(16..=23, rw)]
        val1: u8,
        #[bits(0..=15, rw)]
        val0: u16,
    }

    let bitfield = BitfieldU32::new_with_raw_value(0x0);
    defmt_impl_check(&bitfield);
}

pub fn defmt_impl_check<T: defmt::Format>(_: &T) {}

#[cfg(not(feature = "examples"))]
pub fn main() {}
