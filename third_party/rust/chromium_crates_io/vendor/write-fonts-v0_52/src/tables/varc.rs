//! The [VARC (Variable Composites/Components)](https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md) table

use std::collections::BTreeMap;

pub use super::layout::{Condition, CoverageTable};
use crate::ps::cff::v2::Index as Index2;
use crate::tables::variations::{
    common_builder::{TemporaryDeltaSetId, NO_VARIATION_INDEX},
    mivs_builder::{MultiItemVariationStoreBuilder, MultiVariationIndexRemapping},
    PackedDeltas,
};

include!("../../generated/generated_varc.rs");

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VarcVariationIndex {
    PendingVariationIndex(TemporaryDeltaSetId),
    VariationIndex(u32),
}

impl VarcVariationIndex {
    pub fn to_u32(&self, remappings: &MultiVariationIndexRemapping) -> u32 {
        match self {
            VarcVariationIndex::PendingVariationIndex(temp_id) => {
                remappings.get(*temp_id).unwrap_or(NO_VARIATION_INDEX)
            }
            VarcVariationIndex::VariationIndex(idx) => *idx,
        }
    }
}

impl Varc {
    pub fn new_from_composite_glyphs(
        coverage: CoverageTable,
        store_builder: MultiItemVariationStoreBuilder,
        conditions: Vec<Condition>,
        composites: Vec<VarCompositeGlyph>,
    ) -> Self {
        let axis_indices_list = Varc::create_axis_indices_list(&composites);
        let axis_indices_list_items = axis_indices_list
            .iter()
            .map(|axes_indices| {
                let mut writer = TableWriter::default();
                let packed = PackedDeltas::new(axes_indices.iter().map(|v| *v as i32).collect());
                packed.write_into(&mut writer);
                writer.into_data().bytes
            })
            .collect();
        let (store, remappings) = store_builder.build();

        let axis_indices_list_raw = Index2::from_items(axis_indices_list_items);
        let var_composite_glyphs = Index2::from_items(
            composites
                .iter()
                .map(|x| x.to_bytes(&axis_indices_list, &remappings))
                .collect(),
        );
        let condition_list: NullableOffsetMarker<ConditionList, WIDTH_32> =
            NullableOffsetMarker::new(if conditions.is_empty() {
                None
            } else {
                Some(ConditionList::new(conditions.len() as u32, conditions))
            });
        let multi_var_store: NullableOffsetMarker<MultiItemVariationStore, WIDTH_32> =
            NullableOffsetMarker::new(if store.variation_data_count == 0 {
                None
            } else {
                Some(store)
            });
        Self {
            coverage: coverage.into(),
            multi_var_store,
            condition_list,
            axis_indices_list: axis_indices_list_raw.into(),
            var_composite_glyphs: var_composite_glyphs.into(),
        }
    }

    fn create_axis_indices_list(composites: &Vec<VarCompositeGlyph>) -> Vec<Vec<u16>> {
        let mut axis_indices_counter: BTreeMap<Vec<u16>, u32> = BTreeMap::new();

        for composite in composites {
            for component in &composite.0 {
                if let Some(axis_values) = &component.axis_values {
                    let axis_indices: Vec<u16> = axis_values.keys().cloned().collect();
                    *axis_indices_counter.entry(axis_indices).or_insert(0) += 1;
                }
            }
        }
        // Order by most used to least used
        let mut axis_indices_list: Vec<(Vec<u16>, u32)> = axis_indices_counter
            .into_iter()
            .collect::<Vec<(Vec<u16>, u32)>>();
        axis_indices_list.sort_by_key(|(_, count)| std::cmp::Reverse(*count));

        axis_indices_list
            .into_iter()
            .map(|(indices, _)| indices)
            .collect()
    }
}

pub struct VarCompositeGlyph(pub Vec<VarComponent>);
impl VarCompositeGlyph {
    fn to_bytes(
        &self,
        axis_indices_list: &[Vec<u16>],
        remappings: &MultiVariationIndexRemapping,
    ) -> Vec<u8> {
        let mut writer = TableWriter::default();
        for component in &self.0 {
            let raw_component = RawVarComponent {
                flags: if component.reset_unspecified_axes {
                    VarcFlags::RESET_UNSPECIFIED_AXES
                } else {
                    VarcFlags::empty()
                },
                gid: component.gid,
                condition_index: component.condition_index.map(|ci| ci.to_u32(remappings)),
                axis_indices_index: component.axis_values.as_ref().and_then(|axis_values| {
                    let axis_indices: Vec<u16> = axis_values.keys().cloned().collect();
                    axis_indices_list
                        .iter()
                        .position(|indices| *indices == axis_indices)
                        .map(|idx| idx as u32)
                }),
                axis_values: component
                    .axis_values
                    .as_ref()
                    .map(|axis_values| axis_values.values().cloned().collect::<Vec<f32>>()),
                axis_values_var_index: component
                    .axis_values_var_index
                    .map(|avi| avi.to_u32(remappings)),
                transform_var_index: component
                    .transform_var_index
                    .map(|tvi| tvi.to_u32(remappings)),
                transform: component.transform.clone(),
            };
            raw_component.write_into(&mut writer);
        }
        writer.into_data().bytes
    }
}

