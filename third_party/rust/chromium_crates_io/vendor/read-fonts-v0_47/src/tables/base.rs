//! The [BASE](https://learn.microsoft.com/en-us/typography/opentype/spec/base) table

use super::{
    layout::DeviceOrVariationIndex,
    variations::{DeltaSetIndex, ItemVariationStore},
};

include!("../../generated/generated_base.rs");

/// Which axis table a baseline is read from.
///
/// This is the direction the text runs in, not the direction of the baseline
/// itself: horizontal text takes its baselines from the horizontal axis.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum BaseAxis {
    /// Text that runs left to right or right to left.
    Horizontal,
    /// Text that runs top to bottom or bottom to top.
    Vertical,
}

/// A baseline coordinate read at a location in variation space and a size.
///
/// A coordinate carries at most one adjustment, and the two kinds land in
/// different units. A `VariationIndex` names a delta in design units, so it is
/// already folded into `value`. A `Device` table adjusts by whole pixels, and
/// scaling that into design units needs the units per em, which this table
/// does not know; a caller working in font units wants
/// `value + delta_px * upem / ppem`, and one working in pixels has the
/// adjustment already.
#[derive(Clone, Copy, PartialEq, Eq, Default, Debug)]
pub struct BaseValue {
    /// The coordinate in design units, with any variation delta applied.
    pub value: F48Dot16,
    /// The adjustment the coordinate's device table makes at the requested
    /// size, in pixels. Zero unless the coordinate names a `Device` table that
    /// covers that size.
    pub delta_px: i32,
}

/// The script a font falls back to when it describes no baselines for the one
/// asked for.
const DEFAULT_SCRIPT: Tag = Tag::new(b"DFLT");

/// The baseline tags the OpenType spec registers.
pub mod baseline_tags {
    use super::Tag;

    /// The baseline alphabetic scripts sit on, such as Latin, Cyrillic and
    /// Greek.
    pub const ROMAN: Tag = Tag::new(b"romn");
    /// The baseline scripts hang from, such as Devanagari.
    pub const HANGING: Tag = Tag::new(b"hang");
    /// The bottom or left edge of an ideographic character face.
    pub const IDEO_FACE_BOTTOM_OR_LEFT: Tag = Tag::new(b"icfb");
    /// The top or right edge of an ideographic character face.
    pub const IDEO_FACE_TOP_OR_RIGHT: Tag = Tag::new(b"icft");
    /// The centre of an ideographic character face.
    pub const IDEO_FACE_CENTRAL: Tag = Tag::new(b"Icfc");
    /// The bottom or left edge of an ideographic em box.
    pub const IDEO_EMBOX_BOTTOM_OR_LEFT: Tag = Tag::new(b"ideo");
    /// The top or right edge of an ideographic em box.
    pub const IDEO_EMBOX_TOP_OR_RIGHT: Tag = Tag::new(b"idtp");
    /// The centre of an ideographic em box.
    pub const IDEO_EMBOX_CENTRAL: Tag = Tag::new(b"Idce");
    /// The baseline mathematical characters are centred on.
    pub const MATH: Tag = Tag::new(b"math");
}

/// Scripts whose horizontal text hangs from a line above it.
///
/// Grouped by the Unicode version that added each, as HarfBuzz lists them.
const HANGING_SCRIPTS: &[Tag] = &[
    // Unicode 1.1
    Tag::new(b"Beng"),
    Tag::new(b"Deva"),
    Tag::new(b"Gujr"),
    Tag::new(b"Guru"),
    // Unicode 2.0
    Tag::new(b"Tibt"),
    // Unicode 4.0
    Tag::new(b"Limb"),
    // Unicode 4.1
    Tag::new(b"Sylo"),
    // Unicode 5.0
    Tag::new(b"Phag"),
    // Unicode 5.2
    Tag::new(b"Mtei"),
    // Unicode 6.1
    Tag::new(b"Shrd"),
    Tag::new(b"Takr"),
    // Unicode 7.0
    Tag::new(b"Modi"),
    Tag::new(b"Sidd"),
    Tag::new(b"Tirh"),
    // Unicode 9.0
    Tag::new(b"Marc"),
    Tag::new(b"Newa"),
    // Unicode 10.0
    Tag::new(b"Soyo"),
    Tag::new(b"Zanb"),
    // Unicode 11.0
    Tag::new(b"Dogr"),
    Tag::new(b"Gong"),
    // Unicode 12.0
    Tag::new(b"Nand"),
];

