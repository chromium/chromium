//! Class definition remapping and format selection.

use super::*;

impl<'a> SubsetTable<'a> for ClassDef<'a> {
    type ArgsForSubset = &'a ClassDefSubsetStruct<'a>;
    type Output = Option<ClassMap>;

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let truncated = match self {
            Self::Format1(t) => t.class_value_array().len() < t.glyph_count() as usize,
            Self::Format3(t) => t.class_value_array().len() < t.glyph_count().to_u32() as usize,
            _ => false,
        };
        if truncated {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        let glyph_set = &plan.glyphset_gsub;
        let mut retained_classes = IntSet::empty();
        let mut pairs = Vec::with_capacity((glyph_set.len() as usize).min(self.population()));
        let mut retain = |gid, class| {
            if class == 0
                || args
                    .glyph_filter
                    .is_some_and(|filter| filter.get(gid).is_none())
            {
                return;
            }
            if let Some(mapped) = map_gsub_glyph(&plan.glyph_map_gsub, gid) {
                retained_classes.insert(class);
                pairs.push((mapped.to_u32(), class));
            }
        };
        if self.population() as u64 > glyph_set.len() * self.cost() as u64 {
            for gid in glyph_set.iter() {
                retain(gid, self.get(gid));
            }
        } else {
            for (gid, class) in self.iter() {
                retain(gid, class);
            }
        }
        pairs.sort_unstable_by_key(|&(gid, _)| gid);

        if !args.keep_empty_table && pairs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        if !args.remap_class {
            return ClassDef::serialize(s, &pairs).map(|()| None);
        }
        let glyph_count = if let Some(filter) = args.glyph_filter {
            glyph_set
                .iter()
                .filter(|&gid| filter.get(gid).is_some())
                .count()
        } else {
            glyph_set.len() as usize
        };
        let use_class_zero = args.use_class_zero && glyph_count <= pairs.len();
        // one entry per retained class, plus class 0
        let mut class_map = ClassMap::with_capacity_and_hasher(
            retained_classes.len() as usize + 1,
            foldhash::fast::FixedState::default(),
        );
        if !use_class_zero {
            class_map.insert(0, 0);
        }
        for (next_class, class) in (u32::from(!use_class_zero)..).zip(retained_classes.iter()) {
            Uint24::checked_new(next_class)
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            class_map.insert(class, next_class);
        }
        for (_, class) in &mut pairs {
            *class = class_map[class];
        }
        ClassDef::serialize(s, &pairs).map(|()| Some(class_map))
    }
}

macro_rules! subset_classdef {
    ($table:ident, $format:ident) => {
        impl<'a> SubsetTable<'a> for $table<'a> {
            type ArgsForSubset = &'a ClassDefSubsetStruct<'a>;
            type Output = Option<ClassMap>;
            fn subset(
                &self,
                plan: &Plan,
                s: &mut Serializer,
                args: Self::ArgsForSubset,
            ) -> Result<Self::Output, SerializeErrorFlags> {
                ClassDef::$format(self.clone()).subset(plan, s, args)
            }
        }
    };
}
subset_classdef!(ClassDefFormat1, Format1);
subset_classdef!(ClassDefFormat2, Format2);
subset_classdef!(ClassDefFormat3, Format3);
subset_classdef!(ClassDefFormat4, Format4);

impl<'a> Serialize<'a> for ClassDef<'a> {
    type Args = &'a [(u32, u32)];
    fn serialize(s: &mut Serializer, pairs: Self::Args) -> Result<(), SerializeErrorFlags> {
        for &(gid, class) in pairs.iter().filter(|&&(_, class)| class != 0) {
            if Uint24::checked_new(gid).is_none() || Uint24::checked_new(class).is_none() {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW));
            }
        }
        let mut nonzero = pairs.iter().copied().filter(|&(_, class)| class != 0);
        let Some((first, first_class)) = nonzero.next() else {
            return ClassDefFormat2::serialize(s, pairs);
        };
        let mut last = first;
        let mut previous_class = first_class;
        let mut max_class = first_class;
        let mut ranges = 1usize;
        for (gid, class) in nonzero {
            if last.checked_add(1) != Some(gid) || previous_class != class {
                ranges += 1;
            }
            last = gid;
            previous_class = class;
            max_class = max_class.max(class);
        }
        let span = last - first + 1;
        if last <= u16::MAX as u32 && max_class <= u16::MAX as u32 {
            // Preserve the existing array-versus-range choice when both fit.
            if span <= u16::MAX as u32 && (span as usize) < ranges * 3 {
                return ClassDefFormat1::serialize(s, pairs);
            }
            if ranges <= u16::MAX as usize {
                return ClassDefFormat2::serialize(s, pairs);
            }
        }
        // Format 4 widens glyph IDs and the range count, but not class values.
        if max_class > u16::MAX as u32
            || (Uint24::checked_new(span).is_some() && u64::from(span) * 3 < ranges as u64 * 8)
        {
            ClassDefFormat3::serialize(s, pairs)
        } else {
            ClassDefFormat4::serialize(s, pairs)
        }
    }
}

