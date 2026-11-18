//! impl subset() for maxp
use crate::{
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Subset, SubsetError, SubsetFlags,
};
use read_fonts::{
    tables::maxp::{Maxp, MaxpExtended},
    types::{Uint24, Version16Dot16},
    FontRef, TopLevelTable,
};
use write_fonts::FontBuilder;

// reference: subset() for maxp in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/hb-ot-maxp-table.hh#L97
macro_rules! subset_maxp {
    ($table:ident, $count:ty) => {
        impl Subset for $table<'_> {
            fn subset(
                &self,
                plan: &Plan,
                _font: &FontRef,
                s: &mut Serializer,
                _builder: &mut FontBuilder,
            ) -> Result<(), SubsetError> {
                let num_glyphs = <$count>::try_from(plan.num_output_glyphs).map_err(|_| {
                    s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW);
                    SubsetError::SubsetTableError($table::TAG)
                })?;
                s.embed_bytes(self.offset_data().as_bytes())
                    .map_err(|_| SubsetError::SubsetTableError($table::TAG))?;
                s.copy_assign(self.num_glyphs_byte_range().start, num_glyphs);

                //drop hints
                if self.version() == Version16Dot16::VERSION_1_0
                    && plan
                        .subset_flags
                        .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
                {
                    //maxZones
                    s.copy_assign_from_bytes(self.max_zones_byte_range().start, &[0, 1]);
                    //maxTwilightPoints..maxSizeOfInstructions
                    s.copy_assign(self.max_twilight_points_byte_range().start, 0_u16);
                    s.copy_assign(self.max_storage_byte_range().start, 0_u16);
                    s.copy_assign(self.max_function_defs_byte_range().start, 0_u16);
                    s.copy_assign(self.max_instruction_defs_byte_range().start, 0_u16);
                    s.copy_assign(self.max_stack_elements_byte_range().start, 0_u16);
                    s.copy_assign(self.max_size_of_instructions_byte_range().start, 0_u16);
                }
                Ok(())
            }
        }
    };
}
subset_maxp!(Maxp, u16);
subset_maxp!(MaxpExtended, Uint24);

#[cfg(test)]
mod tests;