/// Scripts written as ideographs.
const IDEOGRAPHIC_SCRIPTS: &[Tag] = &[
    // Unicode 1.1
    Tag::new(b"Hang"),
    Tag::new(b"Hani"),
    Tag::new(b"Hira"),
    Tag::new(b"Kana"),
    // Unicode 3.0
    Tag::new(b"Bopo"),
    // Unicode 9.0
    Tag::new(b"Tang"),
    // Unicode 10.0
    Tag::new(b"Nshu"),
    // Unicode 13.0
    Tag::new(b"Kits"),
];

/// The baseline horizontal text in a script sits on.
///
/// Scripts that hang from a line above take [`HANGING`], ideographic scripts
/// take [`IDEO_FACE_BOTTOM_OR_LEFT`], and everything else, known or not, takes
/// [`ROMAN`].
///
/// `script` is a Unicode script code -- `Deva`, `Hani` -- and not an OpenType
/// script tag. The two are spelled differently: the script `Hang` is Hangul,
/// while the baseline tag `hang` is the hanging baseline.
///
/// Mirrors HarfBuzz's [`hb_ot_layout_get_horizontal_baseline_tag_for_script`].
///
/// [`hb_ot_layout_get_horizontal_baseline_tag_for_script`]: https://github.com/harfbuzz/harfbuzz/blob/92e67ef19f2d595b0fe81f05a80783a321bb918f/src/hb-ot-layout.cc#L2235
///
/// [`HANGING`]: baseline_tags::HANGING
/// [`IDEO_FACE_BOTTOM_OR_LEFT`]: baseline_tags::IDEO_FACE_BOTTOM_OR_LEFT
/// [`ROMAN`]: baseline_tags::ROMAN
pub fn horizontal_baseline_tag_for_script(script: Tag) -> Tag {
    if HANGING_SCRIPTS.contains(&script) {
        baseline_tags::HANGING
    } else if IDEOGRAPHIC_SCRIPTS.contains(&script) {
        baseline_tags::IDEO_FACE_BOTTOM_OR_LEFT
    } else {
        baseline_tags::ROMAN
    }
}

impl<'a> Base<'a> {
    /// The axis table for a writing direction, or `None` where the font
    /// describes no baselines for that direction.
    pub fn axis(&self, axis: BaseAxis) -> Option<Axis<'a>> {
        match axis {
            BaseAxis::Horizontal => self.horiz_axis(),
            BaseAxis::Vertical => self.vert_axis(),
        }?
        .ok()
    }
}

impl<'a> Axis<'a> {
    /// The baselines a script uses, falling back to the default script, or
    /// `None` where neither is described.
    pub fn base_script(&self, script_tag: Tag) -> Option<BaseScript<'a>> {
        let list = self.base_script_list().ok()?;
        let records = list.base_script_records();
        let record =
            find_script(records, script_tag).or_else(|| find_script(records, DEFAULT_SCRIPT))?;
        record.base_script(list.offset_data()).ok()
    }

    /// The coordinate of one baseline for a script, or `None` where the font
    /// does not place that baseline.
    ///
    /// The script falls back to the default script, as in
    /// [`base_script`][Self::base_script]. A language does not come into it:
    /// languages vary the minimum and maximum extents, not the baselines.
    pub fn baseline_coord(&self, baseline_tag: Tag, script_tag: Tag) -> Option<BaseCoord<'a>> {
        let values = self.base_script(script_tag)?.base_values()?.ok()?;
        let tags = self.base_tag_list()?.ok()?;
        // The spec has these in alphabetical order, and a font that does not
        // keep to it hides the baselines that are out of place.
        let index = tags
            .baseline_tags()
            .binary_search_by_key(&baseline_tag, |tag| tag.get())
            .ok()?;
        values.base_coords().get(index).ok()
    }
}

/// The record for a script, by tag.
fn find_script(records: &[BaseScriptRecord], tag: Tag) -> Option<&BaseScriptRecord> {
    let index = records
        .binary_search_by_key(&tag, |record| record.base_script_tag())
        .ok()?;
    records.get(index)
}

impl<'a> BaseCoord<'a> {
    /// The device or variation table that adjusts this coordinate.
    ///
    /// Only a format 3 coordinate has one.
    pub fn device(&self) -> Option<DeviceOrVariationIndex<'a>> {
        match self {
            BaseCoord::Format3(coord) => coord.device()?.ok(),
            _ => None,
        }
    }
}