pub struct VarComponent {
    pub reset_unspecified_axes: bool,
    pub gid: GlyphId,
    pub condition_index: Option<VarcVariationIndex>,
    pub axis_values: Option<BTreeMap<u16, f32>>,
    pub axis_values_var_index: Option<VarcVariationIndex>,
    pub transform: DecomposedTransform,
    pub transform_var_index: Option<VarcVariationIndex>,
}

struct RawVarComponent {
    flags: VarcFlags,
    gid: GlyphId,
    condition_index: Option<u32>,
    axis_indices_index: Option<u32>,
    axis_values: Option<Vec<f32>>,
    axis_values_var_index: Option<u32>,
    transform_var_index: Option<u32>,
    transform: DecomposedTransform,
}

impl RawVarComponent {
    fn determine_flags(&self) -> Uint32Var {
        let mut flags = self.flags;

        if self.gid.to_u32() > 0xFFFF {
            flags.insert(VarcFlags::GID_IS_24BIT);
        }

        if self.condition_index.is_some() {
            flags.insert(VarcFlags::HAVE_CONDITION);
        }

        if self.axis_indices_index.is_some() {
            flags.insert(VarcFlags::HAVE_AXES);
        }

        if self.axis_values_var_index.is_some() {
            flags.insert(VarcFlags::AXIS_VALUES_HAVE_VARIATION);
        }

        if self.transform_var_index.is_some() {
            flags.insert(VarcFlags::TRANSFORM_HAS_VARIATION);
        }
        flags |= self.transform.flags();

        // Set the reserved bits to zero
        flags.remove(VarcFlags::RESERVED_MASK);

        Uint32Var(flags.bits())
    }
}

impl FontWrite for RawVarComponent {
    // Hand-roll this for now
    fn write_into(&self, writer: &mut TableWriter) {
        self.determine_flags().write_into(writer);
        if self.gid.to_u32() > 0xFFFF {
            Uint24::new(self.gid.to_u32()).write_into(writer);
        } else {
            (self.gid.to_u32() as u16).write_into(writer);
        }
        if let Some(condition_index) = self.condition_index {
            Uint32Var(condition_index).write_into(writer);
        }
        if let Some(axis_indices_index) = self.axis_indices_index {
            Uint32Var(axis_indices_index).write_into(writer);
        }
        if let Some(axis_values) = &self.axis_values {
            let converted_axis_values = axis_values
                .iter()
                .map(|v| F2Dot14::from_f32(*v).to_bits() as i32)
                .collect();
            let packed = PackedDeltas::new(converted_axis_values);
            packed.write_into(writer);
        }
        if let Some(axis_values_var_index) = self.axis_values_var_index {
            Uint32Var(axis_values_var_index).write_into(writer);
        }

        if let Some(transform_var_index) = self.transform_var_index {
            Uint32Var(transform_var_index).write_into(writer);
        }

        self.transform.write_into(writer);

        // Technically we are supposed to process and discard one uint32var
        // per each set bit in RESERVED_MASK. But we explicitly set bits in
        // RESERVED_MASK to zero, as per the spec. So we just do nothing here.
    }
}

/// A variable Uint32
///
/// See <https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md#uint32var>.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Uint32Var(u32);