fn embed_u16(s: &mut Serializer, value: u32) -> Result<(), SerializeErrorFlags> {
    let value = u16::try_from(value)
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
    s.embed(value).map(|_| ())
}

fn embed_u24(s: &mut Serializer, value: u32) -> Result<(), SerializeErrorFlags> {
    let value = Uint24::checked_new(value)
        .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
    s.embed(value).map(|_| ())
}

fn serialize_array(
    s: &mut Serializer,
    pairs: &[(u32, u32)],
    wide: bool,
) -> Result<(), SerializeErrorFlags> {
    let mut nonzero = pairs.iter().copied().filter(|&(_, class)| class != 0);
    let (first, _) = nonzero.next().unwrap_or_default();
    let last = nonzero.next_back().map(|(gid, _)| gid).unwrap_or(first);
    let count = if pairs.iter().any(|&(_, class)| class != 0) {
        last - first + 1
    } else {
        0
    };
    let embed = if wide { embed_u24 } else { embed_u16 };
    if (wide && Uint24::checked_new(last).is_none()) || (!wide && u16::try_from(last).is_err()) {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW));
    }
    s.embed(if wide { 3u16 } else { 1u16 })?;
    embed(s, first)?;
    embed(s, count)?;
    let width = if wide { 3 } else { 2 };
    let pos = s.allocate_size(count as usize * width, true)?;
    for &(gid, class) in pairs.iter().filter(|&&(_, class)| class != 0) {
        let index = (gid - first) as usize;
        if wide {
            let class = Uint24::checked_new(class)
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.copy_assign(pos + index * width, class);
        } else {
            let class = u16::try_from(class)
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.copy_assign(pos + index * width, class);
        }
    }
    Ok(())
}

fn serialize_ranges(
    s: &mut Serializer,
    pairs: &[(u32, u32)],
    wide: bool,
) -> Result<(), SerializeErrorFlags> {
    s.embed(if wide { 4u16 } else { 2u16 })?;
    let count_pos = s.allocate_size(if wide { 3 } else { 2 }, true)?;
    let mut count = 0u32;
    let mut range: Option<(u32, u32, u32)> = None;
    let mut emit_range = |first, last, class| {
        let embed = if wide { embed_u24 } else { embed_u16 };
        embed(s, first)?;
        embed(s, last)?;
        embed_u16(s, class)?;
        count += 1;
        Ok::<_, SerializeErrorFlags>(())
    };
    for &(gid, class) in pairs.iter().filter(|&&(_, class)| class != 0) {
        if let Some((first, last, previous_class)) = range {
            if last.checked_add(1) == Some(gid) && class == previous_class {
                range = Some((first, gid, class));
                continue;
            }
            emit_range(first, last, previous_class)?;
        }
        range = Some((gid, gid, class));
    }
    if let Some((first, last, class)) = range {
        emit_range(first, last, class)?;
    }
    if wide {
        let count = Uint24::checked_new(count)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(count_pos, count);
    } else {
        let count = u16::try_from(count)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(count_pos, count);
    }
    Ok(())
}

