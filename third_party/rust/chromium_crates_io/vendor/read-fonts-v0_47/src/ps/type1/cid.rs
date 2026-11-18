//! The PostScript resource and binary maps of a CID-keyed Type 1 font.
//!
//! See Adobe Technical Note #5014, sections 5-6, and FreeType's `src/cid`.

use super::*;

pub(super) struct CidData {
    pub data: Vec<u8>,
    // Keep CID numbering, including holes. An invalid glyph must not prevent
    // other glyphs in the font from being used.
    pub glyphs: Vec<Option<CidGlyph>>,
}

pub(super) struct CidGlyph {
    pub subfont: u32,
    pub data: Range<usize>,
}

pub(super) fn is_cid(data: &[u8]) -> bool {
    data.starts_with(b"%!PS-Adobe-3.0 Resource-CIDFont")
}

struct FontDict {
    subfont: Subfont,
    len_iv: i64,
    subr_map_offset: usize,
    subr_count: usize,
    sd_bytes: usize,
}

impl Default for FontDict {
    fn default() -> Self {
        Self {
            subfont: Subfont::default(),
            len_iv: 4,
            subr_map_offset: 0,
            subr_count: 0,
            sd_bytes: 0,
        }
    }
}

pub(super) fn load(data: &[u8]) -> Option<Type1Font> {
    let mut font = Type1Font::empty()?;
    let mut parser = Parser::new(data);
    let mut dicts = Vec::new();
    let mut dict_index = None;
    let mut dicts_seen = 0usize;
    let mut cid_count = 0;
    let mut cid_map_offset = 0;
    let mut fd_bytes = 0;
    let mut gd_bytes = 0;
    let mut previous = [None; 2];
    let binary = loop {
        let token = parser.next_token(true)?;
        match token {
            Token::Comment(comment) => {
                // These markers delimit font dictionaries in the resource
                // format. Tokenizing comments avoids matching text in strings
                // and procedures. FreeType uses the same markers.
                if comment.starts_with(b"%ADOBeginFontDict") {
                    dicts.get(dicts_seen)?;
                    dict_index = Some(dicts_seen);
                    dicts_seen += 1;
                } else if comment.starts_with(b"%ADOEndFontDict") {
                    dict_index = None;
                }
                continue;
            }
            Token::Raw(b"StartData") => {
                let Some(Token::Int(length)) = previous[1] else {
                    return None;
                };
                let length = usize::try_from(length).ok()?;
                // StartData consumes exactly one separator. In binary data,
                // the next byte can itself be whitespace.
                if !is_whitespace(parser.next_byte()?) {
                    return None;
                }
                let bytes = data.get(parser.pos..)?;
                break match previous[0] {
                    Some(Token::LitString(b"Hex")) => Cow::Owned(read_hex(bytes, length)?),
                    Some(Token::LitString(b"Binary")) => Cow::Borrowed(bytes.get(..length)?),
                    _ => return None,
                };
            }
            Token::Name(b"sfnts") => return None, // CIDFontType 2 is not Type 1.
            Token::Name(name) => {
                if let Some(index) = dict_index {
                    let dict: &mut FontDict = dicts.get_mut(index)?;
                    match name {
                        b"FontMatrix" => {
                            let (matrix, changes_upem) = parser.read_font_matrix_with_scale()?;
                            dict.subfont.matrix = matrix.matrix;
                            // Like FreeType, a non-default matrix updates the
                            // face UPEM; subsequent default matrices leave it.
                            if changes_upem {
                                font.upem = matrix.scale as u16 as i32;
                            }
                        }
                        b"lenIV" => dict.len_iv = parser.read_int()?,
                        b"SubrMapOffset" => dict.subr_map_offset = read_usize(&mut parser)?,
                        b"SubrCount" => dict.subr_count = read_usize(&mut parser)?,
                        b"SDBytes" => dict.sd_bytes = read_usize(&mut parser)?,
                        b"FontType" if parser.read_int()? != 1 => return None,
                        _ => {}
                    }
                } else {
                    match name {
                        b"CIDFontName" => font.name = parser.read_string(),
                        b"CIDFontType" if parser.read_int()? != 0 => return None,
                        b"CIDCount" => cid_count = read_usize(&mut parser)?,
                        b"CIDMapOffset" => cid_map_offset = read_usize(&mut parser)?,
                        b"FDBytes" => fd_bytes = read_usize(&mut parser)?,
                        b"GDBytes" => gd_bytes = read_usize(&mut parser)?,
                        b"FDArray" => {
                            // Repeated declarations would invalidate the
                            // dictionary indices already read.
                            if !dicts.is_empty() {
                                return None;
                            }
                            let count = read_usize(&mut parser)?;
                            parser.expect(Token::Raw(b"array"))?;
                            // Every dictionary needs at least its marker.
                            if count == 0 || count > data.len() / 17 {
                                return None;
                            }
                            dicts.resize_with(count, FontDict::default);
                        }
                        b"FontBBox" => {
                            let [x_min, y_min, x_max, y_max] = parser.read_font_bbox()?;
                            font.bbox = BoundingBox {
                                x_min,
                                y_min,
                                x_max,
                                y_max,
                            };
                        }
                        b"FullName" => font.full_name = parser.read_string(),
                        b"FamilyName" => font.family_name = parser.read_string(),
                        b"Weight" => font.weight = parser.read_string(),
                        b"version" => font.version = parser.read_string(),
                        b"Notice" => font.notice = parser.read_string(),
                        b"ItalicAngle" => font.italic_angle = parser.read_num_as_int()?,
                        b"isFixedPitch" => {
                            font.is_fixed_pitch = parser.next()? == Token::Raw(b"true")
                        }
                        b"UnderlinePosition" => {
                            font.underline_position = parser.read_num_as_int()?
                        }
                        b"UnderlineThickness" => {
                            font.underline_thickness = parser.read_num_as_int()?
                        }
                        _ => {}
                    }
                }
            }
            _ => {}
        }
        previous = [previous[1], Some(token)];
    };
    if dicts.is_empty() || dicts_seen != dicts.len() || fd_bytes > 4 || !(1..=4).contains(&gd_bytes)
    {
        return None;
    }
    // Bound both index allocations and the sentinel used to find the last
    // glyph's end before reading any entries.
    u32::try_from(cid_count).ok()?;
    let entry_size = fd_bytes + gd_bytes;
    let map_len = cid_count.checked_add(1)?.checked_mul(entry_size)?;
    let map = binary.get(cid_map_offset..cid_map_offset.checked_add(map_len)?)?;

    // Overlapping source ranges must not amplify a small file into unbounded
    // decoded storage. Base the budget on the complete input, including font
    // dictionaries, to allow shared subroutine maps as FreeType does.
    let mut budget = data.len().checked_mul(4)?;
    for dict in &mut dicts {
        if dict.sd_bytes > 4 || (dict.subr_count != 0 && dict.sd_bytes == 0) {
            return None;
        }
        if dict.subr_count == 0 {
            continue;
        }
        let size = dict.subr_count.checked_add(1)?.checked_mul(dict.sd_bytes)?;
        let subr_map = binary.get(dict.subr_map_offset..dict.subr_map_offset.checked_add(size)?)?;
        // Count index storage/work too: many FDs can point at the same map
        // of zero-length subroutines without consuming any charstring bytes.
        budget = budget.checked_sub(subr_map.len())?;
        let subrs = &mut dict.subfont.subrs;
        subrs.is_dense = true;
        for index in 0..dict.subr_count {
            let start = offset(subr_map, index * dict.sd_bytes, dict.sd_bytes)?;
            let end = offset(subr_map, (index + 1) * dict.sd_bytes, dict.sd_bytes)?;
            let bytes = binary.get(start..end)?;
            let range = append_charstring(&mut subrs.data, bytes, dict.len_iv, &mut budget)?;
            subrs.index.push((u32::try_from(index).ok()?, range));
        }
    }
    let mut cid = CidData {
        data: Vec::new(),
        glyphs: Vec::with_capacity(cid_count),
    };
    for index in 0..cid_count {
        let pos = index * entry_size;
        let subfont = offset(map, pos, fd_bytes)?;
        let start = offset(map, pos + fd_bytes, gd_bytes)?;
        let end = offset(map, pos + entry_size + fd_bytes, gd_bytes)?;
        let glyph = if let (Some(dict), Some(bytes)) = (dicts.get(subfont), binary.get(start..end))
        {
            // An empty charstring is a valid empty slot, even when lenIV > 0.
            if bytes.is_empty() {
                Some(CidGlyph {
                    subfont: subfont as u32,
                    data: cid.data.len()..cid.data.len(),
                })
            } else if dict.len_iv >= 0 && dict.len_iv as u64 > bytes.len() as u64 {
                None
            } else {
                let range = append_charstring(&mut cid.data, bytes, dict.len_iv, &mut budget)?;
                Some(CidGlyph {
                    subfont: subfont as u32,
                    data: range,
                })
            }
        } else {
            None
        };
        cid.glyphs.push(glyph);
    }
    font.subfonts = dicts.into_iter().map(|dict| dict.subfont).collect();
    font.cid = Some(cid);
    // There is no top level outline transform. FreeType defaults the face's
    // UPEM to 1000 if none of the dictionaries supplied a nonzero value.
    if font.upem == 0 {
        font.upem = 1000;
    }
    Some(font)
}

