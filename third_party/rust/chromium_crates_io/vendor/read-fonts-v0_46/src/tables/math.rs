//! The [MATH](https://learn.microsoft.com/en-us/typography/opentype/spec/math) table

use super::layout::{CoverageTable, DeviceOrVariationIndex};

include!("../../generated/generated_math.rs");

/// Which corner of a glyph a [`MathKern`] applies to.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum MathKernCorner {
    /// Above and to the right.
    TopRight,
    /// Above and to the left.
    TopLeft,
    /// Below and to the right.
    BottomRight,
    /// Below and to the left.
    BottomLeft,
}

/// The axis a glyph is stretched along.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum StretchAxis {
    /// Grows to the left and right, like an overbrace.
    Horizontal,
    /// Grows up and down, like a parenthesis.
    Vertical,
}

/// A `MATH` value read at a particular size.
///
/// The two fields are in different units and cannot simply be added: a device
/// table adjusts by whole pixels, so scaling its adjustment into design units
/// needs the units per em, which the `MATH` table does not know. A caller
/// working in font units wants `value + delta_px * upem / ppem`; one working
/// in pixels has the adjustment already.
#[derive(Clone, Copy, PartialEq, Eq, Default, Debug)]
pub struct MathValue {
    /// The value as the font stores it, in design units.
    pub value: i32,
    /// The adjustment the value's device table makes at the requested size,
    /// in pixels. Zero when there is no device table, when it does not cover
    /// that size, or when the offset names a `VariationIndex`.
    pub delta_px: i32,
}

/// One band of a [`MathKern`] table.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct MathKernEntry {
    /// The height the band reaches up to, or `None` for the last band, which
    /// is unbounded.
    pub max_height: Option<MathValue>,
    /// The kern to apply within the band.
    pub kern: MathValue,
}

impl Math<'_> {
    /// Whether this is one of the Cambria Math builds that stores
    /// `delimitedSubFormulaMinHeight` and `displayOperatorMinHeight` the wrong
    /// way round.
    ///
    /// A display mode n-ary operator should have the taller of the two
    /// thresholds, and in these builds it does not. Microsoft's implementation
    /// reads them the other way round, so anything that wants to lay out maths
    /// the way Word does has to swap them back; HarfBuzz does the same. This
    /// only reports the font, and leaves that decision to the caller, so that
    /// the constants keep saying what the font says.
    ///
    /// The font is recognized by its table length together with the two
    /// values, since either value alone is one a font could legitimately hold.
    /// That means it only answers `true` for a table read as a table: a
    /// [`Math`] read from a buffer with anything after it measures longer and
    /// is not recognized.
    ///
    /// See [harfbuzz#4653](https://github.com/harfbuzz/harfbuzz/issues/4653).
    pub fn has_swapped_min_heights(&self) -> bool {
        // The three known builds of cambria.ttc, by sha1:
        // ab4a4fe054d23061f3c039493d6f665cfda2ecf5
        // 086855301bff644f9d8827b88491fcf73a6d4cb9
        // b1e5a3feaca2ea3dfcf79ccb377de749ecf60343
        const TABLE_LEN: usize = 25722;
        const DELIMITED_SUB_FORMULA_MIN_HEIGHT: u16 = 3000;
        const DISPLAY_OPERATOR_MIN_HEIGHT: u16 = 2500;

        if self.offset_data().len() != TABLE_LEN {
            return false;
        }
        let Ok(constants) = self.math_constants() else {
            return false;
        };
        constants.delimited_sub_formula_min_height().to_u16() == DELIMITED_SUB_FORMULA_MIN_HEIGHT
            && constants.display_operator_min_height().to_u16() == DISPLAY_OPERATOR_MIN_HEIGHT
    }
}

impl MathValueRecord {
    /// The value as read at a size.
    ///
    /// `data` is the data of the table this record was read from, since its
    /// device offset is measured from there rather than from the record.
    pub fn value_for_ppem(&self, data: FontData<'_>, ppem: u16) -> MathValue {
        // A `VariationIndex` yields nothing: the deltas one names live in an
        // item variation store, and the `MATH` table has none.
        let delta_px = match self.device(data) {
            Some(Ok(DeviceOrVariationIndex::Device(device))) => device.delta_for_ppem(ppem),
            _ => 0,
        };
        MathValue {
            value: self.value().to_i16() as i32,
            delta_px,
        }
    }
}

