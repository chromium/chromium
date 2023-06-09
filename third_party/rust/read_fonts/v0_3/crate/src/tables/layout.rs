//! OpenType Layout common table formats

#[path = "./lookupflag.rs"]
mod lookupflag;

pub use lookupflag::LookupFlag;

#[cfg(test)]
#[path = "../tests/layout.rs"]
mod tests;

include!("../../generated/generated_layout.rs");

impl<'a, T: FontRead<'a>> Lookup<'a, T> {
    pub fn get_subtable(&self, offset: Offset16) -> Result<T, ReadError> {
        self.resolve_offset(offset)
    }

    #[cfg(feature = "traversal")]
    fn traverse_lookup_flag(&self) -> traversal::FieldType<'a> {
        self.lookup_flag().to_bits().into()
    }
}

/// An enum for different possible tables referenced by [Feature::feature_params_offset]
pub enum FeatureParams<'a> {
    StylisticSet(StylisticSetParams<'a>),
    Size(SizeParams<'a>),
    CharacterVariant(CharacterVariantParams<'a>),
}

impl ReadArgs for FeatureParams<'_> {
    type Args = Tag;
}

impl<'a> FontReadWithArgs<'a> for FeatureParams<'a> {
    fn read_with_args(bytes: FontData<'a>, args: &Tag) -> Result<FeatureParams<'a>, ReadError> {
        match *args {
            t if t == Tag::new(b"size") => SizeParams::read(bytes).map(Self::Size),
            // to whoever is debugging this dumb bug I wrote: I'm sorry.
            t if &t.to_raw()[..2] == b"ss" => {
                StylisticSetParams::read(bytes).map(Self::StylisticSet)
            }
            t if &t.to_raw()[..2] == b"cv" => {
                CharacterVariantParams::read(bytes).map(Self::CharacterVariant)
            }
            // NOTE: what even is our error condition here? an offset exists but
            // we don't know the tag?
            _ => Err(ReadError::InvalidFormat(0xdead)),
        }
    }
}

#[cfg(feature = "traversal")]
impl<'a> SomeTable<'a> for FeatureParams<'a> {
    fn type_name(&self) -> &str {
        match self {
            FeatureParams::StylisticSet(table) => table.type_name(),
            FeatureParams::Size(table) => table.type_name(),
            FeatureParams::CharacterVariant(table) => table.type_name(),
        }
    }

    fn get_field(&self, idx: usize) -> Option<Field<'a>> {
        match self {
            FeatureParams::StylisticSet(table) => table.get_field(idx),
            FeatureParams::Size(table) => table.get_field(idx),
            FeatureParams::CharacterVariant(table) => table.get_field(idx),
        }
    }
}

impl FeatureTableSubstitutionRecord {
    pub fn alternate_feature<'a>(&self, data: FontData<'a>) -> Result<Feature<'a>, ReadError> {
        self.alternate_feature_offset()
            .resolve_with_args(data, &Tag::new(b"NULL"))
    }
}

impl CoverageTable<'_> {
    pub fn iter(&self) -> impl Iterator<Item = GlyphId> + '_ {
        // all one expression so that we have a single return type
        let (iter1, iter2) = match self {
            CoverageTable::Format1(t) => (Some(t.glyph_array().iter().map(|g| g.get())), None),
            CoverageTable::Format2(t) => {
                let iter = t.range_records().iter().flat_map(RangeRecord::iter);
                (None, Some(iter))
            }
        };

        iter1
            .into_iter()
            .flatten()
            .chain(iter2.into_iter().flatten())
    }
}

impl RangeRecord {
    fn iter(&self) -> impl Iterator<Item = GlyphId> + '_ {
        (self.start_glyph_id().to_u16()..=self.end_glyph_id().to_u16()).map(GlyphId::new)
    }
}

impl DeltaFormat {
    pub(crate) fn value_count(self, start_size: u16, end_size: u16) -> usize {
        let range_len = end_size.saturating_add(1).saturating_sub(start_size) as usize;
        let val_per_word = match self {
            DeltaFormat::Local2BitDeltas => 8,
            DeltaFormat::Local4BitDeltas => 4,
            DeltaFormat::Local8BitDeltas => 2,
            _ => return 0,
        };

        let count = range_len / val_per_word;
        let extra = (range_len % val_per_word).min(1);
        count + extra
    }
}
