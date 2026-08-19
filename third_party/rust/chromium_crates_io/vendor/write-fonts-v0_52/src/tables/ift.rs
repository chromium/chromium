//! Incremental Font Transfer [Patch Map](https://w3c.github.io/IFT/Overview.html#font-format-extensions)
// TODO(garretrieger) remove once we've actually implemented stuff here.
#![allow(unused_variables)]
include!("../../generated/generated_ift.rs");

use crate::FontWrite;
use read_fonts::tables::ift::CompatibilityId;

impl FontWrite for CompatibilityId {
    fn write_into(&self, writer: &mut TableWriter) {
        writer.write_slice(self.as_slice());
    }
}
