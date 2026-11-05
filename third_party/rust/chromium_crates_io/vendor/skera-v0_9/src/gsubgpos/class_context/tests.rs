use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::{types::GlyphId24, FontData, FontRead};

fn class_context(chain: bool, primary_class: u32, records: &[(u16, u16)]) -> Vec<u8> {
    let count = primary_class + 1;
    let header_size = if chain { 20 } else { 13 };
    let coverage_offset = header_size + count * 3;
    let input_class_offset = coverage_offset + 8;
    let input_class = BeBuffer::new()
        .push(3u16)
        .push(GlyphId24::new(65536))
        .push(Uint24::new(4465))
        .extend((0..4465).map(|i| {
            Uint24::new(match i {
                0 => primary_class,
                4464 => 65535,
                _ => 0,
            })
        }));
    let backtrack_class_offset = input_class_offset + input_class.len() as u32;
    let lookahead_class_offset = backtrack_class_offset + 11;
    let set_offset = if chain {
        lookahead_class_offset + 11
    } else {
        backtrack_class_offset
    };
    let mut bytes = BeBuffer::new().push(5u16).push(coverage_offset);
    if chain {
        bytes = bytes
            .push(backtrack_class_offset)
            .push(input_class_offset)
            .push(lookahead_class_offset)
            .push(count as u16);
    } else {
        bytes = bytes.push(input_class_offset).push(Uint24::new(count));
    }
    bytes = bytes
        .extend((0..count).map(|i| Uint24::new(if i == primary_class { set_offset } else { 0 })))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(65536))
        .extend(input_class.iter().copied());
    if chain {
        bytes = bytes
            .push(3u16)
            .push(GlyphId24::new(65535))
            .push(Uint24::new(1))
            .push(Uint24::new(3))
            .push(3u16)
            .push(GlyphId24::new(0xffffff))
            .push(Uint24::new(1))
            .push(Uint24::new(4));
    }
    bytes = bytes.push(1u16).push(Uint24::new(5));
    if chain {
        bytes = bytes
            .push(1u16)
            .push(3u16)
            .push(2u16)
            .push(65535u16)
            .push(1u16)
            .push(4u16)
            .push(records.len() as u16);
    } else {
        bytes = bytes.push(2u16).push(records.len() as u16).push(65535u16);
    }
    for &(sequence, lookup) in records {
        bytes = bytes.push(sequence).push(lookup);
    }
    bytes.to_vec()
}

fn plan(mapped: [u32; 4]) -> Plan {
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 0x1000000],
        ..Default::default()
    };
    for (old, new) in [65535, 65536, 70000, 0xffffff].into_iter().zip(mapped) {
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
    }
    plan.gsub_lookups.insert(7, 2);
    plan
}

fn subset(chain: bool, bytes: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
    let mut s = Serializer::new(2 * 1024 * 1024);
    s.start_serialize().unwrap();
    let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
    let state = SubsetState::default();
    let args = (&state, &font, &plan.gsub_lookups);
    if chain {
        ChainedSequenceContext::read(FontData::new(bytes))
            .unwrap()
            .subset(plan, &mut s, args)?;
    } else {
        SequenceContext::read(FontData::new(bytes))
            .unwrap()
            .subset(plan, &mut s, args)?;
    }
    s.end_serialize();
    assert!(!s.in_error());
    Ok(s.copy_bytes())
}

