//! impl subset() for head
use crate::{serialize::Serializer, Plan, Subset, SubsetError};
use write_fonts::{
    read::{tables::head::Head, FontRef, TopLevelTable},
    FontBuilder,
};

// reference: subset() for head in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/hb-ot-head-table.hh#L63
impl Subset for Head<'_> {
    fn subset(
        &self,
        _plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        s.embed_bytes(self.offset_data().as_bytes())
            .map_err(|_| SubsetError::SubsetTableError(Head::TAG))?;
        Ok(())
    }
}