impl FontWrite for Uint32Var {
    fn write_into(&self, writer: &mut TableWriter) {
        let value = self.0;
        if value < 0x80 {
            (value as u8).write_into(writer);
        } else if value < 0x4000 {
            let byte1 = ((value >> 8) as u8) | 0x80;
            let byte2 = (value & 0xFF) as u8;
            (byte1).write_into(writer);
            (byte2).write_into(writer);
        } else if value < 0x200000 {
            let byte1 = ((value >> 16) as u8) | 0xC0;
            let byte2 = ((value >> 8) & 0xFF) as u8;
            let byte3 = (value & 0xFF) as u8;
            (byte1).write_into(writer);
            (byte2).write_into(writer);
            (byte3).write_into(writer);
        } else if value < 0x10000000 {
            let byte1 = ((value >> 24) as u8) | 0xE0;
            let byte2 = ((value >> 16) & 0xFF) as u8;
            let byte3 = ((value >> 8) & 0xFF) as u8;
            let byte4 = (value & 0xFF) as u8;
            (byte1).write_into(writer);
            (byte2).write_into(writer);
            (byte3).write_into(writer);
            (byte4).write_into(writer);
        } else {
            (0xF0u8).write_into(writer);
            (((value >> 24) & 0xFF) as u8).write_into(writer);
            (((value >> 16) & 0xFF) as u8).write_into(writer);
            (((value >> 8) & 0xFF) as u8).write_into(writer);
            ((value & 0xFF) as u8).write_into(writer);
        }
    }
}

/// <https://github.com/fonttools/fonttools/blob/5e6b12d12fa08abafbeb7570f47707fbedf69a45/Lib/fontTools/misc/transform.py#L410>
#[derive(Debug, Clone, Default, PartialEq)]
pub struct DecomposedTransform {
    pub translate_x: Option<f64>,
    pub translate_y: Option<f64>,
    pub rotation: Option<f64>, // degrees, counter-clockwise
    pub scale_x: Option<f64>,
    pub scale_y: Option<f64>,
    pub skew_x: Option<f64>,
    pub skew_y: Option<f64>,
    pub center_x: Option<f64>,
    pub center_y: Option<f64>,
}

impl DecomposedTransform {
    fn flags(&self) -> VarcFlags {
        let mut flags = VarcFlags::empty();
        if self.translate_x.is_some() {
            flags.insert(VarcFlags::HAVE_TRANSLATE_X);
        }
        if self.translate_y.is_some() {
            flags.insert(VarcFlags::HAVE_TRANSLATE_Y);
        }
        if self.rotation.is_some() {
            flags.insert(VarcFlags::HAVE_ROTATION);
        }
        if self.scale_x.is_some() {
            flags.insert(VarcFlags::HAVE_SCALE_X);
        }
        if self.scale_y.is_some() {
            flags.insert(VarcFlags::HAVE_SCALE_Y);
        }
        if self.skew_x.is_some() {
            flags.insert(VarcFlags::HAVE_SKEW_X);
        }
        if self.skew_y.is_some() {
            flags.insert(VarcFlags::HAVE_SKEW_Y);
        }
        if self.center_x.is_some() {
            flags.insert(VarcFlags::HAVE_TCENTER_X);
        }
        if self.center_y.is_some() {
            flags.insert(VarcFlags::HAVE_TCENTER_Y);
        }
        flags
    }
}

impl FontWrite for DecomposedTransform {
    fn write_into(&self, writer: &mut TableWriter) {
        if let Some(translate_x) = self.translate_x {
            FWord::from(translate_x as i16).write_into(writer);
        }
        if let Some(translate_y) = self.translate_y {
            FWord::from(translate_y as i16).write_into(writer);
        }
        if let Some(rotation) = self.rotation {
            F4Dot12::from_f32(rotation as f32).write_into(writer);
        }
        if let Some(scale_x) = self.scale_x {
            F6Dot10::from_f32(scale_x as f32).write_into(writer);
        }
        if let Some(scale_y) = self.scale_y {
            F6Dot10::from_f32(scale_y as f32).write_into(writer);
        }
        if let Some(skew_x) = self.skew_x {
            F4Dot12::from_f32(skew_x as f32).write_into(writer);
        }
        if let Some(skew_y) = self.skew_y {
            F4Dot12::from_f32(skew_y as f32).write_into(writer);
        }
        if let Some(center_x) = self.center_x {
            FWord::from(center_x as i16).write_into(writer);
        }
        if let Some(center_y) = self.center_y {
            FWord::from(center_y as i16).write_into(writer);
        }
    }
}
#[cfg(test)]
mod tests {
    use crate::{
        dump_table,
        tables::{layout::CoverageFormat1, variations::mivs_builder::SparseRegion},
        write::TableWriter,
    };

    use super::*;

