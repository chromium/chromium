//! impl subset() for glyf and loca
use crate::{
    estimate_subset_table_size,
    serialize::Serializer,
    Plan, Subset,
    SubsetError::{self, SubsetTableError},
    SubsetFlags,
};
use write_fonts::{
    read::{
        tables::{
            glyf::{
                CompositeGlyph, CompositeGlyphFlags, Glyf,
                Glyph::{self, Composite, Simple},
                SimpleGlyph, SimpleGlyphFlags,
            },
            head::Head,
            loca::Loca,
        },
        types::GlyphId,
        FontRef, TableProvider, TopLevelTable,
    },
    FontBuilder,
};

// reference: subset() for glyf/loca/head in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/OT/glyf/glyf.hh#L77
impl Subset for Glyf<'_> {
    fn subset(
        &self,
        plan: &Plan,
        font: &FontRef,
        s: &mut Serializer,
        builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        let loca = font.loca(None).or(Err(SubsetTableError(Loca::TAG)))?;
        let head = font.head().or(Err(SubsetTableError(Head::TAG)))?;

        let num_output_glyphs = plan.num_output_glyphs;
        let mut subset_glyphs = Vec::with_capacity(num_output_glyphs);
        let mut max_offset: u32 = 0;

        for (new_gid, old_gid) in &plan.new_to_old_gid_list {
            match loca.get_glyf(*old_gid, self) {
                Ok(g) => {
                    if *old_gid == GlyphId::NOTDEF
                        && *new_gid == GlyphId::NOTDEF
                        && !plan
                            .subset_flags
                            .contains(SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE)
                    {
                        subset_glyphs.push(Vec::new());
                        continue;
                    }

                    let Some(glyph) = g else {
                        subset_glyphs.push(Vec::new());
                        continue;
                    };
                    let subset_glyph = subset_glyph(&glyph, plan);
                    let trimmed_len = subset_glyph.len();
                    max_offset += padded_size(trimmed_len) as u32;
                    subset_glyphs.push(subset_glyph);
                }
                _ => {
                    return Err(SubsetTableError(Glyf::TAG));
                }
            }
        }

        //TODO: support force_long_loca in the plan
        let loca_format: u8 = if max_offset < 0x1FFFF { 0 } else { 1 };
        let loca_out = write_glyf_loca(font, plan, s, loca_format, &subset_glyphs)?;

        let head_out = subset_head(&head, loca_format);

        builder.add_raw(Loca::TAG, loca_out);
        builder.add_raw(Head::TAG, head_out);
        Ok(())
    }
}

fn padded_size(len: usize) -> usize {
    len + len % 2
}

// glyf data is written into the serializer, returning loca data to be added by FontBuilder
fn write_glyf_loca(
    font: &FontRef,
    plan: &Plan,
    s: &mut Serializer,
    loca_format: u8,
    subset_glyphs: &[Vec<u8>],
) -> Result<Vec<u8>, SubsetError> {
    let loca_cap = estimate_subset_table_size(font, Loca::TAG, plan);
    let mut loca_out: Vec<u8> = Vec::with_capacity(loca_cap);

    if loca_format == 0 {
        loca_out.extend_from_slice(&0_u16.to_be_bytes());
    } else {
        loca_out.extend_from_slice(&0_u32.to_be_bytes());
    }

    let init_len = s.length();
    let mut last: u32 = 0;
    if loca_format == 0 {
        let mut offset: u32 = 0;
        let mut value = 0_u16.to_be_bytes();
        for ((new_gid, _), i) in plan.new_to_old_gid_list.iter().zip(0u16..) {
            let gid = new_gid.to_u32();

            while last < gid {
                loca_out.extend_from_slice(&value);
                last += 1;
            }
            let g = &subset_glyphs[i as usize];
            let padded_len = padded_size(g.len());
            offset += padded_len as u32;
            value = ((offset >> 1) as u16).to_be_bytes();
            loca_out.extend_from_slice(&value);
            s.embed_bytes(g)
                .map_err(|_| SubsetError::SubsetTableError(Glyf::TAG))?;
            if padded_len > g.len() {
                s.embed_bytes(&[0])
                    .map_err(|_| SubsetError::SubsetTableError(Glyf::TAG))?;
            }

            last += 1;
        }

        while last < plan.num_output_glyphs as u32 {
            loca_out.extend_from_slice(&value);
            last += 1;
        }
    } else {
        let mut offset: u32 = 0;
        let mut value = 0_u32.to_be_bytes();
        for ((new_gid, _), i) in plan.new_to_old_gid_list.iter().zip(0u16..) {
            let gid = new_gid.to_u32();

            while last < gid {
                loca_out.extend_from_slice(&value);
                last += 1;
            }
            let g = &subset_glyphs[i as usize];
            offset += g.len() as u32;
            value = offset.to_be_bytes();
            loca_out.extend_from_slice(&value);

            s.embed_bytes(g)
                .map_err(|_| SubsetError::SubsetTableError(Glyf::TAG))?;

            last += 1;
        }

        while last < plan.num_output_glyphs as u32 {
            loca_out.extend_from_slice(&value);
            last += 1;
        }
    }

    // As a special case when all glyph in the font are empty, add a zero byte to the table,
    // so that OTS doesn’t reject it, and to make the table work on Windows as well.
    // See https://github.com/khaledhosny/ots/issues/52
    if init_len == s.length() {
        s.embed_bytes(&[0])
            .map_err(|_| SubsetError::SubsetTableError(Glyf::TAG))?;
    }

    Ok(loca_out)
}

