//! The [sbix (Standard Bitmap Graphics)](https://docs.microsoft.com/en-us/typography/opentype/spec/sbix) table

include!("../../generated/generated_sbix.rs");

impl Sbix {
    fn compile_header_flags(&self) -> u16 {
        self.flags.bits() & 1
    }
}