/// A `BASE` table paired with a location in variation space.
///
/// The table carries its own item variation store, so an instance needs only
/// the coordinates to read at; without them it reads the values as the font
/// stores them.
#[derive(Clone)]
pub struct BaseInstance<'a> {
    base: Base<'a>,
    var_store: Option<ItemVariationStore<'a>>,
    coords: &'a [F2Dot14],
}

impl<'a> BaseInstance<'a> {
    /// Creates an instance that reads coordinates as the font stores them,
    /// with no deltas applied.
    pub fn new(base: Base<'a>) -> Self {
        Self::with_coords(base, &[])
    }

    /// Creates an instance that reads coordinates at `coords`.
    ///
    /// Deltas are applied only when the table has an item variation store and
    /// `coords` is not empty; without either this behaves as
    /// [`new`][Self::new].
    pub fn with_coords(base: Base<'a>, coords: &'a [F2Dot14]) -> Self {
        let var_store = base.item_var_store().and_then(|store| store.ok());
        Self {
            base,
            var_store,
            coords,
        }
    }

    /// The position of one baseline, in design units, or `None` where the font
    /// does not place it.
    ///
    /// A baseline is a position on the axis across the writing direction: a y
    /// coordinate for horizontal text, an x coordinate for vertical text.
    pub fn baseline(&self, baseline_tag: Tag, axis: BaseAxis, script_tag: Tag) -> Option<F48Dot16> {
        Some(
            self.baseline_for_ppem(baseline_tag, axis, script_tag, 0)?
                .value,
        )
    }

    /// The position of one baseline as read at a size, or `None` where the
    /// font does not place it.
    ///
    /// `ppem` is the size along the same axis the coordinate lies on, so it is
    /// the vertical size for horizontal text and the horizontal size for
    /// vertical text.
    pub fn baseline_for_ppem(
        &self,
        baseline_tag: Tag,
        axis: BaseAxis,
        script_tag: Tag,
        ppem: u16,
    ) -> Option<BaseValue> {
        let coord = self
            .base
            .axis(axis)?
            .baseline_coord(baseline_tag, script_tag)?;
        Some(self.coord_for_ppem(&coord, ppem))
    }

    /// A coordinate in design units, with any variation delta applied.
    pub fn coord(&self, coord: &BaseCoord) -> F48Dot16 {
        self.coord_for_ppem(coord, 0).value
    }

    /// A coordinate as read at a size.
    pub fn coord_for_ppem(&self, coord: &BaseCoord, ppem: u16) -> BaseValue {
        // A format 2 coordinate names a glyph and a contour point to take the
        // position from. Nothing reads them: HarfBuzz answers with the plain
        // coordinate, so the two formats behave alike.
        let mut value = F48Dot16::from_i32(coord.coordinate() as i32);
        let mut delta_px = 0;
        match coord.device() {
            Some(DeviceOrVariationIndex::Device(device)) => {
                delta_px = device.delta_for_ppem(ppem);
            }
            Some(DeviceOrVariationIndex::VariationIndex(index)) => {
                if let Some(delta) = self.delta(index.into()) {
                    value += delta;
                }
            }
            None => {}
        }
        BaseValue { value, delta_px }
    }

    fn delta(&self, index: DeltaSetIndex) -> Option<F48Dot16> {
        if self.coords.is_empty() {
            return None;
        }
        self.var_store.as_ref()?.compute_delta(index, self.coords)
    }
}

impl<'a> core::ops::Deref for BaseInstance<'a> {
    type Target = Base<'a>;

    fn deref(&self) -> &Self::Target {
        &self.base
    }
}

#[cfg(test)]
mod tests {
    use font_test_data::bebuffer::BeBuffer;
    use font_types::MajorMinor;

    use super::*;

