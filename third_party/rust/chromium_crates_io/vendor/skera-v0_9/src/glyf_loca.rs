//! impl subset() for glyf and loca
#[cfg(test)]
mod extended_tests;

use crate::{
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Subset,
    SubsetError::{self, SubsetTableError},
    SubsetFlags,
};
use read_fonts::{
    tables::{
        glyf::{
            CompositeGlyph, CompositeGlyphFlags, Glyf,
            Glyph::{self, Composite, Simple},
            SimpleGlyph, SimpleGlyphFlags,
        },
        head::Head,
        loca::Loca,
    },
    types::{GlyphId, Scalar, Tag, Uint24},
    FontRef, TableProvider, TopLevelTable,
};
use write_fonts::FontBuilder;

pub(crate) const GLYF: Tag = Tag::new(b"GLYF");
pub(crate) const LOCA: Tag = Tag::new(b"LOCA");

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
        subset_glyf(self, plan, font, s, builder, false)
    }
}

pub(crate) fn subset_glyf(
    glyf: &Glyf<'_>,
    plan: &Plan,
    font: &FontRef,
    s: &mut Serializer,
    builder: &mut FontBuilder,
    extended: bool,
) -> Result<(), SubsetError> {
    let result = subset_glyf_inner(glyf, plan, font, s, builder, extended);
    if result.is_err() {
        s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
    }
    result
}

fn subset_glyf_inner(
    glyf: &Glyf<'_>,
    plan: &Plan,
    font: &FontRef,
    s: &mut Serializer,
    builder: &mut FontBuilder,
    extended: bool,
) -> Result<(), SubsetError> {
    let loca_tag = if extended { LOCA } else { Loca::TAG };
    let loca = if extended {
        font.loca_extended(None)
    } else {
        font.loca(None)
    }
    .or(Err(SubsetTableError(loca_tag)))?;
    let head = font.head().or(Err(SubsetTableError(Head::TAG)))?;

    let mut subset_glyphs = Vec::with_capacity(plan.new_to_old_gid_list.len());
    let mut max_offset: u32 = 0;

    for (new_gid, old_gid) in &plan.new_to_old_gid_list {
        match loca.get(*old_gid, glyf) {
            Some(g) => {
                if *old_gid == GlyphId::NOTDEF
                    && *new_gid == GlyphId::NOTDEF
                    && !plan
                        .subset_flags
                        .contains(SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE)
                {
                    subset_glyphs.push(SubsetGlyph::Empty);
                    continue;
                }

                let Some(glyph) = g.into_glyph() else {
                    subset_glyphs.push(SubsetGlyph::Empty);
                    continue;
                };
                let subset_glyph = SubsetGlyph::new(&glyph, plan);
                let trimmed_len = subset_glyph.len();
                max_offset = max_offset
                    .checked_add(
                        u32::try_from(padded_size(trimmed_len))
                            .map_err(|_| SubsetTableError(Glyf::TAG))?,
                    )
                    .ok_or(SubsetTableError(Glyf::TAG))?;
                subset_glyphs.push(subset_glyph);
            }
            None => {
                return Err(SubsetTableError(Glyf::TAG));
            }
        }
    }

    //TODO: support force_long_loca in the plan
    let loca_format: u8 = if max_offset < 0x1FFFF { 0 } else { 1 };
    let loca_out = write_glyf_loca(plan, s, loca_format, &subset_glyphs)?;

    let head_out = subset_head(&head, loca_format);

    builder.add_raw(loca_tag, loca_out);
    builder.add_raw(Head::TAG, head_out);
    Ok(())
}

fn padded_size(len: usize) -> usize {
    len + len % 2
}