fn subset_glyph(glyph: &Glyph, plan: &Plan) -> Vec<u8> {
    //TODO: support set_overlaps_flag and drop_hints
    match glyph {
        Composite(comp_g) => subset_composite_glyph(comp_g, plan),
        Simple(simple_g) => subset_simple_glyph(simple_g, plan),
    }
}

// TODO: drop_hints and set_overlaps_flag
fn subset_simple_glyph(g: &SimpleGlyph, plan: &Plan) -> Vec<u8> {
    let mut out = Vec::with_capacity(g.offset_data().len());

    let Some(num_coords) = g.end_pts_of_contours().last() else {
        return out;
    };
    let num_coords = num_coords.get() + 1;
    let glyph_data = g.glyph_data();
    let i = trim_simple_glyph_padding(glyph_data, num_coords);
    if i == 0 {
        return out;
    }

    let glyph_bytes = g.offset_data().as_bytes();
    let header_len = 10 + 2 * g.end_pts_of_contours().len() + 2;
    let Some(header_slice) = glyph_bytes.get(0..header_len) else {
        return out;
    };
    out.extend_from_slice(header_slice);

    if plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
    {
        // drop hints: set instructionLength field to 0
        out[header_len - 2] = 0;
        out[header_len - 1] = 0;
    } else {
        let instruction_end = header_len + g.instruction_length() as usize;
        let Some(instruction_slice) = glyph_bytes.get(header_len..instruction_end) else {
            return Vec::new();
        };
        out.extend_from_slice(instruction_slice);
    }

    let Some(trimmed_slice) = glyph_data.get(0..i) else {
        return Vec::new();
    };
    let first_flag_index = out.len();
    out.extend_from_slice(trimmed_slice);
    if plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_SET_OVERLAPS_FLAG)
    {
        out[first_flag_index] |= SimpleGlyphFlags::OVERLAP_SIMPLE.bits();
    }
    out
}

fn subset_composite_glyph(g: &CompositeGlyph, plan: &Plan) -> Vec<u8> {
    let mut out = g.offset_data().as_bytes().to_owned();

    let mut more = true;
    let mut we_have_instructions = false;
    let mut i: usize = 10;
    let len: usize = out.len();

    while more {
        if i + 3 >= len {
            return Vec::new();
        }
        let flags = u16::from_be_bytes([out[i], out[i + 1]]);
        let mut flags = CompositeGlyphFlags::from_bits_truncate(flags);

        if flags.contains(CompositeGlyphFlags::WE_HAVE_INSTRUCTIONS) {
            we_have_instructions = true;
            if plan
                .subset_flags
                .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
            {
                flags.remove(CompositeGlyphFlags::WE_HAVE_INSTRUCTIONS);
                out.get_mut(i..i + 2)
                    .unwrap()
                    .copy_from_slice(&flags.bits().to_be_bytes());
            }
        }

        // only set overlaps flag on the first component
        if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_SET_OVERLAPS_FLAG)
            && i == 10
        {
            flags.insert(CompositeGlyphFlags::OVERLAP_COMPOUND);
            out.get_mut(i..i + 2)
                .unwrap()
                .copy_from_slice(&flags.bits().to_be_bytes());
        }

        let old_gid = u16::from_be_bytes([out[i + 2], out[i + 3]]);
        let Some(new_gid) = plan.glyph_map.get(&GlyphId::from(old_gid)) else {
            return Vec::new();
        };
        let new_gid = new_gid.to_u32() as u16;
        out[i + 2] = (new_gid >> 8) as u8;
        out[i + 3] = (new_gid & 0xFF) as u8;

        i += 4;

        if flags.contains(CompositeGlyphFlags::ARG_1_AND_2_ARE_WORDS) {
            i += 4;
        } else {
            i += 2;
        }

        if flags.contains(CompositeGlyphFlags::WE_HAVE_A_SCALE) {
            i += 2;
        } else if flags.contains(CompositeGlyphFlags::WE_HAVE_AN_X_AND_Y_SCALE) {
            i += 4;
        } else if flags.contains(CompositeGlyphFlags::WE_HAVE_A_TWO_BY_TWO) {
            i += 8;
        }

        more = flags.contains(CompositeGlyphFlags::MORE_COMPONENTS);
    }

    if we_have_instructions
        && !plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
    {
        if i + 1 >= len {
            return Vec::new();
        }
        let instruction_len = u16::from_be_bytes([out[i], out[i + 1]]);
        i += 2 + instruction_len as usize;
    }

    out.truncate(i);
    out
}