fn read_usize(parser: &mut Parser<'_>) -> Option<usize> {
    usize::try_from(parser.read_int()?).ok()
}

fn offset(data: &[u8], start: usize, width: usize) -> Option<usize> {
    let bytes = data.get(start..start.checked_add(width)?)?;
    Some(
        bytes
            .iter()
            .fold(0, |value, byte| (value << 8) | *byte as usize),
    )
}

fn append_charstring(
    out: &mut Vec<u8>,
    bytes: &[u8],
    len_iv: i64,
    budget: &mut usize,
) -> Option<Range<usize>> {
    *budget = budget.checked_sub(bytes.len())?;
    let start = out.len();
    // Empty subroutine slots have no encryption padding, even when lenIV > 0.
    if len_iv >= 0 && !bytes.is_empty() {
        let padding = usize::try_from(len_iv).ok()?;
        if padding > bytes.len() {
            return None;
        }
        out.extend(decrypt(bytes.iter().copied(), CHARSTRING_SEED).skip(padding));
    } else {
        out.extend_from_slice(bytes);
    }
    Some(start..out.len())
}

fn read_hex(data: &[u8], length: usize) -> Option<Vec<u8>> {
    // Each output byte needs two digits, except a final padded nibble.
    if length > data.len().div_ceil(2) {
        return None;
    }
    // Validate only the requested bytes. Unlike eexec, CID data rejects
    // invalid digits and requires '>' to pad a final nibble.
    let mut end = 0;
    let mut digits = 0;
    while digits / 2 < length {
        let byte = *data.get(end)?;
        end += 1;
        match byte {
            b'>' => break,
            byte if is_whitespace(byte) => {}
            byte if byte.is_ascii_hexdigit() => digits += 1,
            _ => return None,
        }
    }
    let mut out = Vec::with_capacity(length);
    out.extend(decode_hex(data[..end].iter().copied()));
    (out.len() == length).then_some(out)
}