// glyf data is written into the serializer, returning loca data to be added by FontBuilder
fn write_glyf_loca(
    plan: &Plan,
    s: &mut Serializer,
    loca_format: u8,
    subset_glyphs: &[SubsetGlyph<'_>],
) -> Result<Vec<u8>, SubsetError> {
    let entry_size = if loca_format == 0 { 2 } else { 4 };
    let total_len = (plan.num_output_glyphs + 1) * entry_size;
    let mut loca_out = vec![0u8; total_len];

    let init_len = s.length();
    let mut last: usize = 0;
    if loca_format == 0 {
        let mut offset: u32 = 0;
        let mut value = 0_u16.to_be_bytes();
        for ((new_gid, _), g) in plan.new_to_old_gid_list.iter().zip(subset_glyphs) {
            let gid = new_gid.to_u32() as usize;

            while last < gid {
                let pos = (last + 1) * 2;
                loca_out[pos..pos + 2].copy_from_slice(&value);
                last += 1;
            }
            let padded_len = padded_size(g.len());
            offset += padded_len as u32;
            value = ((offset >> 1) as u16).to_be_bytes();
            let pos = (last + 1) * 2;
            loca_out[pos..pos + 2].copy_from_slice(&value);
            g.serialize(s, plan)?;
            if padded_len > g.len() {
                s.embed_bytes(&[0])
                    .map_err(|_| SubsetError::SubsetTableError(Glyf::TAG))?;
            }

            last += 1;
        }

        while last < plan.num_output_glyphs {
            let pos = (last + 1) * 2;
            loca_out[pos..pos + 2].copy_from_slice(&value);
            last += 1;
        }
    } else {
        let mut offset: u32 = 0;
        let mut value = 0_u32.to_be_bytes();
        for ((new_gid, _), g) in plan.new_to_old_gid_list.iter().zip(subset_glyphs) {
            let gid = new_gid.to_u32() as usize;

            while last < gid {
                let pos = (last + 1) * 4;
                loca_out[pos..pos + 4].copy_from_slice(&value);
                last += 1;
            }
            offset += g.len() as u32;
            value = offset.to_be_bytes();
            let pos = (last + 1) * 4;
            loca_out[pos..pos + 4].copy_from_slice(&value);

            g.serialize(s, plan)?;

            last += 1;
        }

        while last < plan.num_output_glyphs {
            let pos = (last + 1) * 4;
            loca_out[pos..pos + 4].copy_from_slice(&value);
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

enum SubsetGlyph<'a> {
    Empty,
    Simple(SimpleSubsetGlyph<'a>),
    Composite(&'a [u8]),
}

impl<'a> SubsetGlyph<'a> {
    fn new(glyph: &Glyph<'a>, plan: &Plan) -> Self {
        match glyph {
            Simple(glyph) => SimpleSubsetGlyph::new(glyph, plan)
                .map(Self::Simple)
                .unwrap_or(Self::Empty),
            Composite(glyph) => trim_composite_glyph(glyph, plan)
                .map(Self::Composite)
                .unwrap_or(Self::Empty),
        }
    }

    fn len(&self) -> usize {
        match self {
            Self::Empty => 0,
            Self::Simple(glyph) => glyph.len(),
            Self::Composite(bytes) => bytes.len(),
        }
    }

    fn serialize(&self, s: &mut Serializer, plan: &Plan) -> Result<(), SubsetError> {
        match self {
            Self::Empty => Ok(()),
            Self::Simple(glyph) => glyph.serialize(s),
            Self::Composite(bytes) => serialize_composite_glyph(s, bytes, plan),
        }
    }
}

struct SimpleSubsetGlyph<'a> {
    header: &'a [u8],
    instructions: &'a [u8],
    glyph_data: &'a [u8],
    drop_hinting: bool,
    set_overlaps_flag: bool,
}

impl<'a> SimpleSubsetGlyph<'a> {
    fn new(g: &SimpleGlyph<'a>, plan: &Plan) -> Option<Self> {
        let num_coords = g.end_pts_of_contours().last()?.get() + 1;
        let glyph_data = g.glyph_data();
        let glyph_data = glyph_data.get(..trim_simple_glyph_padding(glyph_data, num_coords))?;
        if glyph_data.is_empty() {
            return None;
        }

        let glyph_bytes = g.offset_data().as_bytes();
        let header_len = 10 + 2 * g.end_pts_of_contours().len() + 2;
        let drop_hinting = plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING);
        let set_overlaps_flag = plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_SET_OVERLAPS_FLAG);

        let (header, instructions, glyph_data) = if !drop_hinting && !set_overlaps_flag {
            let total_len = header_len + g.instruction_length() as usize + glyph_data.len();
            (&[][..], &[][..], glyph_bytes.get(..total_len)?)
        } else {
            let header = glyph_bytes.get(..header_len)?;
            let instructions = if drop_hinting {
                &[]
            } else {
                glyph_bytes.get(header_len..header_len + g.instruction_length() as usize)?
            };
            (header, instructions, glyph_data)
        };

        Some(Self {
            header,
            instructions,
            glyph_data,
            drop_hinting,
            set_overlaps_flag,
        })
    }

    fn len(&self) -> usize {
        self.header.len() + self.instructions.len() + self.glyph_data.len()
    }

    fn serialize(&self, s: &mut Serializer) -> Result<(), SubsetError> {
        let map_err = |_| SubsetTableError(Glyf::TAG);
        if !self.drop_hinting && !self.set_overlaps_flag {
            return s.embed_bytes(self.glyph_data).map(|_| ()).map_err(map_err);
        }

        if self.drop_hinting {
            s.embed_bytes(&self.header[..self.header.len() - 2])
                .map_err(map_err)?;
            s.embed(0_u16).map_err(map_err)?;
        } else {
            s.embed_bytes(self.header).map_err(map_err)?;
            s.embed_bytes(self.instructions).map_err(map_err)?;
        }

        if self.set_overlaps_flag {
            s.embed(self.glyph_data[0] | SimpleGlyphFlags::OVERLAP_SIMPLE.bits())
                .map_err(map_err)?;
            s.embed_bytes(&self.glyph_data[1..]).map_err(map_err)?;
        } else {
            s.embed_bytes(self.glyph_data).map_err(map_err)?;
        }
        Ok(())
    }
}

fn trim_composite_glyph<'a>(g: &CompositeGlyph<'a>, plan: &Plan) -> Option<&'a [u8]> {
    let glyph_bytes = g.offset_data().as_bytes();
    let len = glyph_bytes.len();
    if len < 10 {
        return None;
    }

    let mut more = true;
    let mut we_have_instructions = false;
    let mut i: usize = 10;

    while more {
        if i + 4 > len {
            return None;
        }
        let flags = CompositeGlyphFlags::from_bits_truncate(u16::from_be_bytes([
            glyph_bytes[i],
            glyph_bytes[i + 1],
        ]));

        if flags.contains(CompositeGlyphFlags::WE_HAVE_INSTRUCTIONS) {
            we_have_instructions = true;
        }

        let (old_gid, gid_len) = component_gid(glyph_bytes.get(i + 2..)?, flags)?;
        if !plan.glyph_map.contains_key(&old_gid) {
            return None;
        }

        i += 2 + gid_len;

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
        if i + 2 > len {
            return None;
        }
        let instruction_len = u16::from_be_bytes([glyph_bytes[i], glyph_bytes[i + 1]]) as usize;
        i += 2 + instruction_len;
    }

    if i > len {
        return None;
    }

    glyph_bytes.get(..i)
}