/// Declares the constants of `MathConstants`, in the order the spec lists them.
///
/// One list defines the [`MathConstant`] enum, its numbering, and the arm of
/// [`MathConstants::constant`] that reads each one, so a constant cannot be
/// numbered as one field and read from another. The matches have no catch-all,
/// so a constant added here without a field will not compile.
///
/// The `record` constants are `MathValueRecord`s and can carry a device table.
/// The `int` and `uint` ones are stored as bare integers, so they never do.
macro_rules! math_constants {
    ($($variant:ident = $discriminant:literal, $kind:ident $field:ident;)+) => {
        /// A constant in the `MathConstants` subtable.
        ///
        /// The values must match HarfBuzz's [`hb_ot_math_constant_t`], which is
        /// also the order the OpenType spec lists the fields in.
        ///
        /// [`hb_ot_math_constant_t`]: https://github.com/harfbuzz/harfbuzz/blob/92e67ef19f2d595b0fe81f05a80783a321bb918f/src/hb-ot-math.h#L133
        #[derive(Clone, Copy, PartialEq, Eq, Debug)]
        #[repr(u8)]
        pub enum MathConstant {
            $(
                #[doc = concat!("See [`MathConstants::", stringify!($field), "`].")]
                $variant = $discriminant,
            )+
        }

        impl MathConstant {
            /// Every constant, in the order the spec lists them.
            ///
            /// A constant's position here is its numeric value.
            pub const ALL: &'static [MathConstant] = &[$(MathConstant::$variant,)+];

            /// The constant with a given numeric value, or `None` if the value
            /// names no constant.
            pub fn new(value: u8) -> Option<Self> {
                Self::ALL.get(value as usize).copied()
            }
        }

        impl MathConstants<'_> {
            /// The value of a constant, in design units.
            pub fn constant(&self, constant: MathConstant) -> i32 {
                match constant {
                    $(
                        MathConstant::$variant => math_constants!(@value self, $kind $field),
                    )+
                }
            }

            /// The value of a constant, as read at a size.
            ///
            /// The four percentages and the two minimum heights are stored as
            /// bare integers, so their [`delta_px`][MathValue::delta_px] is
            /// always zero.
            pub fn constant_for_ppem(&self, constant: MathConstant, ppem: u16) -> MathValue {
                match constant {
                    $(
                        MathConstant::$variant => math_constants!(@for_ppem self, $kind $field, ppem),
                    )+
                }
            }
        }
    };
    (@value $self:ident, record $field:ident) => {
        $self.$field().value().to_i16() as i32
    };
    (@value $self:ident, int $field:ident) => {
        $self.$field() as i32
    };
    (@value $self:ident, uint $field:ident) => {
        $self.$field().to_u16() as i32
    };
    (@for_ppem $self:ident, record $field:ident, $ppem:ident) => {
        $self.$field().value_for_ppem($self.offset_data(), $ppem)
    };
    (@for_ppem $self:ident, int $field:ident, $ppem:ident) => {
        MathValue { value: $self.$field() as i32, delta_px: 0 }
    };
    (@for_ppem $self:ident, uint $field:ident, $ppem:ident) => {
        MathValue { value: $self.$field().to_u16() as i32, delta_px: 0 }
    };
}

math_constants! {
    ScriptPercentScaleDown = 0, int script_percent_scale_down;
    ScriptScriptPercentScaleDown = 1, int script_script_percent_scale_down;
    DelimitedSubFormulaMinHeight = 2, uint delimited_sub_formula_min_height;
    DisplayOperatorMinHeight = 3, uint display_operator_min_height;
    MathLeading = 4, record math_leading;
    AxisHeight = 5, record axis_height;
    AccentBaseHeight = 6, record accent_base_height;
    FlattenedAccentBaseHeight = 7, record flattened_accent_base_height;
    SubscriptShiftDown = 8, record subscript_shift_down;
    SubscriptTopMax = 9, record subscript_top_max;
    SubscriptBaselineDropMin = 10, record subscript_baseline_drop_min;
    SuperscriptShiftUp = 11, record superscript_shift_up;
    SuperscriptShiftUpCramped = 12, record superscript_shift_up_cramped;
    SuperscriptBottomMin = 13, record superscript_bottom_min;
    SuperscriptBaselineDropMax = 14, record superscript_baseline_drop_max;
    SubSuperscriptGapMin = 15, record sub_superscript_gap_min;
    SuperscriptBottomMaxWithSubscript = 16, record superscript_bottom_max_with_subscript;
    SpaceAfterScript = 17, record space_after_script;
    UpperLimitGapMin = 18, record upper_limit_gap_min;
    UpperLimitBaselineRiseMin = 19, record upper_limit_baseline_rise_min;
    LowerLimitGapMin = 20, record lower_limit_gap_min;
    LowerLimitBaselineDropMin = 21, record lower_limit_baseline_drop_min;
    StackTopShiftUp = 22, record stack_top_shift_up;
    StackTopDisplayStyleShiftUp = 23, record stack_top_display_style_shift_up;
    StackBottomShiftDown = 24, record stack_bottom_shift_down;
    StackBottomDisplayStyleShiftDown = 25, record stack_bottom_display_style_shift_down;
    StackGapMin = 26, record stack_gap_min;
    StackDisplayStyleGapMin = 27, record stack_display_style_gap_min;
    StretchStackTopShiftUp = 28, record stretch_stack_top_shift_up;
    StretchStackBottomShiftDown = 29, record stretch_stack_bottom_shift_down;
    StretchStackGapAboveMin = 30, record stretch_stack_gap_above_min;
    StretchStackGapBelowMin = 31, record stretch_stack_gap_below_min;
    FractionNumeratorShiftUp = 32, record fraction_numerator_shift_up;
    FractionNumeratorDisplayStyleShiftUp = 33, record fraction_numerator_display_style_shift_up;
    FractionDenominatorShiftDown = 34, record fraction_denominator_shift_down;
    FractionDenominatorDisplayStyleShiftDown = 35, record fraction_denominator_display_style_shift_down;
    FractionNumeratorGapMin = 36, record fraction_numerator_gap_min;
    FractionNumDisplayStyleGapMin = 37, record fraction_num_display_style_gap_min;
    FractionRuleThickness = 38, record fraction_rule_thickness;
    FractionDenominatorGapMin = 39, record fraction_denominator_gap_min;
    FractionDenomDisplayStyleGapMin = 40, record fraction_denom_display_style_gap_min;
    SkewedFractionHorizontalGap = 41, record skewed_fraction_horizontal_gap;
    SkewedFractionVerticalGap = 42, record skewed_fraction_vertical_gap;
    OverbarVerticalGap = 43, record overbar_vertical_gap;
    OverbarRuleThickness = 44, record overbar_rule_thickness;
    OverbarExtraAscender = 45, record overbar_extra_ascender;
    UnderbarVerticalGap = 46, record underbar_vertical_gap;
    UnderbarRuleThickness = 47, record underbar_rule_thickness;
    UnderbarExtraDescender = 48, record underbar_extra_descender;
    RadicalVerticalGap = 49, record radical_vertical_gap;
    RadicalDisplayStyleVerticalGap = 50, record radical_display_style_vertical_gap;
    RadicalRuleThickness = 51, record radical_rule_thickness;
    RadicalExtraAscender = 52, record radical_extra_ascender;
    RadicalKernBeforeDegree = 53, record radical_kern_before_degree;
    RadicalKernAfterDegree = 54, record radical_kern_after_degree;
    RadicalDegreeBottomRaisePercent = 55, int radical_degree_bottom_raise_percent;
}