#[cfg(all(test, feature = "type1"))]
mod tests {
    use super::*;
    use crate::model::{Font, Format};
    use crate::ps::charstring::test_helpers::CaptureCommandSink;

    #[test]
    fn hex_data_length_and_termination() {
        type Case<'a> = (&'a [u8], usize, Option<&'a [u8]>);
        let cases: &[Case<'_>] = &[
            (b"", 0, Some(b"")),
            (b"!", 0, Some(b"")),
            (b"aB 0c", 2, Some(&[0xab, 0x0c])),
            (b"\0\t\n\r\x0c A B", 1, Some(&[0xab])),
            (b"AB!", 1, Some(&[0xab])),
            (b"ABCD", 1, Some(&[0xab])),
            (b"A>", 1, Some(&[0xa0])),
            (b"AB C >", 2, Some(&[0xab, 0xc0])),
            (b"A", 1, None),
            (b"A ", 1, None),
            (b"A!", 1, None),
            (b"A\x80", 1, None),
            (b"AB>CD", 2, None),
            (b"A>BC", 2, None),
            (b">AB", 1, None),
            (b"AB", usize::MAX, None),
        ];
        for &(data, length, expected) in cases {
            assert_eq!(
                read_hex(data, length).as_deref(),
                expected,
                "{data:?}, {length}"
            );
        }
    }