    #[test]
    /// https://learn.microsoft.com/en-us/typography/opentype/spec/base#base-table-examples
    fn example_1() {
        let data = BeBuffer::new()
            .push(MajorMinor::VERSION_1_0)
            .push(8u16) // horizaxis offset
            .push(0x10c_u16) // verticalaxis
            // axis table
            .push(4u16) //basetaglist
            .push(0x12_u16) // basescript list
            // base tag list
            .push(3u16) // count
            .push(Tag::new(b"hang"))
            .push(Tag::new(b"ideo"))
            .push(Tag::new(b"romn"))
            // basescriptlist
            .push(4u16) // basescript count
            .push(Tag::new(b"cyrl"))
            .push(0x1a_u16)
            .push(Tag::new(b"devn"))
            .push(0x60_u16)
            .push(Tag::new(b"hani"))
            .push(0x8a_u16)
            .push(Tag::new(b"latn"))
            .push(0xb4_u16);

        let base = Base::read(data.data().into()).unwrap();
        assert_eq!(base.version(), MajorMinor::VERSION_1_0);
        let horiz = base.horiz_axis().unwrap().unwrap();
        let base_tag = horiz.base_tag_list().unwrap().unwrap();
        assert_eq!(
            base_tag.baseline_tags(),
            &[Tag::new(b"hang"), Tag::new(b"ideo"), Tag::new(b"romn")]
        );
        assert_eq!(base_tag.min_byte_range().end, 14);
        let base_script = horiz.base_script_list().unwrap();
        assert_eq!(
            base_script.base_script_records()[3].base_script_tag(),
            Tag::new(b"latn")
        );
    }

    const HANG: Tag = Tag::new(b"hang");
    const IDEO: Tag = Tag::new(b"ideo");
    const ROMN: Tag = Tag::new(b"romn");
    const LATN: Tag = Tag::new(b"latn");
    const HANI: Tag = Tag::new(b"hani");
    const ARAB: Tag = Tag::new(b"arab");

    /// The delta the test store holds for delta set (0, 0) at the far end of
    /// its one axis.
    const ROMN_DELTA: i32 = -40;