fn serialize_composite_glyph(
    s: &mut Serializer,
    bytes: &[u8],
    plan: &Plan,
) -> Result<(), SubsetError> {
    let map_err = |_| SubsetTableError(Glyf::TAG);
    let start = s.embed_bytes(bytes).map_err(map_err)?;
    let out = s
        .get_mut_data(start..start + bytes.len())
        .ok_or(SubsetTableError(Glyf::TAG))?;

    let drop_hinting = plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING);
    let set_overlaps_flag = plan
        .subset_flags
        .contains(SubsetFlags::SUBSET_FLAGS_SET_OVERLAPS_FLAG);

    let mut more = true;
    let mut i: usize = 10;
    let len = out.len();

    while more {
        if i + 4 > len {
            return Err(SubsetTableError(Glyf::TAG));
        }
        let orig_flags = u16::from_be_bytes([out[i], out[i + 1]]);
        let mut flags = orig_flags;

        if drop_hinting {
            flags &= !CompositeGlyphFlags::WE_HAVE_INSTRUCTIONS.bits();
        }

        // only set overlaps flag on the first component
        if set_overlaps_flag && i == 10 {
            flags |= CompositeGlyphFlags::OVERLAP_COMPOUND.bits();
        }

        if flags != orig_flags {
            out[i..i + 2].copy_from_slice(&flags.to_be_bytes());
        }

        let comp_flags = CompositeGlyphFlags::from_bits_truncate(orig_flags);
        let (old_gid, gid_len) =
            component_gid(&out[i + 2..], comp_flags).ok_or(SubsetTableError(Glyf::TAG))?;
        let new_gid = plan
            .glyph_map
            .get(&old_gid)
            .ok_or(SubsetTableError(Glyf::TAG))?
            .to_u32();
        if gid_len == 3 {
            let new_gid =
                Uint24::try_from(new_gid as usize).map_err(|_| SubsetTableError(Glyf::TAG))?;
            out[i + 2..i + 5].copy_from_slice(&new_gid.to_be_bytes());
        } else {
            let new_gid = u16::try_from(new_gid).map_err(|_| SubsetTableError(Glyf::TAG))?;
            out[i + 2..i + 4].copy_from_slice(&new_gid.to_be_bytes());
        }

        i += 2 + gid_len;

        if comp_flags.contains(CompositeGlyphFlags::ARG_1_AND_2_ARE_WORDS) {
            i += 4;
        } else {
            i += 2;
        }

        if comp_flags.contains(CompositeGlyphFlags::WE_HAVE_A_SCALE) {
            i += 2;
        } else if comp_flags.contains(CompositeGlyphFlags::WE_HAVE_AN_X_AND_Y_SCALE) {
            i += 4;
        } else if comp_flags.contains(CompositeGlyphFlags::WE_HAVE_A_TWO_BY_TWO) {
            i += 8;
        }

        more = comp_flags.contains(CompositeGlyphFlags::MORE_COMPONENTS);
    }

    Ok(())
}