macro_rules! serialize_classdef {
    ($table:ident, $serialize:ident, $wide:literal) => {
        impl<'a> Serialize<'a> for $table<'a> {
            type Args = &'a [(u32, u32)];
            fn serialize(s: &mut Serializer, pairs: Self::Args) -> Result<(), SerializeErrorFlags> {
                $serialize(s, pairs, $wide)
            }
        }
    };
}
serialize_classdef!(ClassDefFormat1, serialize_array, false);
serialize_classdef!(ClassDefFormat2, serialize_ranges, false);
serialize_classdef!(ClassDefFormat3, serialize_array, true);
serialize_classdef!(ClassDefFormat4, serialize_ranges, true);

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;

    fn serialize(pairs: &[(u32, u32)]) -> Result<Vec<u8>, SerializeErrorFlags> {
        let mut s = Serializer::new(pairs.len() * 8 + 32);
        s.start_serialize().unwrap();
        ClassDef::serialize(&mut s, pairs)?;
        s.end_serialize();
        assert!(!s.in_error());
        Ok(s.copy_bytes())
    }

    fn plan(glyphs: &[(u32, u32)]) -> Plan {
        let len = glyphs
            .iter()
            .map(|&(gid, _)| gid as usize + 1)
            .max()
            .unwrap_or(0);
        let mut plan = Plan {
            glyph_map_gsub: vec![INVALID_GID; len],
            ..Default::default()
        };
        for &(old, new) in glyphs {
            plan.glyphset_gsub.insert(GlyphId::new(old));
            plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
        }
        plan
    }

    fn subset(
        input: &[u8],
        plan: &Plan,
        args: &ClassDefSubsetStruct,
    ) -> Result<(Vec<u8>, Option<ClassMap>), SerializeErrorFlags> {
        let table = ClassDef::read(FontData::new(input)).unwrap();
        let mut s = Serializer::new(plan.glyphset_gsub.len() as usize * 8 + 32);
        s.start_serialize().unwrap();
        let class_map = table.subset(plan, &mut s, args)?;
        s.end_serialize();
        assert!(!s.in_error());
        Ok((s.copy_bytes(), class_map))
    }

    fn args() -> ClassDefSubsetStruct<'static> {
        ClassDefSubsetStruct {
            remap_class: true,
            keep_empty_table: true,
            use_class_zero: false,
            glyph_filter: None,
        }
    }

    fn array_input() -> Vec<u8> {
        BeBuffer::new()
            .push(3u16)
            .push(GlyphId24::new(65534))
            .push(Uint24::new(4))
            .extend([0, 65536, 0xffffff, 2].map(Uint24::new))
            .to_vec()
    }

    #[test]
    fn classdef_selects_narrow_and_wide_array_and_range_formats() {
        for (pairs, format) in [
            (vec![], 2),
            (vec![(65535, 1)], 1),
            ((65532..=65535).map(|gid| (gid, 1)).collect(), 2),
            (vec![(65536, 1)], 3),
            ((65534..=65537).map(|gid| (gid, 1)).collect(), 4),
            (vec![(1, 65536), (2, 0xffffff)], 3),
            ((0..=65535).map(|gid| (gid, 1)).collect(), 2),
            ((0..=65535).map(|gid| (gid, gid % 2 + 1)).collect(), 3),
            ((0..65537).map(|gid| (gid * 3, 42)).collect(), 4),
        ] {
            let bytes = serialize(&pairs).unwrap();
            let table = ClassDef::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.class_format(), format);
            let nonzero: Vec<_> = table
                .iter()
                .filter(|&(_, class)| class != 0)
                .map(|(gid, class)| (gid.to_u32(), class))
                .collect();
            assert_eq!(nonzero, pairs);
            for (gid, class) in pairs {
                assert_eq!(table.get(GlyphId::new(gid)), class);
            }
            if let ClassDef::Format4(table) = table {
                if table.class_range_count().to_u32() == 65537 {
                    assert_eq!(table.class_range_records()[65536].class(), 42);
                }
            }
        }
    }

    #[test]
    fn extended_classdef_subsetting_remaps_full_width_glyphs_and_classes() {
        let array = array_input();
        let ranges = BeBuffer::new()
            .push(4u16)
            .push(Uint24::new(2))
            .push(GlyphId24::new(65535))
            .push(GlyphId24::new(65536))
            .push(42u16)
            .push(GlyphId24::new(65537))
            .push(GlyphId24::new(65537))
            .push(65535u16)
            .to_vec();
        for (bytes, expected, original) in [
            (&array, [2, 3, 1], [65536, 0xffffff, 2]),
            (&ranges, [1, 1, 2], [42, 42, 65535]),
        ] {
            for mapped in [[1, 2, 3], [65536, 65537, 70000]] {
                let plan = plan(
                    &[65535, 65536, 65537]
                        .into_iter()
                        .zip(mapped)
                        .collect::<Vec<_>>(),
                );
                let (bytes, class_map) = subset(bytes, &plan, &args()).unwrap();
                let table = ClassDef::read(FontData::new(&bytes)).unwrap();
                let class_map = class_map.unwrap();
                assert_eq!(class_map.get(&0), Some(&0));
                for ((gid, expected), original) in mapped.into_iter().zip(expected).zip(original) {
                    assert_eq!(table.get(GlyphId::new(gid)), expected);
                    assert_eq!(class_map.get(&original), Some(&expected));
                }
                assert_eq!(table.class_format(), if mapped[2] <= 65535 { 1 } else { 4 });
            }
        }
        let (bytes, class_map) = subset(
            &array,
            &plan(&[(65535, 1), (65536, 2)]),
            &ClassDefSubsetStruct {
                remap_class: false,
                ..args()
            },
        )
        .unwrap();
        assert!(class_map.is_none());
        let table = ClassDef::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.class_format(), 3);
        assert_eq!(table.get(GlyphId::new(1)), 65536);
        assert_eq!(table.get(GlyphId::new(2)), 0xffffff);
    }

    #[test]
    fn classdef_subsetting_handles_class_zero_filtering_and_empty_sets() {
        let coverage_bytes = BeBuffer::new()
            .push(3u16)
            .push(Uint24::new(2))
            .extend([65535, 65537].map(GlyphId24::new))
            .to_vec();
        let coverage = CoverageTable::read(FontData::new(&coverage_bytes)).unwrap();
        let plan = plan(&[(65534, 0), (65535, 1), (65536, 2), (65537, 3)]);
        let filtered_args = ClassDefSubsetStruct {
            use_class_zero: true,
            glyph_filter: Some(&coverage),
            ..args()
        };
        let (bytes, class_map) = subset(&array_input(), &plan, &filtered_args).unwrap();
        let table = ClassDef::read(FontData::new(&bytes)).unwrap();
        let class_map = class_map.unwrap();
        assert_eq!(class_map.len(), 2);
        assert_eq!(class_map.get(&2), Some(&0));
        assert_eq!(class_map.get(&65536), Some(&1));
        assert_eq!(table.get(GlyphId::new(1)), 1);
        assert_eq!(table.get(GlyphId::new(2)), 0);
        assert_eq!(table.get(GlyphId::new(3)), 0);

        let (_, class_map) = subset(
            &array_input(),
            &plan,
            &ClassDefSubsetStruct {
                use_class_zero: true,
                ..args()
            },
        )
        .unwrap();
        assert_eq!(class_map.unwrap().get(&0), Some(&0));
        let (bytes, _) = subset(&array_input(), &Plan::default(), &args()).unwrap();
        assert_eq!(bytes, [0, 2, 0, 0]);
        assert_eq!(
            subset(
                &array_input(),
                &Plan::default(),
                &ClassDefSubsetStruct {
                    keep_empty_table: false,
                    ..args()
                }
            ),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
    }

    #[test]
    fn classdef_subsetting_preserves_counts_and_classes_above_65535() {
        let input = BeBuffer::new()
            .push(3u16)
            .push(GlyphId24::new(0))
            .push(Uint24::new(65537))
            .extend((1..=65537).map(Uint24::new))
            .to_vec();
        let plan = plan(&(0..65537).map(|gid| (gid, gid)).collect::<Vec<_>>());
        let (bytes, class_map) = subset(&input, &plan, &args()).unwrap();
        let table = ClassDefFormat3::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.glyph_count().to_u32(), 65537);
        assert_eq!(table.class_value_array()[65536].get().to_u32(), 65537);
        let class_map = class_map.unwrap();
        assert_eq!(class_map.len(), 65538);
        assert_eq!(class_map.get(&65537), Some(&65537));
    }

    #[test]
    fn legacy_classdef_subsetting_does_not_alias_wide_glyphs() {
        let input = BeBuffer::new()
            .push(2u16)
            .push(3u16)
            .extend([1u16, 1, 1, 2, 2, 1, 3, 3, 1])
            .to_vec();
        let (bytes, class_map) = subset(&input, &plan(&[(65537, 0)]), &args()).unwrap();
        assert_eq!(bytes, [0, 2, 0, 0]);
        assert_eq!(class_map.unwrap().len(), 1);
        for input in [
            BeBuffer::new()
                .push(1u16)
                .push(1u16)
                .push(1u16)
                .push(2u16)
                .to_vec(),
            BeBuffer::new()
                .push(2u16)
                .push(1u16)
                .extend([1u16, 1, 2])
                .to_vec(),
        ] {
            let (bytes, _) = subset(&input, &plan(&[(1, 65536)]), &args()).unwrap();
            let table = ClassDef::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.get(GlyphId::new(65536)), 1);
            assert_eq!(table.get(GlyphId::new(0)), 0);
        }
    }

    #[test]
    fn classdef_rejects_overflow_and_truncated_arrays() {
        for pairs in [[(0x1000000, 1)], [(1, 0x1000000)]] {
            assert_eq!(
                serialize(&pairs),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
            );
        }
        assert_eq!(
            subset(
                &array_input(),
                &plan(&[(65534, 1), (65535, 0x1000000)]),
                &args()
            ),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );
        for bytes in [
            BeBuffer::new()
                .push(1u16)
                .push(1u16)
                .push(2u16)
                .push(1u16)
                .to_vec(),
            BeBuffer::new()
                .push(3u16)
                .push(GlyphId24::new(1))
                .push(Uint24::new(2))
                .push(Uint24::new(1))
                .to_vec(),
        ] {
            assert_eq!(
                subset(&bytes, &plan(&[(1, 1)]), &args()),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
        }
    }
}