    #[test]
    fn test_write_uint32var() {
        let mut writer = TableWriter::default();
        Uint32Var(0x7F).write_into(&mut writer);
        assert_eq!(writer.into_data().bytes, vec![0x7F]);

        let mut writer = TableWriter::default();
        Uint32Var(0x3FFF).write_into(&mut writer);
        assert_eq!(writer.into_data().bytes, vec![0xBF, 0xFF]);

        let mut writer = TableWriter::default();
        Uint32Var(0x1FFFFF).write_into(&mut writer);
        assert_eq!(writer.into_data().bytes, vec![0xDF, 0xFF, 0xFF]);

        let mut writer = TableWriter::default();
        Uint32Var(0x0FFFFFFF).write_into(&mut writer);
        assert_eq!(writer.into_data().bytes, vec![0xEF, 0xFF, 0xFF, 0xFF]);

        let mut writer = TableWriter::default();
        Uint32Var(0xB2D05E00).write_into(&mut writer);
        assert_eq!(writer.into_data().bytes, vec![0xF0, 0xB2, 0xD0, 0x5E, 0x00]);
    }

    #[test]

    fn test_basic_varc() {
        let storebuilder = MultiItemVariationStoreBuilder::new();
        let component = VarComponent {
            reset_unspecified_axes: false,
            gid: GlyphId::new(42),
            condition_index: None,
            axis_values: None,
            axis_values_var_index: None,
            transform: DecomposedTransform {
                translate_x: None,
                translate_y: None,
                ..Default::default()
            },
            transform_var_index: None,
        };
        let composite = VarCompositeGlyph(vec![component]);
        let varc = Varc::new_from_composite_glyphs(
            CoverageTable::Format1(CoverageFormat1::new(vec![1.into()])),
            storebuilder,
            vec![],
            vec![composite],
        );
        varc.validate().expect("Varc validation failed");
        let bytes = dump_table(&varc).expect("Failed to dump varc table");

        let varc_roundtrip = read_fonts::tables::varc::Varc::read(FontData::new(&bytes))
            .expect("Failed to read varc table");
        let glyphs: Vec<GlyphId16> = varc_roundtrip.coverage().unwrap().iter().collect();
        assert_eq!(glyphs, vec![GlyphId16::new(1)]);
        assert!(varc_roundtrip.multi_var_store().is_none());
        assert!(varc_roundtrip.condition_list().is_none());
        let composite = varc_roundtrip.glyph(0).unwrap();
        assert_eq!(composite.components().count(), 1);
        let mut components = composite.components();
        let component = components.next().unwrap().unwrap();
        assert_eq!(component.gid(), GlyphId16::new(42));
        assert_eq!(component.condition_index(), None);
        assert_eq!(component.axis_indices_index(), None);
        assert!(component.axis_values().is_none());
        assert_eq!(component.axis_values_var_index(), None);
        assert_eq!(component.transform_var_index(), None);
    }

    // Let's do one with some axis values
    #[test]
    fn test_varc_with_axis_values_and_transform() {
        let storebuilder = MultiItemVariationStoreBuilder::new();
        let component = VarComponent {
            reset_unspecified_axes: true,
            gid: GlyphId::new(100),
            condition_index: None,
            axis_values: Some(
                // Magic numbers chosen to be exactly representable in F2Dot14
                vec![(0u16, 0.2199707f32), (1u16, 0.2999878f32)]
                    .into_iter()
                    .collect::<BTreeMap<u16, f32>>(),
            ),
            axis_values_var_index: None,
            transform: DecomposedTransform {
                translate_x: Some(10.0),
                translate_y: Some(-10.0),
                ..Default::default()
            },
            transform_var_index: None,
        };
        let composite = VarCompositeGlyph(vec![component]);
        let varc = Varc::new_from_composite_glyphs(
            CoverageTable::Format1(CoverageFormat1::new(vec![2.into()])),
            storebuilder,
            vec![],
            vec![composite],
        );
        varc.validate().expect("Varc validation failed");
        let bytes = dump_table(&varc).expect("Failed to dump varc table");
        let varc_roundtrip = read_fonts::tables::varc::Varc::read(FontData::new(&bytes))
            .expect("Failed to read varc table");
        let glyphs: Vec<GlyphId16> = varc_roundtrip.coverage().unwrap().iter().collect();
        assert_eq!(glyphs, vec![GlyphId16::new(2)]);
        assert!(varc_roundtrip.multi_var_store().is_none());
        assert!(varc_roundtrip.condition_list().is_none());
        let axis_indices_list = varc_roundtrip.axis_indices_list().unwrap().unwrap();
        assert_eq!(axis_indices_list.count(), 1);
        assert_eq!(
            varc_roundtrip
                .axis_indices(0)
                .unwrap()
                .iter()
                .collect::<Vec<_>>(),
            vec![0, 1],
        );
        let composite = varc_roundtrip.glyph(0).unwrap();
        assert_eq!(composite.components().count(), 1);
        let mut components = composite.components();
        let component = components.next().unwrap().unwrap();
        assert!(component
            .flags()
            .contains(VarcFlags::RESET_UNSPECIFIED_AXES));
        assert!(component.flags().contains(VarcFlags::HAVE_AXES));
        assert_eq!(component.gid(), GlyphId16::new(100));
        assert_eq!(component.condition_index(), None);
        assert_eq!(component.axis_indices_index(), Some(0));
        let axis_values = component.axis_values().unwrap();
        let axis_values_vec: Vec<f32> = axis_values
            .iter()
            .map(|b| F2Dot14::from_bits(b as i16).to_f32())
            .collect();
        assert_eq!(axis_values_vec, vec![0.2199707, 0.2999878]);
        assert_eq!(component.axis_values_var_index(), None);
        assert_eq!(component.transform_var_index(), None);
        let matrix = component.transform().matrix();
        assert_eq!(matrix.dx, 10.0); // translate x
        assert_eq!(matrix.dy, -10.0); // translate y
    }

