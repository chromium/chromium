// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
#[diplomat::abi_rename = "icu4x_{0}_mv1"]
#[diplomat::attr(auto, namespace = "icu4x")]
pub mod ffi {
    use diplomat_runtime::DiplomatChar;
    use icu_properties::props;

    #[diplomat::rust_link(icu::properties::props::BidiMirroringGlyph, Struct)]
    pub struct BidiMirroringGlyph {
        /// The mirroring glyph
        pub mirroring_glyph: DiplomatOption<DiplomatChar>,
        /// Whether the glyph is mirrored
        pub mirrored: bool,
        /// The paired bracket type
        pub paired_bracket_type: BidiPairedBracketType,
    }

    #[diplomat::rust_link(icu::properties::props::BidiPairedBracketType, Enum)]
    #[diplomat::enum_convert(props::BidiPairedBracketType, needs_wildcard)]
    pub enum BidiPairedBracketType {
        /// Represents Bidi_Paired_Bracket_Type=Open.
        Open,
        /// Represents Bidi_Paired_Bracket_Type=Close.
        Close,
        /// Represents Bidi_Paired_Bracket_Type=None.
        None,
    }

    impl BidiMirroringGlyph {
        #[diplomat::rust_link(icu::properties::props::EnumeratedProperty::for_char, FnInTrait)]
        #[cfg(feature = "compiled_data")]
        pub fn for_char(ch: DiplomatChar) -> Self {
            icu_properties::CodePointMapData::<props::BidiMirroringGlyph>::new()
                .get32(ch)
                .into()
        }
    }
}

impl From<icu_properties::props::BidiMirroringGlyph> for ffi::BidiMirroringGlyph {
    fn from(other: icu_properties::props::BidiMirroringGlyph) -> Self {
        Self {
            mirroring_glyph: other.mirroring_glyph.map(u32::from).into(),
            mirrored: other.mirrored,
            paired_bracket_type: other.paired_bracket_type.into(),
        }
    }
}
