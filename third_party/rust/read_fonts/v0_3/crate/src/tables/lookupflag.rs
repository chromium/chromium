//! The lookup flag type.
//!
//! This is kind-of-but-not-quite-exactly a bit enumeration, and so we implement
//! it manually.

/// The [LookupFlag](https://learn.microsoft.com/en-us/typography/opentype/spec/chapter2#lookupFlag) bit enumeration.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct LookupFlag(u16);

impl LookupFlag {
    /// Return new, empty flags
    pub fn empty() -> Self {
        Self(0)
    }

    /// Construct a LookupFlag from a raw value, discarding invalid bits
    pub fn from_bits_truncate(bits: u16) -> Self {
        const VALID_BITS: u16 = !0x00E0;
        Self(bits & VALID_BITS)
    }

    /// Raw transmutation to u16.
    pub fn to_bits(self) -> u16 {
        self.0
    }

    /// This bit relates only to the correct processing of the cursive attachment
    /// lookup type (GPOS lookup type 3).
    ///
    /// When this bit is set, the last glyph in a given sequence to which the
    /// cursive attachment lookup is applied, will be positioned on the baseline.
    pub fn right_to_left(self) -> bool {
        (self.0 & 0x0001) != 0
    }

    /// This bit relates only to the correct processing of the cursive attachment
    /// lookup type (GPOS lookup type 3).
    ///
    /// When this bit is set, the last glyph in a given sequence to which the
    /// cursive attachment lookup is applied, will be positioned on the baseline.
    pub fn set_right_to_left(&mut self, val: bool) {
        if val {
            self.0 |= 0x0001
        } else {
            self.0 &= !0x0001
        }
    }

    /// If set, skips over base glyphs
    pub fn ignore_base_glyphs(self) -> bool {
        (self.0 & 0x0002) != 0
    }

    /// If set, skips over base glyphs
    pub fn set_ignore_base_glyphs(&mut self, val: bool) {
        if val {
            self.0 |= 0x0002
        } else {
            self.0 &= !0x0002
        }
    }

    /// If set, skips over ligatures
    pub fn ignore_ligatures(self) -> bool {
        (self.0 & 0x0004) != 0
    }

    /// If set, skips over ligatures
    pub fn set_ignore_ligatures(&mut self, val: bool) {
        if val {
            self.0 |= 0x0004
        } else {
            self.0 &= !0x0004
        }
    }

    /// If set, skips over all combining marks
    pub fn ignore_marks(self) -> bool {
        (self.0 & 0x0008) != 0
    }

    /// If set, skips over all combining marks
    pub fn set_ignore_marks(&mut self, val: bool) {
        if val {
            self.0 |= 0x0008
        } else {
            self.0 &= !0x0008
        }
    }

    /// If set, indicates that the lookup table structure is followed by a
    /// MarkFilteringSet field.
    ///
    /// The layout engine skips over all mark glyphs not in the mark filtering set
    /// indicated.
    pub fn use_mark_filtering_set(self) -> bool {
        (self.0 & 0x0010) != 0
    }

    /// If set, indicates that the lookup table structure is followed by a
    /// MarkFilteringSet field.
    ///
    /// The layout engine skips over all mark glyphs not in the mark filtering set
    /// indicated.
    pub fn set_use_mark_filtering_set(&mut self, val: bool) {
        if val {
            self.0 |= 0x0010
        } else {
            self.0 &= !0x0010
        }
    }

    /// If not zero, skips over all marks of attachment type different from specified.
    pub fn mark_attachment_type_mask(self) -> Option<u16> {
        let val = self.0 & 0xff00;
        if val == 0 {
            None
        } else {
            Some(val >> 8)
        }
    }

    /// If not zero, skips over all marks of attachment type different from specified.
    pub fn set_mark_attachment_type(&mut self, val: u16) {
        let val = (val & 0xff) << 8;
        self.0 = (self.0 & 0xff) | val;
    }
}

impl types::Scalar for LookupFlag {
    type Raw = <u16 as types::Scalar>::Raw;
    fn to_raw(self) -> Self::Raw {
        self.0.to_raw()
    }
    fn from_raw(raw: Self::Raw) -> Self {
        let t = <u16>::from_raw(raw);
        Self(t)
    }
}