impl MathGlyphInfo<'_> {
    /// Whether a glyph is an extended shape: one tall enough that a following
    /// script is positioned against its own height rather than the font's.
    pub fn is_extended_shape(&self, glyph: GlyphId) -> bool {
        self.extended_shape_coverage()
            .and_then(|coverage| coverage.ok())
            .and_then(|coverage| coverage.get(glyph))
            .is_some()
    }
}

impl MathItalicsCorrectionInfo<'_> {
    /// The italics correction for a glyph in design units, or `None` where the
    /// table does not cover it.
    pub fn correction(&self, glyph: GlyphId) -> Option<i32> {
        Some(self.record(glyph)?.value().to_i16() as i32)
    }

    /// The italics correction for a glyph, as read at a size, or `None` where
    /// the table does not cover it.
    pub fn correction_for_ppem(&self, glyph: GlyphId, ppem: u16) -> Option<MathValue> {
        Some(self.record(glyph)?.value_for_ppem(self.offset_data(), ppem))
    }

    fn record(&self, glyph: GlyphId) -> Option<&MathValueRecord> {
        let index = self.coverage().ok()?.get(glyph)?;
        self.italics_correction().get(index as usize)
    }
}

impl MathTopAccentAttachment<'_> {
    /// The horizontal position at which an accent sits above a glyph, in
    /// design units, or `None` where the table does not cover it.
    pub fn attachment(&self, glyph: GlyphId) -> Option<i32> {
        Some(self.record(glyph)?.value().to_i16() as i32)
    }

    /// The horizontal position at which an accent sits above a glyph, as read
    /// at a size, or `None` where the table does not cover it.
    pub fn attachment_for_ppem(&self, glyph: GlyphId, ppem: u16) -> Option<MathValue> {
        Some(self.record(glyph)?.value_for_ppem(self.offset_data(), ppem))
    }

    fn record(&self, glyph: GlyphId) -> Option<&MathValueRecord> {
        let index = self.top_accent_coverage().ok()?.get(glyph)?;
        self.top_accent_attachment().get(index as usize)
    }
}

impl<'a> MathKernInfo<'a> {
    /// The kerning for one corner of a glyph, or `None` where the table does
    /// not cover the glyph or gives that corner nothing.
    pub fn kern(&self, glyph: GlyphId, corner: MathKernCorner) -> Option<MathKern<'a>> {
        let index = self.math_kern_coverage().ok()?.get(glyph)?;
        let record = self.math_kern_info_records().get(index as usize)?;
        let data = self.offset_data();
        match corner {
            MathKernCorner::TopRight => record.top_right_math_kern(data),
            MathKernCorner::TopLeft => record.top_left_math_kern(data),
            MathKernCorner::BottomRight => record.bottom_right_math_kern(data),
            MathKernCorner::BottomLeft => record.bottom_left_math_kern(data),
        }?
        .ok()
    }
}

impl MathKern<'_> {
    /// The kern to apply at a height above the baseline, in design units.
    ///
    /// The table divides the vertical range into bands, and this answers the
    /// value for the band `correction_height` falls in: the one whose lower
    /// bound is the last correction height at or below it. A height above
    /// every correction height takes the final value.
    ///
    /// Correction heights are stored in ascending order and there are rarely
    /// more than a handful, so this walks them.
    ///
    /// The heights are compared as the font stores them, without the device
    /// adjustments they may carry. Applying those would mean choosing a size
    /// and a coordinate space to compare in, which is the caller's to choose:
    /// [`entries_for_ppem`][Self::entries_for_ppem] hands over the bands so it
    /// can.
    pub fn kerning(&self, correction_height: i32) -> Option<i32> {
        let value = self.kern_values().get(self.band(correction_height))?;
        Some(value.value().to_i16() as i32)
    }

    /// The upper bound and kern of each band, lowest first.
    ///
    /// Every value comes as read at `ppem`, carrying its own adjustment.
    ///
    /// This is what to pick a kern from when the adjustments matter. Which
    /// band a height falls in depends on the space the comparison happens in
    /// -- design units, pixels, or a scaled position, the last of which also
    /// decides whether the vertical axis points up or down -- and only the
    /// caller knows that.
    pub fn entries_for_ppem(&self, ppem: u16) -> impl Iterator<Item = MathKernEntry> + '_ {
        let data = self.offset_data();
        let heights = self.correction_height();
        self.kern_values()
            .iter()
            .enumerate()
            .map(move |(i, kern)| MathKernEntry {
                max_height: heights
                    .get(i)
                    .map(|height| height.value_for_ppem(data, ppem)),
                kern: kern.value_for_ppem(data, ppem),
            })
    }

    /// The index of the band a height falls in.
    fn band(&self, correction_height: i32) -> usize {
        let heights = self.correction_height();
        heights
            .iter()
            .position(|height| correction_height < height.value().to_i16() as i32)
            .unwrap_or(heights.len())
    }
}

