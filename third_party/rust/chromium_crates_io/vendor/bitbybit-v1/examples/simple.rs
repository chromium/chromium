use arbitrary_int::u4;
use bitbybit::bitfield;

#[bitfield(u32, debug)]
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

pub fn main() {
    let bitfield = BitfieldU32::new_with_raw_value(0x0);
    println!("Bitfield with zero values: {:?}", bitfield);
    println!("Field 0: {}", bitfield.val0());
    println!("Field 1: {}", bitfield.val1());
    println!("Field 2: {}", bitfield.val2());
    println!("Field 3: {}", bitfield.val3());
    println!("Raw value: {}", bitfield.raw_value());

    let bitfield_with_values = BitfieldU32::new_with_raw_value(0x1234_5678);

    println!(
        "Bitfield, with value 0x1234_5678: {:?}",
        bitfield_with_values
    );
    println!("Field 0: {:#x}", bitfield_with_values.val0());
    println!("Field 1: {:#x}", bitfield_with_values.val1());
    println!("Field 2: {:#x}", bitfield_with_values.val2());
    println!("Field 3: {:#x}", bitfield_with_values.val3());
    println!("Raw value: {:#x}", bitfield_with_values.raw_value());
}