fn component_gid(bytes: &[u8], flags: CompositeGlyphFlags) -> Option<(GlyphId, usize)> {
    if flags.contains(CompositeGlyphFlags::GID_IS_24_BIT) {
        Some((GlyphId::new(Uint24::read(bytes.get(..3)?)?.to_u32()), 3))
    } else {
        Some((GlyphId::new(u32::from(u16::read(bytes.get(..2)?)?)), 2))
    }
}

// trim padding bytes for simple glyphs, return trimmed length of the raw data for flags & x/y coordinates
fn trim_simple_glyph_padding(glyph_data: &[u8], num_coords: u16) -> usize {
    const REPEAT: u8 = SimpleGlyphFlags::REPEAT_FLAG.bits();
    // Flags are processed in runs of up to this many bytes.
    const RUN: usize = 64;

    let num_coords = num_coords as usize;
    let length = glyph_data.len();
    let mut coord_bytes: usize = 0;
    let mut coords_with_flags: usize = 0;
    let mut i: usize = 0;
    while i < length && coords_with_flags < num_coords {
        // A flag without REPEAT_FLAG describes exactly one point, so a run of such
        // flags, capped at the number of points left, can be summed in bulk. This
        // is the hot loop for glyphs with many points; the bulk sum vectorizes.
        let n = (num_coords - coords_with_flags).min(length - i).min(RUN);
        let run = &glyph_data[i..i + n];
        let k = if run.iter().fold(0, |acc, f| acc | f) & REPEAT == 0 {
            n
        } else {
            run.iter().position(|f| f & REPEAT != 0).unwrap_or(n)
        };
        // at most RUN * 4 = 256, so summing in u16 is enough and vectorizes well
        coord_bytes += run[..k]
            .iter()
            .map(|&f| coord_bytes_for_flag(f) as u16)
            .sum::<u16>() as usize;
        coords_with_flags += k;
        i += k;

        if k < n {
            // glyph_data[i] has REPEAT_FLAG
            let flag_byte = glyph_data[i];
            i += 1;
            if i >= length {
                return 0;
            }
            let repeat = glyph_data[i] as usize + 1;
            i += 1;
            coord_bytes += coord_bytes_for_flag(flag_byte) as usize * repeat;
            coords_with_flags += repeat;
        }
    }

    if num_coords != coords_with_flags {
        return 0;
    }
    i + coord_bytes
}