#[test]
fn class_context_subset_preserves_format_and_remaps_classes_and_actions() {
    for (chain, primary_class) in [(false, 65536), (true, 32768), (false, 0), (true, 0)] {
        let input = class_context(chain, primary_class, &[(0, 7), (1, 8), (1, 7)]);
        for mapped in [[1, 2, 3, 4], [65535, 65536, 70000, 0xffffff]] {
            let bytes = subset(chain, &input, &plan(mapped)).unwrap();
            let (class_def, count, rule) = if chain {
                let table = ChainedSequenceContextFormat5::read(FontData::new(&bytes)).unwrap();
                assert_eq!(table.format(), 5);
                assert!(table
                    .coverage()
                    .unwrap()
                    .get(GlyphId::new(mapped[1]))
                    .is_some());
                let class_def = table.input_class_def().unwrap();
                let primary = class_def.get(GlyphId::new(mapped[1]));
                let set = table
                    .chained_class_seq_rule_sets()
                    .get(primary as usize)
                    .unwrap()
                    .unwrap();
                assert_eq!(set.chained_class_seq_rule_count(), 1);
                let rule = set.chained_class_seq_rules().get(0).unwrap();
                assert_eq!(
                    rule.backtrack_sequence()[0].get() as u32,
                    table
                        .backtrack_class_def()
                        .unwrap()
                        .get(GlyphId::new(mapped[0]))
                );
                assert_eq!(
                    rule.lookahead_sequence()[0].get() as u32,
                    table
                        .lookahead_class_def()
                        .unwrap()
                        .get(GlyphId::new(mapped[3]))
                );
                assert_eq!(
                    rule.seq_lookup_records()
                        .iter()
                        .map(|r| (r.sequence_index(), r.lookup_list_index()))
                        .collect::<Vec<_>>(),
                    [(0, 2), (1, 2)]
                );
                (
                    class_def,
                    table.chained_class_seq_rule_set_count() as u32,
                    rule.input_sequence()[0].get(),
                )
            } else {
                let table = SequenceContextFormat5::read(FontData::new(&bytes)).unwrap();
                assert_eq!(table.format(), 5);
                assert!(table
                    .coverage()
                    .unwrap()
                    .get(GlyphId::new(mapped[1]))
                    .is_some());
                let class_def = table.class_def().unwrap();
                let primary = class_def.get(GlyphId::new(mapped[1]));
                let set = table
                    .class_seq_rule_sets()
                    .get(primary as usize)
                    .unwrap()
                    .unwrap();
                assert_eq!(set.class_seq_rule_count(), 1);
                let rule = set.class_seq_rules().get(0).unwrap();
                assert_eq!(
                    rule.seq_lookup_records()
                        .iter()
                        .map(|r| (r.sequence_index(), r.lookup_list_index()))
                        .collect::<Vec<_>>(),
                    [(0, 2), (1, 2)]
                );
                (
                    class_def,
                    table.class_seq_rule_set_count().to_u32(),
                    rule.input_sequence()[0].get(),
                )
            };
            assert_eq!(count, class_def.get(GlyphId::new(mapped[1])) + 1);
            assert_eq!(rule as u32, class_def.get(GlyphId::new(mapped[2])));
        }
    }
}