    /// A BASE table with a horizontal axis, three baselines, and two scripts
    /// plus a default.
    ///
    /// `latn` places its roman baseline with a `VariationIndex` and its
    /// hanging baseline with a `Device` table, so the two kinds of adjustment
    /// can be told apart.
    fn base_table() -> BeBuffer {
        let mut buf = BeBuffer::new()
            .push(MajorMinor::VERSION_1_1)
            .push_with_tag(0u16, "horiz_axis_offset")
            .push(0u16) // vertical axis: null
            .push_with_tag(0u32, "var_store_offset");

        // -- HorizAxis --
        let axis = buf.len();
        buf = buf
            .push_with_tag(0u16, "base_tag_list_offset")
            .push_with_tag(0u16, "base_script_list_offset");

        // BaseTagList, in the alphabetical order the spec calls for.
        let tag_list = buf.len();
        buf = buf.push(3u16).push(HANG).push(IDEO).push(ROMN);

        // BaseScriptList, also sorted by tag.
        let script_list = buf.len();
        buf = buf
            .push(3u16) // count
            .push(Tag::new(b"DFLT"))
            .push_with_tag(0u16, "dflt_script_offset")
            .push(HANI)
            .push_with_tag(0u16, "hani_script_offset")
            .push(LATN)
            .push_with_tag(0u16, "latn_script_offset");

        // BaseScript for latn.
        let latn_script = buf.len();
        buf = buf
            .push_with_tag(0u16, "latn_values_offset")
            .push(0u16) // default min max: null
            .push(0u16); // base lang sys count
        let latn_values = buf.len();
        buf = buf
            .push(2u16) // default baseline index: romn
            .push(3u16) // count, one per baseline tag
            .push_with_tag(0u16, "latn_hang_offset")
            .push_with_tag(0u16, "latn_ideo_offset")
            .push_with_tag(0u16, "latn_romn_offset");

        // BaseScript for hani, with no BaseValues at all.
        let hani_script = buf.len();
        buf = buf.push(0u16).push(0u16).push(0u16);

        // BaseScript for DFLT.
        let dflt_script = buf.len();
        buf = buf
            .push_with_tag(0u16, "dflt_values_offset")
            .push(0u16)
            .push(0u16);
        let dflt_values = buf.len();
        buf = buf
            .push(2u16)
            .push(3u16)
            .push_with_tag(0u16, "dflt_hang_offset")
            .push_with_tag(0u16, "dflt_ideo_offset")
            .push_with_tag(0u16, "dflt_romn_offset");

        // latn coordinates: hang has a device, romn has a variation index.
        let latn_hang = buf.len();
        buf = buf
            .push(3u16) // format 3
            .push(700i16)
            .push_with_tag(0u16, "latn_hang_device");
        let latn_ideo = buf.len();
        buf = buf.push(1u16).push(-120i16);
        let latn_romn = buf.len();
        buf = buf
            .push(3u16)
            .push(0i16)
            .push_with_tag(0u16, "latn_romn_varidx");

        // DFLT coordinates, all plain. Format 2 is here to show it reads the
        // same as format 1.
        let dflt_hang = buf.len();
        buf = buf.push(1u16).push(600i16);
        let dflt_ideo = buf.len();
        buf = buf.push(2u16).push(-100i16).push(9u16).push(3u16);
        let dflt_romn = buf.len();
        buf = buf.push(1u16).push(5i16);

        let latn_hang_device = buf.len();
        buf = buf
            .push(10u16) // start size
            .push(11u16) // end size
            .push(3u16) // delta format: 8 bit
            .push(0x02FEu16); // ppem 10 -> 2, ppem 11 -> -2
        let latn_romn_varidx = buf.len();
        buf = buf.push(0u16).push(0u16).push(0x8000u16);

        // -- ItemVariationStore --
        let var_store = buf.len();
        buf = buf
            .push(1u16) // format
            .push_with_tag(0u32, "region_list_offset")
            .push(1u16) // item variation data count
            .push_with_tag(0u32, "var_data_offset");
        let region_list = buf.len();
        buf = buf
            .push(1u16) // axis count
            .push(1u16) // region count
            .push(F2Dot14::from_f32(0.0))
            .push(F2Dot14::from_f32(1.0))
            .push(F2Dot14::from_f32(1.0));
        let var_data = buf.len();
        buf = buf
            .push(1u16) // item count
            .push(1u16) // word delta count
            .push(1u16) // region index count
            .push(0u16) // region indexes[0]
            .push(ROMN_DELTA as i16);

        buf.write_at("horiz_axis_offset", axis as u16);
        buf.write_at("var_store_offset", var_store as u32);
        buf.write_at("base_tag_list_offset", (tag_list - axis) as u16);
        buf.write_at("base_script_list_offset", (script_list - axis) as u16);
        buf.write_at("dflt_script_offset", (dflt_script - script_list) as u16);
        buf.write_at("hani_script_offset", (hani_script - script_list) as u16);
        buf.write_at("latn_script_offset", (latn_script - script_list) as u16);
        buf.write_at("latn_values_offset", (latn_values - latn_script) as u16);
        buf.write_at("dflt_values_offset", (dflt_values - dflt_script) as u16);
        buf.write_at("latn_hang_offset", (latn_hang - latn_values) as u16);
        buf.write_at("latn_ideo_offset", (latn_ideo - latn_values) as u16);
        buf.write_at("latn_romn_offset", (latn_romn - latn_values) as u16);
        buf.write_at("dflt_hang_offset", (dflt_hang - dflt_values) as u16);
        buf.write_at("dflt_ideo_offset", (dflt_ideo - dflt_values) as u16);
        buf.write_at("dflt_romn_offset", (dflt_romn - dflt_values) as u16);
        buf.write_at("latn_hang_device", (latn_hang_device - latn_hang) as u16);
        buf.write_at("latn_romn_varidx", (latn_romn_varidx - latn_romn) as u16);
        buf.write_at("region_list_offset", (region_list - var_store) as u32);
        buf.write_at("var_data_offset", (var_data - var_store) as u32);
        buf
    }