    fn replace(data: &[u8], from: &[u8], to: &[u8]) -> Vec<u8> {
        let pos = data
            .windows(from.len())
            .position(|part| part == from)
            .unwrap();
        [data[..pos].as_ref(), to, &data[pos + from.len()..]].concat()
    }

    #[test]
    fn dictionaries_select_subroutines_and_transforms() {
        for hex in [false, true] {
            let data = font_test_data::type1::cid_font(hex);
            let font = Type1Font::new(&data).unwrap();
            assert!(font.is_cid_keyed());
            assert_eq!(font.num_glyphs(), 4);
            assert_eq!(font.max_cid(), Some(3));
            assert_eq!(font.upem(), 1000);
            assert_eq!(font.name(), Some("TwoDicts"));
            assert_eq!(font.family_name(), Some("Two Dicts"));
            assert_eq!(font.glyph_names().count(), 0);
            assert!(font.encoding().is_none());
            assert!(font.unicode_charmap().iter().next().is_none());
            for (id, advance, bbox) in [(0, 600, [0, 0, 20, 30]), (1, 1210, [10, 20, 115, 70])] {
                let gid = GlyphId::new(id);
                assert_eq!(font.subfont_index(gid), Some(id));
                assert_eq!(font.glyph_by_cid(id), Some(gid));
                assert_eq!(
                    font.evaluate_width(gid).unwrap(),
                    Some(Fixed::from_i32(advance))
                );
                let bounds = font.evaluate_extents(gid).unwrap().unwrap();
                assert_eq!(
                    [bounds.x_min, bounds.y_min, bounds.x_max, bounds.y_max],
                    bbox.map(Fixed::from_i32)
                );
                let mut sink = CaptureCommandSink::default();
                assert_eq!(
                    font.draw(gid, None, &mut crate::model::glyph::outline::NullSink)
                        .unwrap(),
                    Some(advance as f32)
                );
                // Scaled drawing and width-only evaluation use the same FD.
                assert_eq!(
                    font.draw(gid, Some(20.0), &mut crate::model::glyph::outline::NullSink)
                        .unwrap(),
                    font.advance(gid, Some(20.0)).unwrap()
                );
                assert_eq!(
                    font.evaluate_charstring(gid, &mut sink).unwrap(),
                    Some(Fixed::from_i32(600))
                );
            }
            assert!(font.glyph_by_cid(2).is_none());
            assert!(font.evaluate_width(GlyphId::new(2)).is_err());
            assert_eq!(font.evaluate_width(GlyphId::new(3)).unwrap(), None);
            assert_eq!(font.evaluate_extents(GlyphId::new(3)).unwrap(), None);
            assert!(font.glyph_by_cid(4).is_none());
        }
    }