#[test]
fn class_context_subset_prunes_missing_classes_and_handles_bad_offsets() {
    let mut plan = plan([1, 2, 3, 4]);
    for (chain, primary_class) in [(false, 65536), (true, 32768)] {
        let input = class_context(chain, primary_class, &[(1, 7)]);
        let header = if chain { 20 } else { 13 };
        let set_offset = header
            + (primary_class as usize + 1) * 3
            + 8
            + 8
            + 4465 * 3
            + if chain { 22 } else { 0 };
        let mut ranges = vec![
            2..6,
            if chain { 10..14 } else { 6..10 },
            header + primary_class as usize * 3..header + primary_class as usize * 3 + 3,
            set_offset + 2..set_offset + 5,
        ];
        if chain {
            ranges.extend([6..10, 14..18]);
        }
        for range in ranges {
            let mut bytes = input.clone();
            bytes[range.clone()].fill(0);
            assert_eq!(
                subset(chain, &bytes, &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
            bytes[range].fill(0xff);
            assert_eq!(
                subset(chain, &bytes, &plan),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
        }
        assert_eq!(
            subset(chain, &input[..input.len() - 1], &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
        plan.glyphset_gsub.remove(GlyphId::new(70000));
        plan.glyph_map_gsub[70000] = crate::INVALID_GID;
        assert_eq!(
            subset(chain, &input, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
        plan.glyphset_gsub.insert(GlyphId::new(70000));
        plan.glyph_map_gsub[70000] = GlyphId::new(3);
    }
}

#[test]
fn class_context_subset_keeps_full_width_rule_set_counts() {
    let count = 65537u32;
    let coverage_offset = 13 + count * 3;
    let class_def_offset = coverage_offset + 8;
    let set_offset = class_def_offset + 8 + count * 3;
    let bytes = BeBuffer::new()
        .push(5u16)
        .push(coverage_offset)
        .push(class_def_offset)
        .push(Uint24::new(count))
        .extend((0..count).map(|c| Uint24::new(if c == count - 1 { set_offset } else { 0 })))
        .push(3u16)
        .push(Uint24::new(1))
        .push(GlyphId24::new(131072))
        .push(3u16)
        .push(GlyphId24::new(65536))
        .push(Uint24::new(count))
        .extend((0..count).map(Uint24::new))
        .push(1u16)
        .push(Uint24::new(5))
        .push(1u16)
        .push(1u16)
        .push(0u16)
        .push(7u16)
        .to_vec();
    let mut plan = Plan {
        glyph_map_gsub: vec![crate::INVALID_GID; 131073],
        ..Default::default()
    };
    for class in 0..count {
        let old = class + 65536;
        plan.glyphset_gsub.insert(GlyphId::new(old));
        plan.glyph_map_gsub[old as usize] = GlyphId::new(class);
    }
    plan.gsub_lookups.insert(7, 2);
    let bytes = subset(false, &bytes, &plan).unwrap();
    let table = SequenceContextFormat5::read(FontData::new(&bytes)).unwrap();
    assert_eq!(table.class_seq_rule_set_count().to_u32(), count);
    assert_eq!(table.class_def().unwrap().get(GlyphId::new(65536)), 65536);
    assert!(table.coverage_offset().to_u32() > u16::MAX as u32);
    assert!(
        table.class_seq_rule_set_offsets()[65536]
            .get()
            .offset()
            .to_u32()
            > u16::MAX as u32
    );
    assert!(table.class_seq_rule_set_offsets()[..65536]
        .iter()
        .all(|o| o.get().is_null()));
}

#[test]
fn class_context_subset_accepts_wide_rule_offsets_and_empty_chained_context() {
    for (chain, primary_class) in [(false, 65536), (true, 32768)] {
        let mut bytes = class_context(chain, primary_class, &[(1, 7)]);
        let set_offset = if chain {
            ChainedSequenceContextFormat5::read(FontData::new(&bytes))
                .unwrap()
                .chained_class_seq_rule_set_offsets()[primary_class as usize]
                .get()
                .offset()
                .to_u32()
        } else {
            SequenceContextFormat5::read(FontData::new(&bytes))
                .unwrap()
                .class_seq_rule_set_offsets()[primary_class as usize]
                .get()
                .offset()
                .to_u32()
        } as usize;
        let rule = bytes[set_offset + 5..].to_vec();
        bytes.truncate(set_offset + 5);
        bytes[set_offset + 2..set_offset + 5].copy_from_slice(&Uint24::new(70000).to_raw());
        bytes.resize(set_offset + 70000, 0);
        bytes.extend(rule);
        assert!(subset(chain, &bytes, &plan([1, 2, 3, 4])).is_ok());
        if chain {
            bytes[6..10].fill(0);
            bytes[14..18].fill(0);
            bytes.truncate(set_offset + 70000);
            bytes.extend(
                BeBuffer::new()
                    .push(0u16)
                    .push(2u16)
                    .push(65535u16)
                    .push(0u16)
                    .push(1u16)
                    .push(1u16)
                    .push(7u16)
                    .to_vec(),
            );
            let output = subset(true, &bytes, &plan([1, 2, 3, 4])).unwrap();
            let table = ChainedSequenceContextFormat5::read(FontData::new(&output)).unwrap();
            assert!(table.backtrack_class_def_offset().is_null());
            assert!(table.lookahead_class_def_offset().is_null());
        }
    }
}
