//! The [sbix (Standard Bitmap Graphics)](https://docs.microsoft.com/en-us/typography/opentype/spec/sbix) table

include!("../../generated/generated_sbix.rs");

impl<'a> Strike<'a> {
    pub fn glyph_data(&self, glyph_id: GlyphId) -> Result<Option<GlyphData<'a>>, ReadError> {
        let offsets = self.glyph_data_offsets();
        let start_ix = glyph_id.to_u16() as usize;
        let start = offsets.get(start_ix).ok_or(ReadError::OutOfBounds)?.get() as usize;
        let end = offsets
            .get(start_ix + 1)
            .ok_or(ReadError::OutOfBounds)?
            .get() as usize;
        if start == end {
            // Empty glyphs are okay
            return Ok(None);
        }
        let data = self
            .offset_data()
            .slice(start..end)
            .ok_or(ReadError::OutOfBounds)?;
        Ok(Some(GlyphData::read(data)?))
    }
}