    #[test]
    fn model_uses_cids_without_names_or_charmaps() {
        let data = font_test_data::type1::cid_font(false);
        assert_eq!(Format::new(&data), Some(Format::Type1));
        let font = Font::new(data, 0).unwrap();
        assert!(font.is_cid_keyed());
        assert_eq!(font.max_cid(), Some(3));
        assert_eq!(font.glyph_by_cid(1), Some(GlyphId::new(1)));
        assert!(font.glyph_by_cid(2).is_none());
        assert!(font.glyph_name(GlyphId::new(1)).unwrap().is_synthesized());
        assert!(font.glyph_by_name("gid1").is_none());
        assert_eq!(font.glyph_metrics().h_advance(GlyphId::new(1)), 1210.0);
        let extents = font.glyph_metrics().extents(GlyphId::new(1)).unwrap();
        assert_eq!(
            [
                extents.x_bearing,
                extents.y_bearing,
                extents.width,
                extents.height
            ],
            [10.0, 70.0, 105.0, 50.0]
        );
        assert_eq!(font.charmap().encodings().count(), 0);
    }

    #[test]
    fn empty_encrypted_subroutines_do_not_reject_live_glyphs() {
        let data = font_test_data::type1::cid_font(false);
        // The next offset is also 52, adding an unused empty subroutine.
        let data = replace(&data, b"/SubrCount 1", b"/SubrCount 2");
        let font = Type1Font::new(&data).unwrap();
        assert_eq!(font.subfonts[0].subrs.get(1), Some(&[][..]));
        for (cid, advance) in [(0, 600.0), (1, 1210.0)] {
            let gid = GlyphId::new(cid);
            assert_eq!(font.advance(gid, None).unwrap(), Some(advance));
            assert_eq!(
                font.draw(gid, None, &mut crate::model::glyph::outline::NullSink)
                    .unwrap(),
                Some(advance)
            );
        }
    }

    #[test]
    fn shared_subroutine_maps_fit_the_resource_budget() {
        // Five FDs share 100 one-byte return subroutines. The binary block
        // is 390 bytes, but loading charges 1580 bytes for maps and bodies.
        let mut binary = Vec::new();
        for cid in 0..=5u16 {
            binary.push(cid as u8);
            binary.extend_from_slice(&(320 + cid * 14).to_be_bytes());
        }
        for index in 0..=100u16 {
            binary.extend_from_slice(&(220 + index).to_be_bytes());
        }
        binary.extend_from_slice(&[11; 100]);
        for _ in 0..5 {
            // 0 600 hsbw 0 0 rmoveto 99 callsubr 20 30 rlineto closepath endchar
            binary
                .extend_from_slice(&[139, 248, 236, 13, 139, 139, 21, 238, 10, 159, 169, 5, 9, 14]);
        }
        assert_eq!(binary.len(), 390);
        let mut data = b"%!PS-Adobe-3.0 Resource-CIDFont\n/CIDFontType 0 def\n/CIDCount 5 def\n/CIDMapOffset 0 def /FDBytes 1 def /GDBytes 2 def\n/FDArray 5 array\n".to_vec();
        for _ in 0..5 {
            data.extend_from_slice(b"%ADOBeginFontDict\n/FontType 1 def /lenIV -1 def /SubrMapOffset 18 def /SDBytes 2 def /SubrCount 100 def\n%ADOEndFontDict\n");
        }
        data.extend_from_slice(b"(Binary) 390 StartData ");
        data.extend_from_slice(&binary);
        let font = Type1Font::new(&data).unwrap();
        for cid in 0..5 {
            let gid = GlyphId::new(cid);
            assert_eq!(font.advance(gid, None).unwrap(), Some(600.0));
            assert_eq!(
                font.draw(gid, None, &mut crate::model::glyph::outline::NullSink)
                    .unwrap(),
                Some(600.0)
            );
            let mut sink = CaptureCommandSink::default();
            font.evaluate_charstring(gid, &mut sink).unwrap();
            assert!(!sink.0.is_empty());
        }
    }