/// Number of x and y coordinate bytes for one point with this flag, computed
/// without branches or a table lookup so it vectorizes.
#[inline]
fn coord_bytes_for_flag(flag: u8) -> u8 {
    // x: 1 byte if X_SHORT (0x02), else 0 if X_IS_SAME (0x10), else 2
    // y: 1 byte if Y_SHORT (0x04), else 0 if Y_IS_SAME (0x20), else 2
    let x_short = (flag >> 1) & 1;
    let y_short = (flag >> 2) & 1;
    let x_long = ((flag >> 1) | (flag >> 4)) & 1 ^ 1;
    let y_long = ((flag >> 2) | (flag >> 5)) & 1 ^ 1;
    x_short + y_short + 2 * (x_long + y_long)
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

    /// The original, flag-at-a-time implementation, kept as a reference.
    fn reference_trim_simple_glyph_padding(glyph_data: &[u8], num_coords: u16) -> usize {
        const COORD_BYTES: [u8; 256] = {
            let mut table = [0u8; 256];
            let mut i = 0;
            while i < 256 {
                let x = if (i & 0x02) != 0 {
                    1
                } else if (i & 0x10) == 0 {
                    2
                } else {
                    0
                };
                let y = if (i & 0x04) != 0 {
                    1
                } else if (i & 0x20) == 0 {
                    2
                } else {
                    0
                };
                table[i] = x + y;
                i += 1;
            }
            table
        };

        let mut coord_bytes: usize = 0;
        let mut coords_with_flags: u32 = 0;
        let length = glyph_data.len();
        let mut i: usize = 0;
        while i < length {
            let flag_byte = glyph_data[i];
            i += 1;
            let mut repeat: u32 = 1;
            if flag_byte & SimpleGlyphFlags::REPEAT_FLAG.bits() != 0 {
                if i >= length {
                    return 0;
                }
                repeat = glyph_data[i] as u32 + 1;
                i += 1;
            }
            coord_bytes += COORD_BYTES[flag_byte as usize] as usize * repeat as usize;
            coords_with_flags += repeat;
            if coords_with_flags >= num_coords as u32 {
                break;
            }
        }
        if num_coords as u32 != coords_with_flags {
            return 0;
        }
        i + coord_bytes
    }

    #[test]
    fn coord_bytes_for_flag_matches_spec() {
        for flag in 0..=255u8 {
            let x = if flag & 0x02 != 0 {
                1
            } else if flag & 0x10 == 0 {
                2
            } else {
                0
            };
            let y = if flag & 0x04 != 0 {
                1
            } else if flag & 0x20 == 0 {
                2
            } else {
                0
            };
            assert_eq!(coord_bytes_for_flag(flag), x + y, "flag {flag:#04x}");
        }
    }

    #[test]
    fn trim_simple_glyph_padding_matches_reference() {
        // deterministic pseudo-random flag streams with varying repeat density,
        // point counts around the real one, and truncated/padded data
        let mut state = 0x2545_f491_4f6c_dd1du64;
        let mut next = || {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            state
        };
        for case in 0..2000 {
            let repeat_one_in = [1, 2, 8, 64, u64::MAX][case % 5];
            let num_flags = (next() % 300) as usize;
            let mut data = Vec::new();
            let mut points = 0u32;
            for _ in 0..num_flags {
                let mut flag = (next() as u8) & !SimpleGlyphFlags::REPEAT_FLAG.bits();
                if next() % repeat_one_in == 0 {
                    flag |= SimpleGlyphFlags::REPEAT_FLAG.bits();
                    let repeat = next() as u8;
                    data.push(flag);
                    data.push(repeat);
                    points += repeat as u32 + 1;
                } else {
                    data.push(flag);
                    points += 1;
                }
            }
            // coordinate bytes plus up to 3 bytes of padding, sometimes truncated
            let extra = (next() % 600) as usize;
            data.extend(std::iter::repeat_n(0u8, extra));
            let num_coords = match next() % 4 {
                0 => points.saturating_sub(1),
                1 => points + 1,
                _ => points,
            }
            .min(u16::MAX as u32) as u16;
            assert_eq!(
                trim_simple_glyph_padding(&data, num_coords),
                reference_trim_simple_glyph_padding(&data, num_coords),
                "case {case}"
            );
        }
    }

    #[test]
    fn test_subset_simple_glyph_trim_padding() {
        let plan = Plan::default();
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();

        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let glyph = loca
            .get(GlyphId::from(1_u16), &glyf)
            .and_then(|g| g.into_glyph())
            .unwrap();

        let subset_glyph = SubsetGlyph::new(&glyph, &plan);
        assert_eq!(subset_glyph.len(), 23);
        let mut s = Serializer::new(1024);
        subset_glyph.serialize(&mut s, &plan).unwrap();
        assert_eq!(
            s.copy_bytes(),
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
        let glyph = loca
            .get(GlyphId::from(4_u16), &glyf)
            .and_then(|g| g.into_glyph())
            .unwrap();
        plan.glyph_map
            .insert(GlyphId::from(1_u16), GlyphId::from(2_u16));

        let subset_glyph = SubsetGlyph::new(&glyph, &plan);
        assert_eq!(subset_glyph.len(), 20);
        let mut s = Serializer::new(1024);
        subset_glyph.serialize(&mut s, &plan).unwrap();
        assert_eq!(
            s.copy_bytes(),
            [
                0xff, 0xff, 0x2, 0x26, 0x0, 0x7d, 0x3, 0x20, 0x0, 0xc8, 0x0, 0x46, 0x0, 0x2, 0x32,
                0x32, 0x7f, 0xff, 0x60, 0x0
            ]
        );
    }

    #[test]
    fn test_subset_composite_glyph_set_overlaps_flag() {
        let mut plan = Plan {
            subset_flags: SubsetFlags::SUBSET_FLAGS_SET_OVERLAPS_FLAG,
            ..Default::default()
        };
        let font = FontRef::new(font_test_data::GLYF_COMPONENTS).unwrap();

        let loca = font.loca(None).unwrap();
        let glyf = font.glyf().unwrap();
        let glyph = loca
            .get(GlyphId::from(4_u16), &glyf)
            .and_then(|g| g.into_glyph())
            .unwrap();
        plan.glyph_map
            .insert(GlyphId::from(1_u16), GlyphId::from(2_u16));

        let subset_glyph = SubsetGlyph::new(&glyph, &plan);
        assert_eq!(subset_glyph.len(), 20);
        let mut s = Serializer::new(1024);
        subset_glyph.serialize(&mut s, &plan).unwrap();
        // Flag at offset 10..12 is changed from 0x0046 to 0x0446 (OVERLAP_COMPOUND = 0x0400)
        assert_eq!(
            s.copy_bytes(),
            [
                0xff, 0xff, 0x2, 0x26, 0x0, 0x7d, 0x3, 0x20, 0x0, 0xc8, 0x4, 0x46, 0x0, 0x2, 0x32,
                0x32, 0x7f, 0xff, 0x60, 0x0
            ]
        );
    }

    #[test]
    fn test_subset_composite_glyph_drop_hints() {
        use read_fonts::{FontData, FontRead};

        // Composite glyph with WE_HAVE_INSTRUCTIONS (0x0100) and 2 bytes instructions:
        let raw = [
            0xff, 0xff, // numberOfContours = -1
            0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x64, // bbox (8 bytes)
            0x01, 0x01, // flags = ARG_1_AND_2_ARE_WORDS | WE_HAVE_INSTRUCTIONS
            0x00, 0x01, // component gid = 1
            0x00, 0x00, 0x00, 0x00, // args (4 bytes)
            0x00, 0x02, // num_instructions = 2
            0xaa, 0xbb, // instructions (2 bytes)
            0x00, 0x00, // padding (2 bytes)
        ];

        let comp = CompositeGlyph::read(FontData::new(&raw)).unwrap();
        let glyph = Glyph::Composite(comp);

        let mut plan = Plan::default();
        plan.glyph_map
            .insert(GlyphId::from(1_u16), GlyphId::from(5_u16));

        // When hinting is retained, instructions are kept and padding is trimmed:
        let subset_retained = SubsetGlyph::new(&glyph, &plan);
        assert_eq!(subset_retained.len(), 22);
        let mut s = Serializer::new(1024);
        subset_retained.serialize(&mut s, &plan).unwrap();
        assert_eq!(
            s.copy_bytes(),
            [
                0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x64, 0x01,
                0x01, // WE_HAVE_INSTRUCTIONS retained
                0x00, 0x05, // remapped gid = 5
                0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xaa, 0xbb,
            ]
        );

        // When SUBSET_FLAGS_NO_HINTING is set, instructions are dropped and flag cleared:
        plan.subset_flags = SubsetFlags::SUBSET_FLAGS_NO_HINTING;
        let subset_dropped = SubsetGlyph::new(&glyph, &plan);
        assert_eq!(subset_dropped.len(), 18);
        let mut s = Serializer::new(1024);
        subset_dropped.serialize(&mut s, &plan).unwrap();
        assert_eq!(
            s.copy_bytes(),
            [
                0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x64, 0x00,
                0x01, // WE_HAVE_INSTRUCTIONS cleared
                0x00, 0x05, // remapped gid = 5
                0x00, 0x00, 0x00, 0x00,
            ]
        );
    }
}
