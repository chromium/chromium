use super::*;
use crate::{subset_font, IntSet, SubsetFlags};
use read_fonts::{FontRef, TableProvider};

fn tags(vertical: bool, wide: bool) -> (Tag, Tag) {
    match (vertical, wide) {
        (false, false) => (Tag::new(b"hmtx"), Tag::new(b"hhea")),
        (false, true) => (Tag::new(b"HMTX"), Tag::new(b"HHEA")),
        (true, false) => (Tag::new(b"vmtx"), Tag::new(b"vhea")),
        (true, true) => (Tag::new(b"VMTX"), Tag::new(b"VHEA")),
    }
}

fn advance(gid: usize, count: usize) -> u16 {
    if gid >= count - 10 {
        111
    } else {
        100 + (gid % 1000) as u16
    }
}

fn bearing(gid: usize) -> i16 {
    (gid % 50) as i16 - 25
}

fn source(vertical: bool, wide: bool, truncated: bool, hybrid: bool) -> Vec<u8> {
    let count = if wide { 70002 } else { 25 };
    let num_long = count - 9;
    let mut metrics = Vec::new();
    for gid in 0..count {
        if gid < num_long {
            metrics.extend(advance(gid, count).to_be_bytes());
        }
        metrics.extend(bearing(gid).to_be_bytes());
    }
    if truncated {
        metrics.truncate(metrics.len() - 2);
    }
    let mut header = vec![0u8; if wide { 38 } else { 36 }];
    header[..4].copy_from_slice(&0x10000u32.to_be_bytes());
    header[4..6].copy_from_slice(&123i16.to_be_bytes());
    if wide {
        header[34..38].copy_from_slice(&(num_long as u32).to_be_bytes());
    } else {
        header[34..36].copy_from_slice(&(num_long as u16).to_be_bytes());
    }
    let mut builder = FontBuilder::new();
    let (metric_tag, header_tag) = tags(vertical, wide);
    builder.add_raw(metric_tag, metrics);
    builder.add_raw(header_tag, header);
    let mut maxp = 0x5000u32.to_be_bytes().to_vec();
    maxp.extend(read_fonts::types::Uint24::new(count as u32).to_be_bytes());
    builder.add_raw(Tag::new(b"MAXP"), maxp);
    if hybrid {
        let (metric_tag, header_tag) = tags(vertical, false);
        builder.add_raw(metric_tag, &[0u8][..]);
        builder.add_raw(header_tag, &[0u8][..]);
    }
    builder.build()
}

fn plan(font: &FontRef, gids: &IntSet<GlyphId>, retain: bool) -> Plan {
    Plan::new(
        gids,
        &Default::default(),
        font,
        if retain {
            SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS
        } else {
            SubsetFlags::SUBSET_FLAGS_DEFAULT
        },
        &Default::default(),
        &Default::default(),
        &Default::default(),
        &Default::default(),
        &Default::default(),
    )
}

fn records<'a>(font: &FontRef<'a>, vertical: bool) -> GlyphMetricRecords<'a> {
    if vertical {
        font.v_glyph_metric_records().unwrap()
    } else {
        font.h_glyph_metric_records().unwrap()
    }
}

#[test]
fn metric_subsets_preserve_wide_counts_and_tail_compression() {
    for vertical in [false, true] {
        for wide in [false, true] {
            let bytes = source(vertical, wide, false, wide);
            let font = FontRef::new(&bytes).unwrap();
            let count = font.selected_maxp().unwrap().num_glyphs() as usize;
            let gids = (0..count as u32).map(GlyphId::new).collect();
            let bytes = subset_font(&font, &plan(&font, &gids, false)).unwrap();
            let font = FontRef::new(&bytes).unwrap();
            let metrics = records(&font, vertical);
            assert_eq!(metrics.long_metrics().len(), count - 9);
            assert_eq!(metrics.side_bearings().len(), 9);
            for gid in 0..count {
                assert_eq!(
                    metrics.advance(GlyphId::new(gid as u32)),
                    Some(advance(gid, count))
                );
                assert_eq!(
                    metrics.side_bearing(GlyphId::new(gid as u32)),
                    Some(bearing(gid))
                );
            }
            let (_, header_tag) = tags(vertical, wide);
            assert_eq!(
                &font.data_for_tag(header_tag).unwrap().as_bytes()[4..6],
                &123i16.to_be_bytes()
            );
            if wide {
                let (old_metric, old_header) = tags(vertical, false);
                assert!(font.data_for_tag(old_metric).is_none());
                assert!(font.data_for_tag(old_header).is_none());
            }
        }
    }
}