    #[test]
    fn malformed_maps_are_rejected() {
        let data = font_test_data::type1::cid_font(false);
        for (from, to) in [
            ("/FDBytes 1", "/FDBytes 5"),
            ("/GDBytes 4", "/GDBytes 0"),
            ("/CIDCount 4", "/CIDCount 4294967295"),
            ("/CIDMapOffset 0", "/CIDMapOffset 4294967295"),
            ("/SubrMapOffset 25", "/SubrMapOffset 4294967295"),
            ("/SubrCount 1", "/SubrCount 4294967295"),
            ("/SDBytes 4", "/SDBytes 0"),
            ("/SDBytes 4", "/SDBytes 5"),
            ("/FDArray 2", "/FDArray 4294967295"),
            ("/lenIV 4", "/lenIV 1000"),
            ("/CIDFontType 0", "/CIDFontType 2"),
        ] {
            assert!(
                Type1Font::new(&replace(&data, from.as_bytes(), to.as_bytes())).is_err(),
                "{to}"
            );
        }
        // Every truncated prefix must be safe to parse, even inside binary data.
        for end in 0..data.len() {
            let _ = Type1Font::new(&data[..end]);
        }
    }

    #[test]
    fn tokens_in_comments_strings_and_procedures_are_not_dictionary_fields() {
        let data = font_test_data::type1::cid_font(false);
        let data = replace(&data, b"/CIDCount 4", b"% StartData /CIDCount 999\n( StartData %ADOBeginFontDict ) pop\n{ /CIDCount 999 /FDArray 900 array StartData } pop\n/CIDCount 4");
        assert_eq!(Type1Font::new(&data).unwrap().num_glyphs(), 4);
    }

    #[test]
    fn negative_cid_advances_are_preserved_when_drawing() {
        let data = font_test_data::type1::cid_font(false);
        // Change the unencrypted glyph's width from 600 to -600. Its FD
        // doubles x and translates by 10, so the resulting advance is -1190.
        let data = replace(&data, &[139, 248, 236, 13], &[139, 252, 236, 13]);
        let font = Type1Font::new(&data).unwrap();
        let gid = GlyphId::new(1);
        assert_eq!(font.advance(gid, None).unwrap(), Some(-1190.0));
        assert_eq!(
            font.draw(gid, None, &mut crate::model::glyph::outline::NullSink)
                .unwrap(),
            Some(-1190.0)
        );
        let model = Font::new(data, 0).unwrap();
        assert_eq!(model.glyph_metrics().h_advance(gid), -1190.0);
        assert_eq!(model.glyph_metrics().h_advance_exact(gid).to_f32(), -1190.0);
    }

    #[test]
    fn matrix_procedures_and_cid_upem() {
        let data = font_test_data::type1::cid_font(false);
        let data = replace(&data, b"[0.001 0 0 0.001 0 0]", b"{0.002 0 0 0.002 0 0}");
        let font = Type1Font::new(&data).unwrap();
        // The non-default matrix supplies the face UPEM. The second FD's
        // default yy does not reset it, but each FD has its own normalized matrix.
        assert_eq!(font.upem(), 500);
        assert_eq!(font.advance(GlyphId::NOTDEF, None).unwrap(), Some(600.0));
        // A non-default yy can still round to 1000 UPEM. It must replace
        // the previous FD's UPEM rather than being mistaken for a default.
        let data = replace(&data, b"0.0005 0.001 0.01", b"0.0005 0.0010001 0.01");
        assert_eq!(Type1Font::new(&data).unwrap().upem(), 1000);
    }

    #[test]
    fn recursive_subroutines_fail_only_the_affected_glyph() {
        let data = font_test_data::type1::cid_font(false);
        // Replace FD 1's unencrypted subroutine zero with a call to itself.
        let data = replace(
            &data,
            &[179, 139, 5, 139, 189, 5, 11],
            &[139, 10, 11, 139, 139, 139, 139],
        );
        let font = Type1Font::new(&data).unwrap();
        let mut sink = crate::model::glyph::outline::NullSink;
        assert!(font.draw(GlyphId::NOTDEF, None, &mut sink).is_ok());
        assert!(font.draw(GlyphId::new(1), None, &mut sink).is_err());
    }
}