    // And let's do one with a var store
    #[test]
    fn test_varc_with_var_store() {
        let mut storebuilder = MultiItemVariationStoreBuilder::new();
        let region1 = SparseRegion::new(vec![(
            0,
            F2Dot14::from_f32(0.0),
            F2Dot14::from_f32(1.0),
            F2Dot14::from_f32(1.0),
        )]);
        let region2 = SparseRegion::new(vec![(
            1,
            F2Dot14::from_f32(-1.0),
            F2Dot14::from_f32(-1.0),
            F2Dot14::from_f32(0.0),
        )]);

        let delta_set_id = storebuilder
            .add_deltas(vec![
                // weight. Increase translate_x by 500 at region1 peak
                (region1, vec![500, 0]),
                // width. Increate translate_y by 500 at region2 peak
                (region2, vec![0, 500]),
            ])
            .unwrap();
        let component = VarComponent {
            reset_unspecified_axes: true,
            gid: GlyphId::new(150),
            condition_index: None,
            axis_values: None,
            axis_values_var_index: None,
            transform: DecomposedTransform {
                translate_x: Some(0.0),
                translate_y: Some(0.0),
                ..Default::default()
            },
            transform_var_index: Some(VarcVariationIndex::PendingVariationIndex(delta_set_id)),
        };
        let composite = VarCompositeGlyph(vec![component]);
        let varc = Varc::new_from_composite_glyphs(
            CoverageTable::Format1(CoverageFormat1::new(vec![3.into()])),
            storebuilder,
            vec![],
            vec![composite],
        );
        varc.validate().expect("Varc validation failed");
        let bytes = dump_table(&varc).expect("Failed to dump varc table");

        let varc_roundtrip = read_fonts::tables::varc::Varc::read(FontData::new(&bytes))
            .expect("Failed to read varc table");
        let glyphs: Vec<GlyphId16> = varc_roundtrip.coverage().unwrap().iter().collect();
        assert_eq!(glyphs, vec![GlyphId16::new(3)]);

        // Verify the multi var store exists and has expected structure
        let multi_var_store = varc_roundtrip.multi_var_store().unwrap().unwrap();
        assert_eq!(multi_var_store.format(), 1);
        let region_list = multi_var_store.region_list().unwrap();
        assert_eq!(region_list.region_count(), 2);

        let region_0 = region_list.regions().get(0).unwrap();
        assert_eq!(region_0.region_axis_count(), 1);
        let region_1 = region_list.regions().get(1).unwrap();
        assert_eq!(region_1.region_axis_count(), 1);

        // Verify the component
        let composite = varc_roundtrip.glyph(0).unwrap();
        assert_eq!(composite.components().count(), 1);
        let mut components = composite.components();
        let component = components.next().unwrap().unwrap();
        assert!(component
            .flags()
            .contains(VarcFlags::RESET_UNSPECIFIED_AXES));
        assert_eq!(component.gid(), GlyphId16::new(150));
        assert_eq!(component.condition_index(), None);
        assert_eq!(component.axis_indices_index(), None);
        assert!(component.axis_values().is_none());
        assert_eq!(component.axis_values_var_index(), None);
        assert_eq!(component.transform_var_index(), Some(0));

        // Verify the transform base values
        let matrix = component.transform().matrix();
        assert_eq!(matrix.dx, 0.0); // translate x
        assert_eq!(matrix.dy, 0.0); // translate y
    }
}