#[test]
fn metric_subsets_remap_high_glyphs_and_zero_sparse_gaps() {
    for vertical in [false, true] {
        for retain in [false, true] {
            let bytes = source(vertical, true, false, false);
            let font = FontRef::new(&bytes).unwrap();
            let gids = [GlyphId::new(70001)].into_iter().collect();
            let bytes = subset_font(&font, &plan(&font, &gids, retain)).unwrap();
            let font = FontRef::new(&bytes).unwrap();
            let metrics = records(&font, vertical);
            let gid = if retain { 70001 } else { 1 };
            assert_eq!(metrics.long_metrics().len(), gid + 1);
            assert_eq!(metrics.advance(GlyphId::new(gid as u32)), Some(111));
            assert_eq!(
                metrics.side_bearing(GlyphId::new(gid as u32)),
                Some(bearing(70001))
            );
            if retain {
                assert_eq!(metrics.advance(GlyphId::new(65536)), Some(0));
                assert_eq!(metrics.side_bearing(GlyphId::new(65536)), Some(0));
            }
        }
    }
}

#[test]
fn metric_subsets_reject_missing_source_side_bearings() {
    for vertical in [false, true] {
        for wide in [false, true] {
            let bytes = source(vertical, wide, true, false);
            let font = FontRef::new(&bytes).unwrap();
            let last = font.selected_maxp().unwrap().num_glyphs() - 1;
            let gids = [GlyphId::new(last)].into_iter().collect();
            assert!(
                matches!(subset_font(&font, &plan(&font, &gids, false)), Err(SubsetError::SubsetTableError(tag)) if tag == tags(vertical, wide).0)
            );
        }
    }
}

#[test]
fn dropped_extended_metrics_leave_the_original_tables_to_subset() {
    for vertical in [false, true] {
        let narrow_bytes = source(vertical, false, false, false);
        let wide_bytes = source(vertical, true, true, false);
        let narrow = FontRef::new(&narrow_bytes).unwrap();
        let wide = FontRef::new(&wide_bytes).unwrap();
        let (metric_tag, header_tag) = tags(vertical, false);
        let (wide_metric_tag, wide_header_tag) = tags(vertical, true);
        let mut builder = FontBuilder::new();
        for (font, tag) in [
            (&narrow, metric_tag),
            (&narrow, header_tag),
            (&narrow, Tag::new(b"MAXP")),
            (&wide, wide_metric_tag),
            (&wide, wide_header_tag),
        ] {
            let mut data = font.table_data(tag).unwrap().as_bytes().to_vec();
            // The dropped metrics are too short to read.
            if tag == wide_metric_tag {
                data.truncate(4);
            }
            builder.add_raw(tag, data);
        }
        let bytes = builder.build();
        let font = FontRef::new(&bytes).unwrap();
        let gids: IntSet<GlyphId> = (0..25u32).map(GlyphId::new).collect();
        let dropped: IntSet<Tag> = [wide_metric_tag, wide_header_tag].into_iter().collect();
        let plan = Plan::new(
            &gids,
            &Default::default(),
            &font,
            SubsetFlags::SUBSET_FLAGS_DEFAULT,
            &dropped,
            &Default::default(),
            &Default::default(),
            &Default::default(),
            &Default::default(),
        );
        let bytes = subset_font(&font, &plan).unwrap();
        let subset = FontRef::new(&bytes).unwrap();
        assert!(subset.table_data(wide_metric_tag).is_none());
        let records = records(&subset, vertical);
        assert_eq!(records.advance(GlyphId::new(3)), Some(advance(3, 25)));
    }
}