    fn base(buf: &BeBuffer) -> Base<'_> {
        Base::read(buf.data().into()).unwrap()
    }

    fn value(v: i32) -> F48Dot16 {
        F48Dot16::from_i32(v)
    }

    #[test]
    fn scripts_map_to_their_baseline() {
        use baseline_tags::{HANGING, IDEO_FACE_BOTTOM_OR_LEFT, ROMAN};

        assert_eq!(
            horizontal_baseline_tag_for_script(Tag::new(b"Deva")),
            HANGING
        );
        assert_eq!(
            horizontal_baseline_tag_for_script(Tag::new(b"Nand")),
            HANGING
        );
        assert_eq!(
            horizontal_baseline_tag_for_script(Tag::new(b"Hani")),
            IDEO_FACE_BOTTOM_OR_LEFT
        );
        assert_eq!(
            horizontal_baseline_tag_for_script(Tag::new(b"Kits")),
            IDEO_FACE_BOTTOM_OR_LEFT
        );
        assert_eq!(horizontal_baseline_tag_for_script(Tag::new(b"Latn")), ROMAN);
        assert_eq!(horizontal_baseline_tag_for_script(Tag::new(b"Arab")), ROMAN);
        // An unknown script is roman like any other.
        assert_eq!(horizontal_baseline_tag_for_script(Tag::new(b"Zzzz")), ROMAN);

        // The script Hang is Hangul, and takes the ideographic baseline. The
        // baseline tag hang is a different thing spelled the same way.
        assert_eq!(
            horizontal_baseline_tag_for_script(Tag::new(b"Hang")),
            IDEO_FACE_BOTTOM_OR_LEFT
        );
        assert_ne!(Tag::new(b"Hang"), HANGING);

        // The two lists are as long as HarfBuzz's, and do not overlap.
        assert_eq!(HANGING_SCRIPTS.len(), 21);
        assert_eq!(IDEOGRAPHIC_SCRIPTS.len(), 8);
        for script in HANGING_SCRIPTS {
            assert!(!IDEOGRAPHIC_SCRIPTS.contains(script), "{script} is in both");
        }
    }

    #[test]
    fn baselines_are_found_by_tag() {
        let buf = base_table();
        let instance = BaseInstance::new(base(&buf));

        assert_eq!(
            instance.baseline(HANG, BaseAxis::Horizontal, LATN),
            Some(value(700))
        );
        assert_eq!(
            instance.baseline(IDEO, BaseAxis::Horizontal, LATN),
            Some(value(-120))
        );
        assert_eq!(
            instance.baseline(ROMN, BaseAxis::Horizontal, LATN),
            Some(value(0))
        );

        // A tag the font does not place.
        assert_eq!(
            instance.baseline(Tag::new(b"math"), BaseAxis::Horizontal, LATN),
            None
        );
        // The font describes no vertical axis.
        assert_eq!(instance.baseline(ROMN, BaseAxis::Vertical, LATN), None);
    }

    #[test]
    fn an_unlisted_script_falls_back_to_the_default() {
        let buf = base_table();
        let instance = BaseInstance::new(base(&buf));

        // arab is not in the list, so it reads the DFLT baselines.
        assert_eq!(
            instance.baseline(HANG, BaseAxis::Horizontal, ARAB),
            Some(value(600))
        );
        // A format 2 coordinate reads as its plain coordinate.
        assert_eq!(
            instance.baseline(IDEO, BaseAxis::Horizontal, ARAB),
            Some(value(-100))
        );

        // hani is listed but places no baselines at all, and the fallback does
        // not apply once a script has been found.
        assert_eq!(instance.baseline(HANG, BaseAxis::Horizontal, HANI), None);
    }

    #[test]
    fn variation_deltas_come_from_the_tables_own_store() {
        let buf = base_table();
        let coords = [F2Dot14::from_f32(1.0)];
        let instance = BaseInstance::with_coords(base(&buf), &coords);

        // latn's roman baseline is stored at 0 with a VariationIndex.
        assert_eq!(
            instance.baseline(ROMN, BaseAxis::Horizontal, LATN),
            Some(value(ROMN_DELTA))
        );
        // Its neighbours have no variation index and are untouched.
        assert_eq!(
            instance.baseline(IDEO, BaseAxis::Horizontal, LATN),
            Some(value(-120))
        );

        // At the default location the store contributes nothing.
        let default = BaseInstance::new(base(&buf));
        assert_eq!(
            default.baseline(ROMN, BaseAxis::Horizontal, LATN),
            Some(value(0))
        );
    }

    #[test]
    fn device_tables_adjust_by_ppem() {
        let buf = base_table();
        let instance = BaseInstance::new(base(&buf));

        // latn's hanging baseline is stored at 700 with a device covering
        // 10 to 11.
        let at = |ppem| instance.baseline_for_ppem(HANG, BaseAxis::Horizontal, LATN, ppem);
        assert_eq!(
            at(10),
            Some(BaseValue {
                value: value(700),
                delta_px: 2
            })
        );
        assert_eq!(
            at(11),
            Some(BaseValue {
                value: value(700),
                delta_px: -2
            })
        );
        // Outside the sizes it covers.
        assert_eq!(
            at(12),
            Some(BaseValue {
                value: value(700),
                delta_px: 0
            })
        );

        // A variation index is not a device table, so it adjusts no pixels.
        let coords = [F2Dot14::from_f32(1.0)];
        let varying = BaseInstance::with_coords(base(&buf), &coords);
        assert_eq!(
            varying.baseline_for_ppem(ROMN, BaseAxis::Horizontal, LATN, 10),
            Some(BaseValue {
                value: value(ROMN_DELTA),
                delta_px: 0
            })
        );
    }
}