// trim padding bytes for simple glyphs, return trimmed length of the raw data for flags & x/y coordinates
fn trim_simple_glyph_padding(glyph_data: &[u8], num_coords: u16) -> usize {
    let mut coord_bytes: usize = 0;
    let mut coords_with_flags: u16 = 0;
    let length = glyph_data.len();
    let mut i: usize = 0;
    while i < length {
        let flag = SimpleGlyphFlags::from_bits_truncate(glyph_data[i]);
        i += 1;

        let mut repeat: u8 = 1;
        if flag.contains(SimpleGlyphFlags::REPEAT_FLAG) {
            if i >= length {
                return 0;
            }
            repeat = glyph_data[i] + 1;
            i += 1;
        }

        let mut x_bytes: u8 = 0;
        let mut y_bytes: u8 = 0;
        if flag.contains(SimpleGlyphFlags::X_SHORT_VECTOR) {
            x_bytes = 1;
        } else if !flag.contains(SimpleGlyphFlags::X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) {
            x_bytes = 2;
        }

        if flag.contains(SimpleGlyphFlags::Y_SHORT_VECTOR) {
            y_bytes = 1;
        } else if !flag.contains(SimpleGlyphFlags::Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) {
            y_bytes = 2;
        }

        coord_bytes += ((x_bytes + y_bytes) * repeat) as usize;
        coords_with_flags += repeat as u16;
        if coords_with_flags >= num_coords {
            break;
        }
    }

    if num_coords != coords_with_flags {
        return 0;
    }
    i += coord_bytes;
    i
}

fn subset_head(head: &Head, loca_format: u8) -> Vec<u8> {
    let mut out = head.offset_data().as_bytes().to_owned();
    out.get_mut(50..52)
        .unwrap()
        .copy_from_slice(&[0, loca_format]);
    out
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_subset_simple_glyph_trim_padding() {
        let plan = Plan::default();
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();

        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let glyph = loca.get_glyf(GlyphId::from(1_u16), &glyf).unwrap().unwrap();

        let subset_output = subset_glyph(&glyph, &plan);
        assert_eq!(subset_output.len(), 23);
        assert_eq!(
            subset_output,
            [
                0x0, 0x1, 0x0, 0xfa, 0x0, 0x32, 0x1, 0x77, 0x0, 0x64, 0x0, 0x3, 0x0, 0x0, 0x37,
                0x33, 0x15, 0x23, 0xfa, 0x7d, 0x7d, 0x64, 0x32
            ]
        );
    }

    #[test]
    fn test_subset_composite_glyph_trim_padding() {
        let mut plan = Plan::default();
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();

        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let glyph = loca.get_glyf(GlyphId::from(4_u16), &glyf).unwrap().unwrap();
        plan.glyph_map
            .insert(GlyphId::from(1_u16), GlyphId::from(2_u16));

        let subset_glyph = subset_glyph(&glyph, &plan);
        assert_eq!(subset_glyph.len(), 20);
        assert_eq!(
            subset_glyph,
            [
                0xff, 0xff, 0x2, 0x26, 0x0, 0x7d, 0x3, 0x20, 0x0, 0xc8, 0x0, 0x46, 0x0, 0x2, 0x32,
                0x32, 0x7f, 0xff, 0x60, 0x0
            ]
        );
    }
}