impl<'a> MathVariants<'a> {
    /// How a glyph is stretched along an axis, or `None` where the table does
    /// not stretch it that way.
    pub fn glyph_construction(
        &self,
        glyph: GlyphId,
        axis: StretchAxis,
    ) -> Option<MathGlyphConstruction<'a>> {
        let index = match axis {
            StretchAxis::Vertical => self.vert_glyph_coverage(),
            StretchAxis::Horizontal => self.horiz_glyph_coverage(),
        }?
        .ok()?
        .get(glyph)?;
        match axis {
            StretchAxis::Vertical => self.vert_glyph_constructions(),
            StretchAxis::Horizontal => self.horiz_glyph_constructions(),
        }
        .get(index as usize)
        .ok()
    }
}

impl GlyphAssembly<'_> {
    /// The italics correction of the assembled glyph, as read at a size.
    pub fn italics_correction_for_ppem(&self, ppem: u16) -> MathValue {
        self.italics_correction()
            .value_for_ppem(self.offset_data(), ppem)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;

    /// Index of `math_leading` among the MathValueRecords of MathConstants.
    ///
    /// The test data gives it a `VariationIndex`, which the `MATH` table has
    /// no store to resolve against.
    const MATH_LEADING_INDEX: usize = 0;

    /// Index of `axis_height` among the MathValueRecords of MathConstants.
    ///
    /// The test data gives it the one `Device` table among the constants.
    const AXIS_HEIGHT_INDEX: usize = 1;

    /// The MathValueRecords of MathConstants, in spec order, named as they are
    /// on the generated table.
    ///
    /// The test data gives the record at index `i` the value `100 + i`, so a
    /// field transcribed out of order reads back as one of its neighbours.
    const VALUE_RECORD_ORDER: &[&str] = &[
        "math_leading",
        "axis_height",
        "accent_base_height",
        "flattened_accent_base_height",
        "subscript_shift_down",
        "subscript_top_max",
        "subscript_baseline_drop_min",
        "superscript_shift_up",
        "superscript_shift_up_cramped",
        "superscript_bottom_min",
        "superscript_baseline_drop_max",
        "sub_superscript_gap_min",
        "superscript_bottom_max_with_subscript",
        "space_after_script",
        "upper_limit_gap_min",
        "upper_limit_baseline_rise_min",
        "lower_limit_gap_min",
        "lower_limit_baseline_drop_min",
        "stack_top_shift_up",
        "stack_top_display_style_shift_up",
        "stack_bottom_shift_down",
        "stack_bottom_display_style_shift_down",
        "stack_gap_min",
        "stack_display_style_gap_min",
        "stretch_stack_top_shift_up",
        "stretch_stack_bottom_shift_down",
        "stretch_stack_gap_above_min",
        "stretch_stack_gap_below_min",
        "fraction_numerator_shift_up",
        "fraction_numerator_display_style_shift_up",
        "fraction_denominator_shift_down",
        "fraction_denominator_display_style_shift_down",
        "fraction_numerator_gap_min",
        "fraction_num_display_style_gap_min",
        "fraction_rule_thickness",
        "fraction_denominator_gap_min",
        "fraction_denom_display_style_gap_min",
        "skewed_fraction_horizontal_gap",
        "skewed_fraction_vertical_gap",
        "overbar_vertical_gap",
        "overbar_rule_thickness",
        "overbar_extra_ascender",
        "underbar_vertical_gap",
        "underbar_rule_thickness",
        "underbar_extra_descender",
        "radical_vertical_gap",
        "radical_display_style_vertical_gap",
        "radical_rule_thickness",
        "radical_extra_ascender",
        "radical_kern_before_degree",
        "radical_kern_after_degree",
    ];

    /// A MATH table with all three subtables, built so that every value
    /// identifies the field it belongs to.
    fn math_table() -> BeBuffer {
        let mut buf = BeBuffer::new()
            // MATH header
            .push(MajorMinor::VERSION_1_0)
            .push_with_tag(0u16, "constants_offset")
            .push_with_tag(0u16, "glyph_info_offset")
            .push_with_tag(0u16, "variants_offset");

        // -- MathConstants --
        let constants = buf.len();
        buf = buf
            .push(1i16) // script_percent_scale_down
            .push(2i16) // script_script_percent_scale_down
            .push_with_tag(3u16, "delimited_sub_formula_min_height")
            .push_with_tag(4u16, "display_operator_min_height");
        for i in 0..VALUE_RECORD_ORDER.len() {
            buf = buf.push(100i16 + i as i16); // value
            buf = match i {
                MATH_LEADING_INDEX => buf.push_with_tag(0u16, "math_leading_device"),
                AXIS_HEIGHT_INDEX => buf.push_with_tag(0u16, "axis_height_device"),
                _ => buf.push(0u16), // null device
            };
        }
        buf = buf.push(5i16); // radical_degree_bottom_raise_percent

        // -- MathGlyphInfo --
        let glyph_info = buf.len();
        buf = buf
            .push_with_tag(0u16, "italics_offset")
            .push(0u16) // top accent attachment: null
            .push(0u16) // extended shape coverage: null
            .push_with_tag(0u16, "kern_info_offset");

        // MathItalicsCorrectionInfo, covering one glyph, with a device table.
        let italics = buf.len();
        buf = buf
            .push_with_tag(0u16, "italics_coverage_offset")
            .push(1u16) // count
            .push(37i16) // value
            .push_with_tag(0u16, "italics_device_offset");
        let italics_coverage = buf.len();
        buf = buf
            .push(1u16) // coverage format 1
            .push(1u16) // glyph count
            .push(9u16); // glyph 9
        let italics_device = buf.len();
        buf = buf
            .push(11u16) // start size
            .push(12u16) // end size
            .push(3u16) // delta format: 8 bit
            .push(0x05FFu16); // ppem 11 -> 5, ppem 12 -> -1

        // MathKernInfo, one record carrying a single top-right kern.
        let kern_info = buf.len();
        buf = buf
            .push_with_tag(0u16, "kern_coverage_offset")
            .push(1u16) // count
            .push_with_tag(0u16, "top_right_kern_offset")
            .push(0u16) // top left: null
            .push(0u16) // bottom right: null
            .push(0u16); // bottom left: null
        let kern_coverage = buf.len();
        buf = buf.push(1u16).push(1u16).push(9u16);
        let math_kern = buf.len();
        buf = buf
            .push(2u16) // height_count
            .push(20i16) // correction height 0
            .push(0u16)
            .push(40i16) // correction height 1
            .push_with_tag(0u16, "kern_height_device")
            .push(-1i16) // kern value 0
            .push(0u16)
            .push(-2i16) // kern value 1
            .push_with_tag(0u16, "kern_value_device")
            .push(-3i16) // kern value 2, the height_count + 1 entry
            .push(0u16);

        // -- MathVariants --
        let variants = buf.len();
        buf = buf
            .push(6u16) // min_connector_overlap
            .push_with_tag(0u16, "vert_coverage_offset")
            .push(0u16) // horizontal coverage: null
            .push(1u16) // vert_glyph_count
            .push(0u16) // horiz_glyph_count
            .push_with_tag(0u16, "vert_construction_offset");
        let vert_coverage = buf.len();
        buf = buf.push(1u16).push(1u16).push(9u16);
        let construction = buf.len();
        buf = buf
            .push_with_tag(0u16, "assembly_offset")
            .push(2u16) // variant_count
            .push(21u16) // variant glyph
            .push(300u16) // advance
            .push(22u16)
            .push(400u16);
        let assembly = buf.len();
        buf = buf
            .push(17i16) // italics correction value
            .push(0u16) // italics correction device: null
            .push(1u16) // part_count
            .push(23u16) // glyph
            .push(10u16) // start connector
            .push(11u16) // end connector
            .push(50u16) // full advance
            .push(1u16); // part flags: extender

        // Device tables inside MathKern: one on a correction height, one on a
        // kern value, so the two are told apart.
        let kern_height_device = buf.len();
        buf = buf
            .push(10u16) // start size
            .push(11u16) // end size
            .push(3u16) // delta format: 8 bit
            .push(0x04FAu16); // ppem 10 -> 4, ppem 11 -> -6
        let kern_value_device = buf.len();
        buf = buf
            .push(10u16) // start size
            .push(11u16) // end size
            .push(3u16) // delta format: 8 bit
            .push(0x0102u16); // ppem 10 -> 1, ppem 11 -> 2

        // A Device table for axis_height.
        let axis_height_device = buf.len();
        buf = buf
            .push(10u16) // start size
            .push(12u16) // end size
            .push(3u16) // delta format: 8 bit
            .push(0xFE03u16) // ppem 10 -> -2, ppem 11 -> 3
            .push(0x0700u16); // ppem 12 -> 7

        // A VariationIndex for math_leading. Nothing can resolve it: the
        // deltas it names would live in an item variation store, and the MATH
        // table has none.
        let math_leading_varidx = buf.len();
        buf = buf
            .push(0u16) // delta set outer index
            .push(0u16) // delta set inner index
            .push(0x8000u16); // delta format: VARIATION_INDEX

        buf.write_at("constants_offset", constants as u16);
        buf.write_at("glyph_info_offset", glyph_info as u16);
        buf.write_at("variants_offset", variants as u16);
        buf.write_at("italics_offset", (italics - glyph_info) as u16);
        buf.write_at("kern_info_offset", (kern_info - glyph_info) as u16);
        buf.write_at(
            "italics_coverage_offset",
            (italics_coverage - italics) as u16,
        );
        buf.write_at("italics_device_offset", (italics_device - italics) as u16);
        buf.write_at("kern_coverage_offset", (kern_coverage - kern_info) as u16);
        buf.write_at("top_right_kern_offset", (math_kern - kern_info) as u16);
        // Both are measured from MathKern, the table holding the records.
        buf.write_at(
            "kern_height_device",
            (kern_height_device - math_kern) as u16,
        );
        buf.write_at("kern_value_device", (kern_value_device - math_kern) as u16);
        buf.write_at("vert_coverage_offset", (vert_coverage - variants) as u16);
        buf.write_at("vert_construction_offset", (construction - variants) as u16);
        buf.write_at("assembly_offset", (assembly - construction) as u16);
        // Both are measured from MathConstants, the table holding the records.
        buf.write_at(
            "axis_height_device",
            (axis_height_device - constants) as u16,
        );
        buf.write_at(
            "math_leading_device",
            (math_leading_varidx - constants) as u16,
        );
        buf
    }

    /// A value with no device adjustment.
    fn plain(value: i32) -> MathValue {
        MathValue { value, delta_px: 0 }
    }

    /// A value and the pixel adjustment its device table makes.
    fn adjusted(value: i32, delta_px: i32) -> MathValue {
        MathValue { value, delta_px }
    }

    /// A kern band.
    fn entry(max_height: Option<MathValue>, kern: MathValue) -> MathKernEntry {
        MathKernEntry { max_height, kern }
    }

    /// The length of the MATH table in the known-bad Cambria Math builds.
    const CAMBRIA_TABLE_LEN: usize = 25722;

    /// The test table with the two minimum heights set, padded to a length.
    fn cambria_table(len: usize, delimited: u16, display_operator: u16) -> BeBuffer {
        let mut buf = math_table();
        buf.write_at("delimited_sub_formula_min_height", delimited);
        buf.write_at("display_operator_min_height", display_operator);
        let padding = len - buf.len();
        buf.extend(vec![0u8; padding])
    }

    fn math(buf: &BeBuffer) -> Math<'_> {
        Math::read(buf.data().into()).unwrap()
    }

    #[test]
    fn constants_are_in_spec_order() {
        let buf = math_table();
        let math = math(&buf);
        let constants = math.math_constants().unwrap();

        assert_eq!(constants.script_percent_scale_down(), 1);
        assert_eq!(constants.script_script_percent_scale_down(), 2);
        assert_eq!(constants.delimited_sub_formula_min_height(), UfWord::new(3));
        assert_eq!(constants.display_operator_min_height(), UfWord::new(4));
        assert_eq!(constants.radical_degree_bottom_raise_percent(), 5);

        // Each record carries its own index, so reading them in the order they
        // are named checks the whole run in one go.
        let values = [
            constants.math_leading(),
            constants.axis_height(),
            constants.accent_base_height(),
            constants.flattened_accent_base_height(),
            constants.subscript_shift_down(),
            constants.subscript_top_max(),
            constants.subscript_baseline_drop_min(),
            constants.superscript_shift_up(),
            constants.superscript_shift_up_cramped(),
            constants.superscript_bottom_min(),
            constants.superscript_baseline_drop_max(),
            constants.sub_superscript_gap_min(),
            constants.superscript_bottom_max_with_subscript(),
            constants.space_after_script(),
            constants.upper_limit_gap_min(),
            constants.upper_limit_baseline_rise_min(),
            constants.lower_limit_gap_min(),
            constants.lower_limit_baseline_drop_min(),
            constants.stack_top_shift_up(),
            constants.stack_top_display_style_shift_up(),
            constants.stack_bottom_shift_down(),
            constants.stack_bottom_display_style_shift_down(),
            constants.stack_gap_min(),
            constants.stack_display_style_gap_min(),
            constants.stretch_stack_top_shift_up(),
            constants.stretch_stack_bottom_shift_down(),
            constants.stretch_stack_gap_above_min(),
            constants.stretch_stack_gap_below_min(),
            constants.fraction_numerator_shift_up(),
            constants.fraction_numerator_display_style_shift_up(),
            constants.fraction_denominator_shift_down(),
            constants.fraction_denominator_display_style_shift_down(),
            constants.fraction_numerator_gap_min(),
            constants.fraction_num_display_style_gap_min(),
            constants.fraction_rule_thickness(),
            constants.fraction_denominator_gap_min(),
            constants.fraction_denom_display_style_gap_min(),
            constants.skewed_fraction_horizontal_gap(),
            constants.skewed_fraction_vertical_gap(),
            constants.overbar_vertical_gap(),
            constants.overbar_rule_thickness(),
            constants.overbar_extra_ascender(),
            constants.underbar_vertical_gap(),
            constants.underbar_rule_thickness(),
            constants.underbar_extra_descender(),
            constants.radical_vertical_gap(),
            constants.radical_display_style_vertical_gap(),
            constants.radical_rule_thickness(),
            constants.radical_extra_ascender(),
            constants.radical_kern_before_degree(),
            constants.radical_kern_after_degree(),
        ];
        assert_eq!(values.len(), VALUE_RECORD_ORDER.len());
        for (i, (value, name)) in values.iter().zip(VALUE_RECORD_ORDER).enumerate() {
            assert_eq!(
                value.value(),
                FWord::new(100 + i as i16),
                "{name} is not at spec index {i}"
            );
        }
    }

    #[test]
    fn cambria_math_is_recognized_by_all_three_values() {
        // The whole fingerprint: the table length and both heights.
        let buf = cambria_table(CAMBRIA_TABLE_LEN, 3000, 2500);
        assert!(math(&buf).has_swapped_min_heights());

        // A byte longer is a different build of the font.
        let buf = cambria_table(CAMBRIA_TABLE_LEN + 1, 3000, 2500);
        assert!(!math(&buf).has_swapped_min_heights());

        // The right length, but the heights are the right way round.
        let buf = cambria_table(CAMBRIA_TABLE_LEN, 2500, 3000);
        assert!(!math(&buf).has_swapped_min_heights());

        // And an ordinary font is not it.
        let buf = math_table();
        assert!(!math(&buf).has_swapped_min_heights());
    }

    #[test]
    fn constant_numbering_matches_harfbuzz() {
        // These cross to and from C as plain integers, so the two ends have to
        // agree on every one of them.
        assert_eq!(MathConstant::ALL.len(), 56);
        for (i, constant) in MathConstant::ALL.iter().enumerate() {
            assert_eq!(*constant as usize, i);
            assert_eq!(MathConstant::new(i as u8), Some(*constant));
        }
        assert_eq!(MathConstant::ScriptPercentScaleDown as u8, 0);
        assert_eq!(MathConstant::MathLeading as u8, 4);
        assert_eq!(MathConstant::AxisHeight as u8, 5);
        assert_eq!(MathConstant::RadicalKernAfterDegree as u8, 54);
        assert_eq!(MathConstant::RadicalDegreeBottomRaisePercent as u8, 55);
        assert_eq!(MathConstant::new(56), None);
    }

    #[test]
    fn every_constant_reads_its_own_field() {
        let buf = math_table();
        let math = math(&buf);
        let constants = math.math_constants().unwrap();
        let first_record = MathConstant::MathLeading as i32;

        // The test data numbers each MathValueRecord by its position, so a
        // constant wired to the wrong field reads back as its neighbour.
        for constant in MathConstant::ALL {
            let expected = match constant {
                MathConstant::ScriptPercentScaleDown => 1,
                MathConstant::ScriptScriptPercentScaleDown => 2,
                MathConstant::DelimitedSubFormulaMinHeight => 3,
                MathConstant::DisplayOperatorMinHeight => 4,
                MathConstant::RadicalDegreeBottomRaisePercent => 5,
                record => 100 + (*record as i32 - first_record),
            };
            assert_eq!(
                constants.constant(*constant),
                expected,
                "{constant:?} does not read its own field"
            );
            // With no device table the two agree, and the bare integers never
            // have one at all.
            assert_eq!(constants.constant_for_ppem(*constant, 12).value, expected);
        }
    }

    #[test]
    fn constant_device_deltas_track_ppem() {
        let buf = math_table();
        let math = math(&buf);
        let constants = math.math_constants().unwrap();
        let stored = 100 + AXIS_HEIGHT_INDEX as i32;

        // axis_height's device covers 10 to 12.
        assert_eq!(
            constants.constant_for_ppem(MathConstant::AxisHeight, 10),
            adjusted(stored, -2)
        );
        assert_eq!(
            constants.constant_for_ppem(MathConstant::AxisHeight, 11),
            adjusted(stored, 3)
        );
        assert_eq!(
            constants.constant_for_ppem(MathConstant::AxisHeight, 12),
            adjusted(stored, 7)
        );
        // Outside the range it covers, and at no size at all.
        assert_eq!(
            constants.constant_for_ppem(MathConstant::AxisHeight, 9),
            plain(stored)
        );
        assert_eq!(
            constants.constant_for_ppem(MathConstant::AxisHeight, 13),
            plain(stored)
        );
        assert_eq!(
            constants.constant_for_ppem(MathConstant::AxisHeight, 0),
            plain(stored)
        );
    }

    #[test]
    fn variation_index_yields_no_adjustment() {
        let buf = math_table();
        let math = math(&buf);
        let constants = math.math_constants().unwrap();
        let stored = 100 + MATH_LEADING_INDEX as i32;

        // math_leading's offset points at a VariationIndex. The deltas it
        // names would live in an item variation store, and MATH has none, so
        // there is nothing to apply at any size.
        assert_eq!(constants.constant(MathConstant::MathLeading), stored);
        for ppem in [0u16, 10, 11, 12, 100] {
            assert_eq!(
                constants.constant_for_ppem(MathConstant::MathLeading, ppem),
                plain(stored)
            );
        }
    }

    #[test]
    fn italics_correction_resolves_against_its_parent() {
        let buf = math_table();
        let math = math(&buf);
        let info = math
            .math_glyph_info()
            .unwrap()
            .math_italics_correction_info()
            .unwrap()
            .unwrap();

        assert_eq!(info.coverage().unwrap().get(GlyphId::new(9)), Some(0));
        assert_eq!(info.correction(GlyphId::new(9)), Some(37));
        // Glyph 10 is outside the coverage.
        assert_eq!(info.correction(GlyphId::new(10)), None);

        // The device offset is measured from MathItalicsCorrectionInfo rather
        // than from the record, so it only resolves against the parent's data.
        assert_eq!(
            info.correction_for_ppem(GlyphId::new(9), 11),
            Some(adjusted(37, 5))
        );
        assert_eq!(
            info.correction_for_ppem(GlyphId::new(9), 12),
            Some(adjusted(37, -1))
        );
        assert_eq!(
            info.correction_for_ppem(GlyphId::new(9), 20),
            Some(plain(37))
        );
    }

    #[test]
    fn glyph_info_lookups() {
        let buf = math_table();
        let math = math(&buf);
        let glyph_info = math.math_glyph_info().unwrap();

        // The font has no top accent attachment or extended shape coverage.
        assert!(glyph_info.math_top_accent_attachment().is_none());
        assert!(!glyph_info.is_extended_shape(GlyphId::new(9)));
    }

    #[test]
    fn math_kern_has_one_more_value_than_height() {
        let buf = math_table();
        let math = math(&buf);
        let kern_info = math
            .math_glyph_info()
            .unwrap()
            .math_kern_info()
            .unwrap()
            .unwrap();

        let kern = kern_info
            .kern(GlyphId::new(9), MathKernCorner::TopRight)
            .unwrap();

        assert_eq!(kern.height_count(), 2);
        assert_eq!(
            kern.correction_height()
                .iter()
                .map(|v| v.value())
                .collect::<Vec<_>>(),
            vec![FWord::new(20), FWord::new(40)]
        );
        // Three kern values for two heights.
        assert_eq!(
            kern.kern_values()
                .iter()
                .map(|v| v.value())
                .collect::<Vec<_>>(),
            vec![FWord::new(-1), FWord::new(-2), FWord::new(-3)]
        );

        // The other three corners are null in this font.
        assert!(kern_info
            .kern(GlyphId::new(9), MathKernCorner::TopLeft)
            .is_none());
        // And glyph 10 is outside the coverage.
        assert!(kern_info
            .kern(GlyphId::new(10), MathKernCorner::TopRight)
            .is_none());
    }

    #[test]
    fn kerning_picks_the_band_the_height_falls_in() {
        let buf = math_table();
        let math = math(&buf);
        let kern = math
            .math_glyph_info()
            .unwrap()
            .math_kern_info()
            .unwrap()
            .unwrap()
            .kern(GlyphId::new(9), MathKernCorner::TopRight)
            .unwrap();

        // Correction heights are 20 and 40, kern values -1, -2 and -3.
        assert_eq!(kern.kerning(0), Some(-1));
        assert_eq!(kern.kerning(19), Some(-1));
        // A height equal to a correction height starts the next band.
        assert_eq!(kern.kerning(20), Some(-2));
        assert_eq!(kern.kerning(39), Some(-2));
        assert_eq!(kern.kerning(40), Some(-3));
        assert_eq!(kern.kerning(1000), Some(-3));
    }

    #[test]
    fn kern_entries_carry_their_own_adjustments() {
        let buf = math_table();
        let math = math(&buf);
        let kern = math
            .math_glyph_info()
            .unwrap()
            .math_kern_info()
            .unwrap()
            .unwrap()
            .kern(GlyphId::new(9), MathKernCorner::TopRight)
            .unwrap();

        // Three bands for two heights, the last one unbounded. The second
        // height and the second kern each carry a device table, and they are
        // adjusted independently.
        assert_eq!(
            kern.entries_for_ppem(10).collect::<Vec<_>>(),
            vec![
                entry(Some(plain(20)), plain(-1)),
                entry(Some(adjusted(40, 4)), adjusted(-2, 1)),
                entry(None, plain(-3)),
            ]
        );
        assert_eq!(
            kern.entries_for_ppem(11).collect::<Vec<_>>(),
            vec![
                entry(Some(plain(20)), plain(-1)),
                entry(Some(adjusted(40, -6)), adjusted(-2, 2)),
                entry(None, plain(-3)),
            ]
        );
        // Outside the sizes those devices cover, nothing is adjusted.
        assert_eq!(
            kern.entries_for_ppem(12).collect::<Vec<_>>(),
            vec![
                entry(Some(plain(20)), plain(-1)),
                entry(Some(plain(40)), plain(-2)),
                entry(None, plain(-3)),
            ]
        );

        // `kerning` compares stored heights, so it is unaffected by them.
        assert_eq!(kern.kerning(40), Some(-3));
    }

    #[test]
    fn variants_and_assembly() {
        let buf = math_table();
        let math = math(&buf);
        let variants = math.math_variants().unwrap();

        assert_eq!(variants.min_connector_overlap(), UfWord::new(6));

        let construction = variants
            .glyph_construction(GlyphId::new(9), StretchAxis::Vertical)
            .unwrap();
        assert_eq!(
            construction
                .math_glyph_variant_records()
                .iter()
                .map(|r| (r.variant_glyph(), r.advance_measurement()))
                .collect::<Vec<_>>(),
            vec![
                (GlyphId16::new(21), UfWord::new(300)),
                (GlyphId16::new(22), UfWord::new(400)),
            ]
        );

        // Nothing stretches horizontally in this font.
        assert!(variants
            .glyph_construction(GlyphId::new(9), StretchAxis::Horizontal)
            .is_none());

        let assembly = construction.glyph_assembly().unwrap().unwrap();
        assert_eq!(assembly.italics_correction().value(), FWord::new(17));
        // Its italics correction has no device table.
        assert_eq!(assembly.italics_correction_for_ppem(12), plain(17));

        let part = &assembly.part_records()[0];
        assert_eq!(part.glyph_id(), GlyphId16::new(23));
        assert_eq!(part.start_connector_length(), UfWord::new(10));
        assert_eq!(part.end_connector_length(), UfWord::new(11));
        assert_eq!(part.full_advance(), UfWord::new(50));
        assert!(part.part_flags().contains(PartFlags::EXTENDER_FLAG));
    }
}
