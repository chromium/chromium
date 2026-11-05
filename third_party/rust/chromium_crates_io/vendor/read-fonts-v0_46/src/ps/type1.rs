//! Type1 fonts.

use super::{
    charmap::UnicodeCharmap,
    charstring::{
        self, CharstringContext, CharstringKind, CommandSink, NopFilterSink, TransformSink,
    },
    encoding::PredefinedEncoding,
    transform::{self, FontMatrix, ScaledFontMatrix, Transform},
};
use crate::model::Error;
use crate::{
    model::glyph::outline::PathSink,
    types::{BoundingBox, Fixed, GlyphId},
};
use alloc::{borrow::Cow, string::String, vec::Vec};
use core::ops::Range;

/// A Type1 font.
pub struct Type1Font {
    name: Option<String>,
    full_name: Option<String>,
    family_name: Option<String>,
    weight: Option<String>,
    version: Option<String>,
    notice: Option<String>,
    bbox: BoundingBox<Fixed>,
    italic_angle: i32,
    is_fixed_pitch: bool,
    underline_position: i32,
    underline_thickness: i32,
    matrix: ScaledFontMatrix,
    charstrings: Charstrings,
    subrs: Subrs,
    encoding: Option<RawEncoding>,
    /// The default blend of a multiple master font, empty for any other.
    weight_vector: Vec<Fixed>,
    multiple_master: Option<MultipleMaster>,
    /// The number of elements in the `BuildCharArray` that othersubrs
    /// read and write.
    build_char_len: usize,
    unicode_charmap: UnicodeCharmap,
}

impl Type1Font {
    /// Creates a new Type1 font from the given data.
    pub fn new(data: &[u8]) -> Result<Self, Error> {
        // any failure to parse is simply malformed data
        Self::new_impl(data).ok_or(Error::Malformed)
    }

    fn new_impl(data: &[u8]) -> Option<Self> {
        let raw_dicts = RawDicts::new(data)?;
        Self::from_dicts(raw_dicts.base, &raw_dicts.private)
    }

    fn empty() -> Self {
        Self {
            name: None,
            full_name: None,
            family_name: None,
            weight: None,
            version: None,
            notice: None,
            italic_angle: 0,
            is_fixed_pitch: false,
            underline_position: 0,
            underline_thickness: 0,
            matrix: ScaledFontMatrix {
                matrix: FontMatrix::IDENTITY,
                scale: 1000,
            },
            bbox: BoundingBox::default(),
            charstrings: Charstrings::default(),
            subrs: Subrs::default(),
            encoding: None,
            weight_vector: Vec::new(),
            multiple_master: None,
            build_char_len: 0,
            unicode_charmap: UnicodeCharmap::default(),
        }
    }

    fn from_dicts(base: &[u8], private: &[u8]) -> Option<Self> {
        let mut font = Self::empty();
        // Read base dict entries
        let mut encoding_offset = None;
        let mut blend = RawBlend::default();
        let mut parser = Parser::new(base);
        while let Some(token) = parser.next() {
            match token {
                Token::Name(b"FontName") => font.name = parser.read_string(),
                Token::Name(b"FullName") => font.full_name = parser.read_string(),
                Token::Name(b"FamilyName") => font.family_name = parser.read_string(),
                Token::Name(b"Weight") => {
                    // The /Weight token can appear elsewhere in MM fonts so take
                    // the first one
                    if font.weight.is_none() {
                        font.weight = parser.read_string();
                    }
                }
                Token::Name(b"version") => font.version = parser.read_string(),
                Token::Name(b"Notice") => font.notice = parser.read_string(),
                // In the /Blend dictionary of a multiple master font these
                // hold an array with a value for each master. FreeType keeps
                // the font's own value then, so only a number replaces it.
                Token::Name(b"ItalicAngle") => {
                    if let Some(angle) = parser.read_num_as_int() {
                        font.italic_angle = angle;
                    }
                }
                Token::Name(b"isFixedPitch") => match parser.next() {
                    Some(Token::Raw(b"[") | Token::Proc(_)) => {}
                    token => font.is_fixed_pitch = token == Some(Token::Raw(b"true")),
                },
                Token::Name(b"UnderlinePosition") => {
                    if let Some(position) = parser.read_num_as_int() {
                        font.underline_position = position;
                    }
                }
                Token::Name(b"UnderlineThickness") => {
                    if let Some(thickness) = parser.read_num_as_int() {
                        font.underline_thickness = thickness;
                    }
                }
                Token::Name(b"FontBBox") => {
                    if let Some([x_min, y_min, x_max, y_max]) = parser.read_font_bbox() {
                        font.bbox = BoundingBox {
                            x_min,
                            y_min,
                            x_max,
                            y_max,
                        };
                    }
                }
                Token::Name(b"FontMatrix") => font.matrix = parser.read_font_matrix()?,
                // Simply save the encoding offset. We'll parse it after
                // we have read charstrings so we have an accurate mapping
                // if we've synthesized or remapped a notdef glyph
                Token::Name(b"Encoding") => encoding_offset = Some(parser.pos),
                Token::Name(b"WeightVector") => {
                    // Gated on a successful read because some fonts reference
                    // the /WeightVector name outside of a proc, leading to
                    // spurious field reads
                    if let Some(weights) = parser.read_weight_vector() {
                        blend.add_weights(weights)?;
                    }
                }
                // The blend dictionaries are each read only where they hold
                // an array, as FreeType reads them.
                Token::Name(b"BlendAxisTypes") => {
                    if let Some(names) = parser.read_blend_axis_types() {
                        blend.add_axis_types(names)?;
                    }
                }
                Token::Name(b"BlendDesignPositions") => {
                    if let Some(positions) = parser.read_blend_design_positions() {
                        blend.add_design_positions(positions)?;
                    }
                }
                Token::Name(b"BlendDesignMap") => {
                    if let Some(maps) = parser.read_blend_design_map() {
                        blend.add_design_maps(maps)?;
                    }
                }
                Token::Name(b"BuildCharArray") => {
                    if let Some(len) = parser.read_array_len() {
                        blend.build_char_len = len;
                    }
                }
                _ => {}
            }
        }
        if let Some((multiple_master, weights)) = blend.finish() {
            font.multiple_master = Some(multiple_master);
            font.weight_vector = weights;
            font.build_char_len = blend.build_char_len;
        }
        // Read private dict entries
        let mut parser = Parser::new(private);
        // Default value if not present
        let mut len_iv = 4;
        while let Some(token) = parser.next() {
            match token {
                Token::Name(b"lenIV") => len_iv = parser.read_int()?,
                Token::Name(b"Subrs") => {
                    // With synthetic fonts, it's possible to read subroutines
                    // twice. FreeType ignores the second copy.
                    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1855>
                    if font.subrs.index.is_empty() {
                        font.subrs = parser.read_subrs(len_iv)?;
                    }
                }
                Token::Name(b"CharStrings") => {
                    // Some non-standard fonts provide multiple copies of
                    // outlines for different resolutions and FreeType only
                    // retains the first copy, so skip parsing if we've
                    // already read some charstrings.
                    // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2058>
                    if font.charstrings.index.is_empty() {
                        font.charstrings = parser.read_charstrings(len_iv)?;
                    }
                }
                _ => {}
            }
        }
        // Reject fonts that are missing a /CharStrings array
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2665>
        if font.charstrings.index.is_empty() {
            return None;
        }
        if let Some(encoding_offset) = encoding_offset {
            let mut parser = Parser::new(base.get(encoding_offset..)?);
            font.encoding = Some(parser.read_encoding(&font.charstrings)?);
        }
        // We can only generate a Unicode cmap if we have the AGL available
        #[cfg(feature = "agl")]
        {
            font.unicode_charmap = UnicodeCharmap::from_glyph_names(font.glyph_names());
        }
        Some(font)
    }

    /// Returns the PostScript name.
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    /// Returns the full font name.
    pub fn full_name(&self) -> Option<&str> {
        self.full_name.as_deref()
    }

    /// Returns the font family name.
    pub fn family_name(&self) -> Option<&str> {
        self.family_name.as_deref()
    }

    /// Returns the weight or style name.
    pub fn weight(&self) -> Option<&str> {
        self.weight.as_deref()
    }

    /// Returns the font version string.
    pub fn version(&self) -> Option<&str> {
        self.version.as_deref()
    }

    /// Returns the font's copyright or trademark notice.
    pub fn notice(&self) -> Option<&str> {
        self.notice.as_deref()
    }

    /// Returns the italic angle.
    pub fn italic_angle(&self) -> i32 {
        self.italic_angle
    }

    /// Returns true if the glyphs in this font have the same width.
    pub fn is_fixed_pitch(&self) -> bool {
        self.is_fixed_pitch
    }

    /// Returns the position of the center of an underline decoration.
    pub fn underline_position(&self) -> i32 {
        self.underline_position
    }

    /// Returns the suggested size for an underline decoration.
    pub fn underline_thickness(&self) -> i32 {
        self.underline_thickness
    }

    /// Returns the font bounding box.
    pub fn bbox(&self) -> BoundingBox<Fixed> {
        self.bbox
    }

    /// Returns the number of glyphs in the Type1 font.
    pub fn num_glyphs(&self) -> u32 {
        self.charstrings.num_glyphs()
    }

    /// Returns the units per em.
    pub fn upem(&self) -> i32 {
        self.matrix.scale
    }

    /// Returns the top level font matrix.
    pub fn matrix(&self) -> FontMatrix {
        self.matrix.matrix
    }

    /// Returns the appropriate transform for adjusting points and metrics.
    pub fn transform(&self, ppem: Option<f32>) -> Transform {
        let scale = ppem.map(|ppem| Transform::compute_scale(ppem, self.upem()));
        Transform {
            matrix: self.matrix(),
            scale,
        }
    }

    /// Returns the character encoding.
    pub fn encoding(&self) -> Option<Encoding<'_>> {
        self.encoding.as_ref().map(|enc| Encoding {
            encoding: enc,
            charstrings: &self.charstrings,
        })
    }

    /// Returns the Unicode charmap for this font.
    ///
    /// Note that this is an empty mapping if the `agl` feature is not enabled.
    pub fn unicode_charmap(&self) -> &UnicodeCharmap {
        &self.unicode_charmap
    }

    /// Returns the glyph name for the given id.
    pub fn glyph_name(&self, gid: GlyphId) -> Option<&str> {
        self.charstrings.name(gid.to_u32())
    }

    /// Returns an iterator over the pairs of glyph ids and associated names in
    /// the Type1 font.
    pub fn glyph_names(&self) -> impl Iterator<Item = (GlyphId, &str)> {
        (0..self.num_glyphs())
            .filter_map(|idx| Some((GlyphId::new(idx), self.charstrings.name(idx)?)))
    }

    /// Given a glyph identifier in the original glyph order, returns the
    /// possibly remapped identifier.
    ///
    /// This occurs if we remap or synthesize a `.notdef` glyph.
    pub fn remapped_gid(&self, original_gid: GlyphId) -> GlyphId {
        if let Some(orig_notdef) = self.charstrings.orig_notdef_index {
            if original_gid == GlyphId::NOTDEF {
                GlyphId::new(orig_notdef as u32)
            } else if orig_notdef == original_gid.to_u32() as usize {
                GlyphId::NOTDEF
            } else {
                original_gid
            }
        } else {
            original_gid
        }
    }

    /// Returns the design space of a multiple master font.
    ///
    /// `None` for a font with no masters, or with blend dictionaries that
    /// do not agree.
    pub fn multiple_master(&self) -> Option<&MultipleMaster> {
        self.multiple_master.as_ref()
    }

    /// Returns the font drawn with the blend that `weights` give, one per
    /// master.
    ///
    /// [`MultipleMaster::blend_to_weights`] computes the weights for a
    /// position in the blend space.
    pub fn instance<'a>(&'a self, weights: &'a [Fixed]) -> Type1Instance<'a> {
        Type1Instance {
            font: self,
            weights,
        }
    }

    /// Returns the font drawn with its own default blend.
    pub fn default_instance(&self) -> Type1Instance<'_> {
        self.instance(&self.weight_vector)
    }

    /// Evaluates the charstring for the requested glyph far enough to read
    /// its advance width, without drawing it.
    ///
    /// A Type 1 charstring states its width in the `hsbw` or `sbw` that must
    /// open it, so evaluation stops there.
    ///
    /// The font matrix is applied and no size is, so the width is exact and
    /// in the units the rest of the font is measured in.
    /// [`advance`](Self::advance) applies a size as well and reports what
    /// [`draw`](Self::draw) does.
    ///
    /// Unlike [`evaluate_charstring`], the matrix is applied here rather than
    /// left to a caller. That one withholds it so a hinter can be slotted in
    /// ahead of it; nothing hints a width.
    ///
    /// [`evaluate_charstring`]: Self::evaluate_charstring
    pub fn evaluate_width(&self, gid: GlyphId) -> Result<Option<Fixed>, Error> {
        self.default_instance().evaluate_width(gid)
    }

    /// Evaluates the charstring for the requested glyph and sends the results
    /// to the given sink.
    ///
    /// Returns the width the charstring states, in the charstring's own
    /// space: neither the font matrix nor a size is applied, to the width or
    /// to the commands. [`draw`](Self::draw) applies both.
    pub fn evaluate_charstring(
        &self,
        gid: GlyphId,
        sink: &mut impl CommandSink,
    ) -> Result<Option<Fixed>, Error> {
        self.default_instance().evaluate_charstring(gid, sink)
    }

    /// Returns the advance width of the glyph with an optional size in
    /// ppem, without drawing it.
    ///
    /// This is the width [`draw`](Self::draw) reports, arrived at without
    /// the drawing.
    pub fn advance(&self, gid: GlyphId, ppem: Option<f32>) -> Result<Option<f32>, Error> {
        self.default_instance().advance(gid, ppem)
    }

    /// Measures the box the glyph's drawing stays within, in design units.
    ///
    /// A control box: a curve counts through its control points rather than
    /// the path they describe, so one bulging inside its hull measures larger
    /// than it draws. HarfBuzz reports a charstring's extents the same way.
    ///
    /// The font matrix is applied and no size is, so the box is exact and in
    /// the units the rest of the font is measured in.
    /// [`extents`](Self::extents) applies a size as well and answers in what
    /// a caller laying out text works in.
    ///
    /// Unlike [`evaluate_charstring`], the matrix is applied here rather than
    /// left to a caller. That one withholds it so a hinter can be slotted in
    /// before it; nothing hints a box, and a box cannot be brought through a
    /// matrix afterwards anyway, because turning a box turns its corners and
    /// a matrix that shears tilts the drawing inside a box that has to grow
    /// to hold them.
    ///
    /// `None` where the glyph draws nothing.
    ///
    /// [`evaluate_charstring`]: Self::evaluate_charstring
    pub fn evaluate_extents(&self, gid: GlyphId) -> Result<Option<BoundingBox<Fixed>>, Error> {
        self.default_instance().evaluate_extents(gid)
    }

    /// Returns the box the glyph's drawing stays within, with an optional
    /// size in ppem.
    ///
    /// The box [`draw`](Self::draw) would stay within, measured as
    /// [`evaluate_extents`](Self::evaluate_extents) measures it and scaled by
    /// the size as well.
    ///
    /// A size can be applied to a finished box, being one scale in both
    /// directions; it is applied here so the answer matches `draw` at that
    /// size without a caller doing the arithmetic. The matrix cannot, which
    /// is why neither of these leaves it out.
    ///
    /// `None` where the glyph draws nothing.
    pub fn extents(
        &self,
        gid: GlyphId,
        ppem: Option<f32>,
    ) -> Result<Option<BoundingBox<f32>>, Error> {
        self.default_instance().extents(gid, ppem)
    }

    /// Draws the glyph with an optional size in ppem to the given pen.
    ///
    /// Returns the advance width of the glyph if the charstring provides
    /// one.
    pub fn draw(
        &self,
        gid: GlyphId,
        ppem: Option<f32>,
        sink: &mut impl PathSink,
    ) -> Result<Option<f32>, Error> {
        self.default_instance().draw(gid, ppem, sink)
    }
}

impl<'a> Type1Instance<'a> {
    /// Returns the font.
    pub fn font(&self) -> &'a Type1Font {
        self.font
    }

    /// Returns the weight of each master.
    pub fn weights(&self) -> &'a [Fixed] {
        self.weights
    }

    /// As [`Type1Font::evaluate_width`], with this instance's blend.
    pub fn evaluate_width(&self, gid: GlyphId) -> Result<Option<Fixed>, Error> {
        let charstring_data = self
            .font
            .charstrings
            .get(gid.to_u32())
            .ok_or(Error::Malformed)?;
        let transform = self.font.transform(None);
        Ok(charstring::evaluate_width(self, None, charstring_data)?
            .map(|width| transform.transform_h_metric(width)))
    }

    /// As [`Type1Font::evaluate_charstring`], with this instance's blend.
    pub fn evaluate_charstring(
        &self,
        gid: GlyphId,
        sink: &mut impl CommandSink,
    ) -> Result<Option<Fixed>, Error> {
        let charstring_data = self
            .font
            .charstrings
            .get(gid.to_u32())
            .ok_or(Error::Malformed)?;
        charstring::evaluate(self, None, charstring_data, sink)
    }

    /// As [`Type1Font::advance`], with this instance's blend.
    pub fn advance(&self, gid: GlyphId, ppem: Option<f32>) -> Result<Option<f32>, Error> {
        let charstring_data = self
            .font
            .charstrings
            .get(gid.to_u32())
            .ok_or(Error::Malformed)?;
        let transform = self.font.transform(ppem);
        Ok(charstring::evaluate_width(self, None, charstring_data)?
            .map(|width| transform.transform_h_metric(width).to_f32().max(0.0)))
    }

    /// As [`Type1Font::evaluate_extents`], with this instance's blend.
    pub fn evaluate_extents(&self, gid: GlyphId) -> Result<Option<BoundingBox<Fixed>>, Error> {
        let mut bounds = charstring::ControlBoundsSink::new();
        let mut nop_filter = charstring::NopFilterSink::new(&mut bounds);
        let transform = self.font.transform(None);
        let mut transformer = charstring::TransformSink::new(&mut nop_filter, transform);
        self.evaluate_charstring(gid, &mut transformer)?;
        Ok(bounds.bounding_box())
    }

    /// As [`Type1Font::extents`], with this instance's blend.
    pub fn extents(
        &self,
        gid: GlyphId,
        ppem: Option<f32>,
    ) -> Result<Option<BoundingBox<f32>>, Error> {
        let mut bounds = charstring::ControlBoundsSink::new();
        let mut nop_filter = charstring::NopFilterSink::new(&mut bounds);
        let transform = self.font.transform(ppem);
        let mut transformer = charstring::TransformSink::new(&mut nop_filter, transform);
        self.evaluate_charstring(gid, &mut transformer)?;
        Ok(bounds.bounding_box().map(|b| BoundingBox {
            x_min: b.x_min.to_f32(),
            y_min: b.y_min.to_f32(),
            x_max: b.x_max.to_f32(),
            y_max: b.y_max.to_f32(),
        }))
    }

    /// As [`Type1Font::draw`], with this instance's blend.
    pub fn draw(
        &self,
        gid: GlyphId,
        ppem: Option<f32>,
        sink: &mut impl PathSink,
    ) -> Result<Option<f32>, Error> {
        let mut nop_filter = NopFilterSink::new(sink);
        let transform = self.font.transform(ppem);
        let mut transformer = TransformSink::new(&mut nop_filter, transform);
        let width = self.evaluate_charstring(gid, &mut transformer)?;
        Ok(width.map(|w| transform.transform_h_metric(w).to_f32().max(0.0)))
    }
}

impl CharstringContext for Type1Font {
    fn kind(&self) -> CharstringKind {
        CharstringKind::Type1
    }

    fn seac_components(&self, base_code: i32, accent_code: i32) -> Result<[&[u8]; 2], Error> {
        let decode = |code: i32| {
            let name = PredefinedEncoding::Standard
                .name(code.try_into().map_err(|_| Error::InvalidOperation)?);
            self.charstrings
                .index_for_name(name)
                .and_then(|idx| self.charstrings.get(idx))
                .ok_or(Error::InvalidOperation)
        };
        let base = decode(base_code)?;
        let accent = decode(accent_code)?;
        Ok([base, accent])
    }

    fn subr(&self, index: i32) -> Result<&[u8], Error> {
        self.subrs.get(index as u32).ok_or(Error::Malformed)
    }

    fn global_subr(&self, _index: i32) -> Result<&[u8], Error> {
        // Type1 fonts don't have global subroutines
        Err(Error::MissingResource)
    }

    fn weight_vector(&self) -> &[Fixed] {
        &self.weight_vector
    }

    fn build_char_len(&self) -> usize {
        self.build_char_len
    }
}

impl CharstringContext for Type1Instance<'_> {
    fn kind(&self) -> CharstringKind {
        CharstringKind::Type1
    }

    fn seac_components(&self, base_code: i32, accent_code: i32) -> Result<[&[u8]; 2], Error> {
        self.font.seac_components(base_code, accent_code)
    }

    fn subr(&self, index: i32) -> Result<&[u8], Error> {
        self.font.subr(index)
    }

    fn global_subr(&self, index: i32) -> Result<&[u8], Error> {
        self.font.global_subr(index)
    }

    fn weight_vector(&self) -> &[Fixed] {
        self.weights
    }

    fn build_char_len(&self) -> usize {
        self.font.build_char_len
    }
}

/// The most axes a multiple master font can have.
const MAX_MASTER_AXES: usize = 4;

/// The most masters a multiple master font can have.
const MAX_MASTERS: usize = 16;

/// The most points in the design map of one axis.
const MAX_MAP_POINTS: usize = 20;

/// The blend dictionaries of a font as they are read, before they are
/// checked against each other.
///
/// Each `add` fails where FreeType refuses the whole font: a count out of
/// range, a count that disagrees with one already read, or a second design
/// map. A later axis type or position list replaces an earlier one, as in
/// FreeType.
///
/// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L764>
#[derive(Default)]
struct RawBlend {
    num_axes: usize,
    num_masters: usize,
    axis_names: Vec<String>,
    positions: bool,
    maps: Vec<Vec<(i32, Fixed)>>,
    weights: Vec<Fixed>,
    build_char_len: usize,
}

impl RawBlend {
    fn set_num_axes(&mut self, num_axes: usize) -> Option<()> {
        if num_axes == 0 || num_axes > MAX_MASTER_AXES {
            return None;
        }
        if self.num_axes != 0 && self.num_axes != num_axes {
            return None;
        }
        self.num_axes = num_axes;
        Some(())
    }

    fn set_num_masters(&mut self, num_masters: usize) -> Option<()> {
        if num_masters == 0 || num_masters > MAX_MASTERS {
            return None;
        }
        if self.num_masters != 0 && self.num_masters != num_masters {
            return None;
        }
        self.num_masters = num_masters;
        Some(())
    }

    fn add_axis_types(&mut self, names: Vec<String>) -> Option<()> {
        self.set_num_axes(names.len())?;
        self.axis_names = names;
        Some(())
    }

    fn add_design_positions(&mut self, positions: Vec<Vec<Fixed>>) -> Option<()> {
        self.set_num_masters(positions.len())?;
        let num_axes = positions.first()?.len();
        if positions.iter().any(|position| position.len() != num_axes) {
            return None;
        }
        self.set_num_axes(num_axes)?;
        // Only their presence matters: a font's masters sit at the corners
        // of the blend space, which the weights are computed for.
        self.positions = true;
        Some(())
    }

    fn add_design_maps(&mut self, maps: Vec<Vec<(i32, Fixed)>>) -> Option<()> {
        if !self.maps.is_empty() {
            return None;
        }
        self.set_num_axes(maps.len())?;
        if maps
            .iter()
            .any(|map| map.is_empty() || map.len() > MAX_MAP_POINTS)
        {
            return None;
        }
        self.maps = maps;
        Some(())
    }

    fn add_weights(&mut self, weights: Vec<Fixed>) -> Option<()> {
        self.set_num_masters(weights.len())?;
        self.weights = weights;
        Some(())
    }

    /// Returns the design space and the default weights, or `None` where
    /// FreeType sets the blend aside and treats the font as an ordinary one.
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2567>
    fn finish(&self) -> Option<(MultipleMaster, Vec<Fixed>)> {
        // Only masters at the corners of the blend space are supported.
        if self.num_axes == 0 || self.num_masters != 1 << self.num_axes {
            return None;
        }
        if self.weights.is_empty() || !self.positions || self.maps.len() != self.num_axes {
            return None;
        }
        let axes = self
            .maps
            .iter()
            .enumerate()
            .map(|(index, map)| MasterAxis {
                name: self.axis_names.get(index).cloned(),
                design_points: map.iter().map(|point| point.0).collect(),
                blend_points: map.iter().map(|point| point.1).collect(),
            })
            .collect();
        let multiple_master = MultipleMaster {
            axes,
            num_masters: self.num_masters,
            default_weights: self.weights.clone(),
        };
        Some((multiple_master, self.weights.clone()))
    }
}

/// The design space of a multiple master font.
///
/// A multiple master font holds two to sixteen masters at the corners of a
/// blend space with one to four axes. An instance is a weighted blend of
/// the masters, and its weights come from a position in that space, given
/// as blend coordinates from 0 to 1 on each axis or as design coordinates,
/// which each axis maps to blend coordinates.
///
/// The conversions follow FreeType's, including where its arithmetic
/// departs from the specification, so that an instance draws as it does
/// in FreeType.
#[derive(Clone, Debug)]
pub struct MultipleMaster {
    axes: Vec<MasterAxis>,
    num_masters: usize,
    default_weights: Vec<Fixed>,
}

/// One axis of a multiple master font.
#[derive(Clone, Debug)]
pub struct MasterAxis {
    name: Option<String>,
    design_points: Vec<i32>,
    blend_points: Vec<Fixed>,
}

impl MasterAxis {
    /// Returns the name the font gives the axis, such as `Weight`.
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    /// Returns the lowest design coordinate.
    pub fn min_design(&self) -> i32 {
        self.design_points.first().copied().unwrap_or_default()
    }

    /// Returns the highest design coordinate.
    pub fn max_design(&self) -> i32 {
        self.design_points.last().copied().unwrap_or_default()
    }

    /// Returns the map from design coordinates to blend coordinates, as
    /// pairs in the font's order.
    pub fn design_map(&self) -> impl Iterator<Item = (i32, Fixed)> + '_ {
        self.design_points
            .iter()
            .copied()
            .zip(self.blend_points.iter().copied())
    }

    /// Returns the blend coordinate for a design coordinate.
    ///
    /// This follows FreeType's `T1_Set_MM_Design`, which interpolates
    /// between two points as though the first mapped to a blend of zero.
    /// The result is exact for a map whose interpolated points start at
    /// zero, as two point maps do.
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L555>
    pub fn design_to_blend(&self, design: i32) -> Fixed {
        let designs = &self.design_points;
        let blends = &self.blend_points;
        let mut before = None;
        let mut after = None;
        for (p, &p_design) in designs.iter().enumerate() {
            if design == p_design {
                return blends[p];
            }
            if design < p_design {
                after = Some(p);
                break;
            }
            before = Some(p);
        }
        match (before, after) {
            (None, _) => blends.first().copied().unwrap_or_default(),
            (Some(_), None) => blends.last().copied().unwrap_or_default(),
            (Some(before), Some(after)) => {
                // FreeType leaves out `+ blends[before]`.
                Fixed::from_bits(saturate(mul_div_wide(
                    i64::from(design) - i64::from(designs[before]),
                    i64::from(blends[after].to_bits()) - i64::from(blends[before].to_bits()),
                    i64::from(designs[after]) - i64::from(designs[before]),
                )))
            }
        }
    }

    /// Returns the design coordinate for a blend coordinate, a whole number.
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L218>
    pub fn blend_to_design(&self, blend: Fixed) -> i32 {
        let designs = &self.design_points;
        let blends = &self.blend_points;
        if designs.is_empty() || blend <= blends[0] {
            return self.min_design();
        }
        for j in 1..designs.len() {
            if blend <= blends[j] {
                let offset = mul_div_wide(
                    i64::from(blend.to_bits()) - i64::from(blends[j - 1].to_bits()),
                    i64::from(designs[j]) - i64::from(designs[j - 1]),
                    i64::from(blends[j].to_bits()) - i64::from(blends[j - 1].to_bits()),
                );
                return saturate(i64::from(designs[j - 1]) + offset);
            }
        }
        self.max_design()
    }
}

/// Returns `a * b / c`, rounded as [`Fixed::mul_div`] rounds, without
/// overflowing on the way.
///
/// Design maps hold any `i32`, so the differences between their points
/// don't fit in one.
fn mul_div_wide(a: i64, b: i64, c: i64) -> i64 {
    let (a_abs, b_abs, c_abs) = (
        u128::from(a.unsigned_abs()),
        u128::from(b.unsigned_abs()),
        u128::from(c.unsigned_abs()),
    );
    let result = if c_abs > 0 {
        (a_abs * b_abs + (c_abs >> 1)) / c_abs
    } else {
        0x7FFFFFFF
    };
    let result = result.min(i64::MAX as u128) as i64;
    if (a < 0) ^ (b < 0) ^ (c < 0) {
        -result
    } else {
        result
    }
}

/// Clamps `value` to the range of an `i32`.
fn saturate(value: i64) -> i32 {
    value.clamp(i32::MIN.into(), i32::MAX.into()) as i32
}

impl MultipleMaster {
    /// Returns the axes.
    pub fn axes(&self) -> &[MasterAxis] {
        &self.axes
    }

    /// Returns the number of masters, which is two to the power of the
    /// number of axes.
    pub fn num_masters(&self) -> usize {
        self.num_masters
    }

    /// Returns the weights of the font's default instance, one per master.
    pub fn default_weights(&self) -> &[Fixed] {
        &self.default_weights
    }

    /// Writes the blend coordinates of the font's default instance to
    /// `blend`, one per axis.
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L248>
    pub fn default_blend(&self, blend: &mut [Fixed]) {
        self.weights_to_blend(&self.default_weights, blend);
    }

    /// Writes the blend coordinates that give rise to `weights` to `blend`,
    /// one per axis.
    ///
    /// The blend on an axis is the sum of the weights of the masters at its
    /// far end.
    pub fn weights_to_blend(&self, weights: &[Fixed], blend: &mut [Fixed]) {
        for (axis, value) in blend.iter_mut().enumerate().take(self.axes.len()) {
            *value = Fixed::from_bits(
                (0..self.num_masters)
                    .filter(|master| master & (1 << axis) != 0)
                    .filter_map(|master| weights.get(master))
                    .fold(0i32, |sum, weight| sum.wrapping_add(weight.to_bits())),
            );
        }
    }

    /// Writes the weights for the blend coordinates `blend` to `weights`, one
    /// per master.
    ///
    /// An axis without a coordinate takes the middle of its range.
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L383>
    pub fn blend_to_weights(&self, blend: &[Fixed], weights: &mut [Fixed]) {
        for (master, weight) in weights.iter_mut().enumerate().take(self.num_masters) {
            let mut result = Fixed::ONE;
            for axis in 0..self.axes.len() {
                let Some(&coord) = blend.get(axis) else {
                    result = Fixed::from_bits(result.to_bits() >> 1);
                    continue;
                };
                let factor = if master & (1 << axis) == 0 {
                    Fixed::ONE - coord
                } else {
                    coord
                };
                if factor <= Fixed::ZERO {
                    result = Fixed::ZERO;
                    break;
                } else if factor >= Fixed::ONE {
                    continue;
                }
                result *= factor;
            }
            *weight = result;
        }
    }

    /// Writes the blend coordinates for the design coordinates `design` to
    /// `blend`, one per axis.
    ///
    /// Each design coordinate is rounded to a whole number first. An axis
    /// without a coordinate takes half its range above zero, `(max - min) /
    /// 2`, which is the middle only of a range that starts at zero. Both
    /// follow FreeType's `T1_Set_Var_Design`, as does
    /// [`MasterAxis::design_to_blend`].
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L653>
    pub fn design_to_blend(&self, design: &[Fixed], blend: &mut [Fixed]) {
        for (index, (axis, value)) in self.axes.iter().zip(blend.iter_mut()).enumerate() {
            let design = match design.get(index) {
                // FreeType's `FIXED_TO_INT`, which rounds a half away from
                // zero.
                Some(design) => {
                    let bits = design.to_bits();
                    bits.wrapping_add(0x8000 - i32::from(bits < 0)) >> 16
                }
                None => saturate((i64::from(axis.max_design()) - i64::from(axis.min_design())) / 2),
            };
            *value = axis.design_to_blend(design);
        }
    }
}

/// A Type 1 font drawn with a given blend of its masters.
///
/// For a multiple master font, the weights give the blend, one per master.
/// [`MultipleMaster::blend_to_weights`] computes them for a position in the
/// blend space. A font with no masters takes empty weights.
#[derive(Clone, Copy)]
pub struct Type1Instance<'a> {
    font: &'a Type1Font,
    weights: &'a [Fixed],
}

/// Associates character codes with glyph names and ids.
#[derive(Clone)]
pub struct Encoding<'a> {
    encoding: &'a RawEncoding,
    charstrings: &'a Charstrings,
}

impl<'a> Encoding<'a> {
    /// Returns the predefined encoding, if any.
    pub fn predefined(&self) -> Option<PredefinedEncoding> {
        if let RawEncoding::Predefined(pre) = self.encoding {
            Some(*pre)
        } else {
            None
        }
    }

    /// Returns the glyph name for the given character code.
    pub fn glyph_name(&self, code: u8) -> Option<&'a str> {
        match self.encoding {
            RawEncoding::Predefined(pre) => Some(pre.name(code)),
            RawEncoding::Custom(custom) => {
                self.charstrings.name(custom.get(code as usize)?.to_u32())
            }
        }
    }

    /// Maps a character code to a glyph identifier.
    pub fn map(&self, code: u8) -> Option<GlyphId> {
        match self.encoding {
            RawEncoding::Predefined(pre) => self
                .charstrings
                .index_for_name(pre.name(code))
                .map(GlyphId::new),
            RawEncoding::Custom(custom) => custom.get(code as usize).copied(),
        }
    }
}

/// Raw dictionary data for a Type1 font.
struct RawDicts<'a> {
    /// Data containing the base dicitionary.
    base: &'a [u8],
    /// Data containing the decrypted private dictionary.
    private: Vec<u8>,
}

impl<'a> RawDicts<'a> {
    fn new(data: &'a [u8]) -> Option<Self> {
        if let Some((PFB_TEXT_SEGMENT_TAG, base_size)) = decode_pfb_tag(data, 0) {
            // We have a PFB; skip the tag
            let data = data.get(6..)?;
            verify_header(data)?;
            let (base_dict, raw_private_dict) = data.split_at_checked(base_size as usize)?;
            // Decrypt private dict segments
            let private_dict = decrypt(
                decode_pfb_binary_segments(raw_private_dict)
                    .flat_map(|segment| segment.iter().copied()),
                EEXEC_SEED,
            )
            // First four bytes are random garbage
            .skip(4)
            .collect::<Vec<_>>();
            Some(Self {
                base: base_dict,
                private: private_dict,
            })
        } else {
            // We have a PFA
            verify_header(data)?;
            // Now find the start of the private dictionary
            let start = find_eexec_data(data)?;
            let (base_dict, raw_private_dict) = data.split_at_checked(start)?;
            let private_dict = if raw_private_dict.len() > 3
                && raw_private_dict[..4].iter().all(|b| b.is_ascii_hexdigit())
            {
                // Hex decode and then decrypt
                decrypt(decode_hex(raw_private_dict.iter().copied()), EEXEC_SEED)
                    .skip(4)
                    .collect::<Vec<_>>()
            } else {
                // Just decrypt
                decrypt(raw_private_dict.iter().copied(), EEXEC_SEED)
                    .skip(4)
                    .collect::<Vec<_>>()
            };
            Some(Self {
                base: base_dict,
                private: private_dict,
            })
        }
    }
}

fn verify_header(data: &[u8]) -> Option<()> {
    (data.starts_with(b"%!PS-AdobeFont") || data.starts_with(b"%!FontType")).then_some(())
}

const PFB_TEXT_SEGMENT_TAG: u16 = 0x8001;
const PFB_BINARY_SEGMENT_TAG: u16 = 0x8002;

/// Returns the PFB tag and segment size.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1parse.c#L69>
fn decode_pfb_tag(data: &[u8], start: usize) -> Option<(u16, u32)> {
    let header: [u8; 6] = data.get(start..start + 6)?.try_into().ok()?;
    let tag = ((header[0] as u16) << 8) | header[1] as u16;
    if matches!(tag, PFB_BINARY_SEGMENT_TAG | PFB_TEXT_SEGMENT_TAG) {
        let size = u32::from_le_bytes(header[2..].try_into().unwrap());
        Some((tag, size))
    } else {
        None
    }
}

/// Returns an iterator over the sequence of PFB binary segments.
fn decode_pfb_binary_segments(data: &[u8]) -> impl Iterator<Item = &[u8]> + '_ {
    let mut pos = 0usize;
    core::iter::from_fn(move || {
        let (tag, len) = decode_pfb_tag(data, pos)?;
        // FT only decodes the sequence of binary segments here
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1parse.c#L286>
        if tag != PFB_BINARY_SEGMENT_TAG {
            return None;
        }
        // Skip tag and size bytes
        let start = pos + 6;
        let end = start + len as usize;
        let segment = data.get(start..end)?;
        pos = end;
        Some(segment)
    })
}

/// Helper to find the position of the data following the 'eexec' token.
///
/// Unsurprisingly, more complicated than it should be.
fn find_eexec_data(data: &[u8]) -> Option<usize> {
    // Use a parser to avoid catching "eexec" in a comment or string
    // which apparently occurs in some fonts.
    let mut parser = Parser::new(data);
    while let Some(token) = parser.next() {
        if token != Token::Raw(b"eexec") {
            continue;
        }
        let mut start = parser.pos;
        // FreeType has some unfun logic for skipping whitespace
        // after the eexec token
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1parse.c#L382>
        let mut linefeed_pos = None;
        while start < data.len() {
            match data[start] {
                b' ' | b'\t' => {}
                b'\n' => linefeed_pos = Some(start),
                b'\r' => {
                    // If we've already seen \n or there is not a \n later
                    // in the data, then stop at this \r
                    if *linefeed_pos.get_or_insert_with(|| {
                        data[start..]
                            .iter()
                            .position(|b| *b == b'\n')
                            .map(|pos| pos + start)
                            .unwrap_or(0)
                    }) < start
                    {
                        break;
                    }
                }
                _ => break,
            }
            start += 1;
        }
        if start == data.len() {
            // eexec not properly terminated
            return None;
        }
        return Some(start);
    }
    None
}

/// Converts hex formatted data to associated bytes.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L464>
fn decode_hex(mut bytes: impl Iterator<Item = u8>) -> impl Iterator<Item = u8> {
    /// Converts digits (as ASCII characters) into integer values.
    const DIGIT_TO_NUM: [i8; 128] = [
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1,
        -1, -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
    ];
    let mut pad = 0x1_u32;
    core::iter::from_fn(move || {
        loop {
            let Some(c) = bytes.next() else {
                break;
            };
            if is_whitespace(c) {
                continue;
            }
            if c >= 0x80 {
                break;
            }
            let c = DIGIT_TO_NUM[(c & 0x7F) as usize] as u32;
            if c >= 16 {
                break;
            }
            pad = (pad << 4) | c;
            if pad & 0x100 != 0 {
                let res = pad as u8;
                pad = 0x1;
                return Some(res);
            } else {
                continue;
            }
        }
        if pad != 0x1 {
            let res = (pad << 4) as u8;
            pad = 0x1;
            return Some(res);
        }
        None
    })
}

/// Decryption seed for eexec segment.
const EEXEC_SEED: u32 = 55665;

/// Decryption seed for charstring (and subroutine) data.
const CHARSTRING_SEED: u32 = 4330;

/// Returns an iterator yielding the decrypted bytes.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L557>
fn decrypt(bytes: impl Iterator<Item = u8>, mut seed: u32) -> impl Iterator<Item = u8> {
    bytes.map(move |b| {
        let b = b as u32;
        let plain = b ^ (seed >> 8);
        seed = b.wrapping_add(seed).wrapping_mul(52845).wrapping_add(22719) & 0xFFFF;
        plain as u8
    })
}

fn is_whitespace(c: u8) -> bool {
    if c <= 32 {
        return matches!(c, b' ' | b'\n' | b'\r' | b'\t' | b'\0' | 0x0C);
    }
    false
}

/// Characters that always delimit tokens.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/include/freetype/internal/psaux.h#L1398>
fn is_special(c: u8) -> bool {
    matches!(
        c,
        b'(' | b')' | b'<' | b'>' | b'[' | b']' | b'{' | b'}' | b'/' | b'%'
    )
}

fn is_special_or_whitespace(c: u8) -> bool {
    is_special(c) || is_whitespace(c)
}

/// Preserve valid UTF-8 names; otherwise decode each source byte as Latin-1.
fn decode_name(bytes: &[u8]) -> Cow<'_, str> {
    match core::str::from_utf8(bytes) {
        Ok(value) => Cow::Borrowed(value),
        Err(_) => Cow::Owned(bytes.iter().copied().map(char::from).collect()),
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
enum Token<'a> {
    /// Integers
    Int(i64),
    /// Literal strings, delimited by ()
    LitString(&'a [u8]),
    /// Hex strings, delimited by <>
    HexString(&'a [u8]),
    /// Procedures, delimited by {}
    Proc(&'a [u8]),
    /// Binary blobs
    Binary(&'a [u8]),
    /// Names, preceded by /
    Name(&'a [u8]),
    /// All other raw tokens (identifiers and self-delimiting punctuation)
    Raw(&'a [u8]),
}

/// Collection of subroutines.
#[derive(Default)]
struct Subrs {
    /// Packed data for all subroutines.
    data: Vec<u8>,
    /// Index mapping subroutine number to range in the packed data. Sorted
    /// by subroutine number.
    index: Vec<(u32, Range<usize>)>,
    /// If true, subroutine number == index so we don't need to
    /// bsearch.
    is_dense: bool,
}

impl Subrs {
    fn get(&self, index: u32) -> Option<&[u8]> {
        let entry_idx = if self.is_dense {
            index as usize
        } else {
            self.index.binary_search_by_key(&index, |e| e.0).ok()?
        };
        self.data.get(self.index.get(entry_idx)?.1.clone())
    }
}

struct CharstringEntry {
    name: Range<usize>,
    data: Range<usize>,
}

/// Collection of charstrings.
#[derive(Default)]
struct Charstrings {
    /// Packed data for all charstrings.
    data: Vec<u8>,
    /// Packed data for all glyph names.
    names: Vec<u8>,
    /// Index containing all charstrings.
    index: Vec<CharstringEntry>,
    /// If notdef was remapped, holds the original index of the notdef
    /// charstring.
    orig_notdef_index: Option<usize>,
}

impl Charstrings {
    fn num_glyphs(&self) -> u32 {
        self.index.len() as u32
    }

    fn get(&self, index: u32) -> Option<&[u8]> {
        self.data.get(self.index.get(index as usize)?.data.clone())
    }

    fn name(&self, index: u32) -> Option<&str> {
        core::str::from_utf8(
            self.names
                .get(self.index.get(index as usize)?.name.clone())?,
        )
        .ok()
    }

    fn index_for_name(&self, name: &str) -> Option<u32> {
        let name = name.as_bytes();
        for (idx, entry) in self.index.iter().enumerate() {
            if self.names.get(entry.name.clone()) == Some(name) {
                return Some(idx as u32);
            }
        }
        None
    }

    fn push(&mut self, name: &[u8], data: &[u8], len_iv: i64) {
        let start = self.data.len();
        if len_iv >= 0 {
            // use decryption; skip first len_iv bytes
            self.data
                .extend(decrypt(data.iter().copied(), CHARSTRING_SEED).skip(len_iv as usize));
        } else {
            // just add the data
            self.data.extend_from_slice(data);
        }
        let end = self.data.len();
        let name_start = self.names.len();
        self.names.extend_from_slice(decode_name(name).as_bytes());
        let name_end = self.names.len();
        self.index.push(CharstringEntry {
            name: name_start..name_end,
            data: start..end,
        });
    }
}

/// Encoding that maps characters to glyph identifiers.
#[derive(PartialEq, Debug)]
enum RawEncoding {
    Predefined(PredefinedEncoding),
    Custom(Vec<GlyphId>),
}

/// Simulated .notdef glyph, same as FreeType:
///
/// 0 333 hsbw endchar
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2192>
const NOTDEF_GLYPH: &[u8] = &[0x8B, 0xF7, 0xE1, 0x0D, 0x0E];

#[derive(Clone)]
struct Parser<'a> {
    data: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(data: &'a [u8]) -> Self {
        Self { data, pos: 0 }
    }

    fn next(&mut self) -> Option<Token<'a>> {
        // Roughly follows the logic of ps_parser_skip_PS_token
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psobjs.c#L482>
        loop {
            self.skip_whitespace()?;
            let start = self.pos;
            let c = self.next_byte()?;
            match c {
                // Line comment
                b'%' => self.skip_line(),
                // Procedures
                b'{' => return self.read_proc(start),
                // Literal strings
                b'(' => return self.read_lit_string(start),
                b'<' => {
                    if self.peek_byte() == Some(b'<') {
                        // Just ignore these
                        self.pos += 1;
                        continue;
                    }
                    // Hex string: hex digits and whitespace
                    return self.read_hex_string(start);
                }
                b'>' => {
                    // We consume single '>' when parsing hex strings so a
                    // double >> is expected here
                    if self.next_byte()? != b'>' {
                        return None;
                    }
                }
                // Name
                b'/' => {
                    if let Some(c) = self.peek_byte() {
                        if is_whitespace(c) || is_special(c) {
                            if !is_special(c) {
                                self.pos += 1;
                            }
                            return Some(Token::Name(&[]));
                        } else {
                            let count = self.skip_until(|c| is_whitespace(c) || is_special(c));
                            return self.data.get(start + 1..start + count).map(Token::Name);
                        }
                    }
                }
                // Brackets
                b'[' | b']' => {
                    let data = self.data.get(start..start + 1)?;
                    return Some(Token::Raw(data));
                }
                _ => {
                    let count = self.skip_until(is_special_or_whitespace);
                    let content = self.data.get(start..start + count)?;
                    // Look for numbers but don't try to parse fractional
                    // values since we want to handle those with special
                    // precision
                    if (c.is_ascii_digit() || c == b'-') && !content.contains(&b'.') {
                        if let Some(int) = decode_int(content) {
                            // HACK: if we have an int followed by RD or -|
                            // then is a binary blob in Type1. Hack because
                            // this is not actually how PostScript works
                            // but Type1 fonts define /RD procs and this
                            // pattern is used by FreeType.
                            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1351>
                            if self.accept_blob_start() {
                                // skip a single space
                                self.pos += 1;
                                // read the internal data
                                let data = self.read_bytes(int as usize)?;
                                // there's often some form of terminator here
                                // but let the calling code handle it because
                                // some buggy fonts may omit it
                                return Some(Token::Binary(data));
                            }
                            return Some(Token::Int(int));
                        }
                    }
                    return Some(Token::Raw(content));
                }
            }
        }
    }

    /// Special lookahead case for RD or -| which indicates a binary blob in
    /// Type1 fonts.
    fn accept_blob_start(&mut self) -> bool {
        let mut p = self.clone();
        p.skip_whitespace();
        let start = p.pos;
        p.skip_until(is_special_or_whitespace);
        let end = p.pos;
        if matches!(self.data.get(start..end), Some(b"RD") | Some(b"-|")) {
            self.pos = end;
            true
        } else {
            false
        }
    }

    fn accept(&mut self, token: Token) -> bool {
        let mut p = self.clone();
        if p.next() == Some(token) {
            self.pos = p.pos;
            true
        } else {
            false
        }
    }

    fn expect(&mut self, token: Token) -> Option<()> {
        (self.next()? == token).then_some(())
    }

    fn next_byte(&mut self) -> Option<u8> {
        let byte = self.peek_byte()?;
        self.pos += 1;
        Some(byte)
    }

    fn peek_byte(&self) -> Option<u8> {
        self.data.get(self.pos).copied()
    }

    fn read_bytes(&mut self, len: usize) -> Option<&'a [u8]> {
        let end = self.pos.checked_add(len)?;
        let content = self.data.get(self.pos..end)?;
        self.pos = end;
        Some(content)
    }

    fn skip_whitespace(&mut self) -> Option<()> {
        while is_whitespace(*self.data.get(self.pos)?) {
            self.pos += 1;
        }
        Some(())
    }

    fn skip_line(&mut self) {
        while let Some(c) = self.next_byte() {
            if c == b'\n' || c == b'\r' {
                break;
            }
        }
    }

    fn skip_until(&mut self, f: impl Fn(u8) -> bool) -> usize {
        let mut count = 0;
        while let Some(byte) = self.peek_byte() {
            if f(byte) {
                break;
            }
            self.pos += 1;
            count += 1;
        }
        count + 1
    }

    fn read_proc(&mut self, start: usize) -> Option<Token<'a>> {
        let mut nest_depth = 1i32;
        while let Some(c) = self.next_byte() {
            match c {
                b'{' => nest_depth = nest_depth.checked_add(1)?,
                b'}' => {
                    nest_depth -= 1;
                    if nest_depth == 0 {
                        break;
                    }
                }
                // Skip over comments
                b'%' => self.skip_line(),
                // Skip over literal strings since they can contain braces
                b'(' => {
                    self.read_lit_string(self.pos - 1)?;
                }
                _ => {}
            }
        }
        if nest_depth != 0 {
            // unterminated procedure
            return None;
        }
        let end = self.pos;
        Some(Token::Proc(self.data.get(start + 1..end - 1)?))
    }

    fn read_lit_string(&mut self, start: usize) -> Option<Token<'a>> {
        let mut nest_depth = 1i32;
        while let Some(c) = self.next_byte() {
            match c {
                b'(' => nest_depth = nest_depth.checked_add(1)?,
                b')' => {
                    nest_depth -= 1;
                    if nest_depth == 0 {
                        break;
                    }
                }
                // Escape sequence
                b'\\' => {
                    // Just eat the next byte. We only care
                    // about avoiding \( and \) anyway.
                    self.next_byte()?;
                }
                _ => {}
            }
        }
        if nest_depth != 0 {
            // unterminated string
            return None;
        }
        let end = self.pos;
        Some(Token::LitString(self.data.get(start + 1..end - 1)?))
    }

    fn read_hex_string(&mut self, start: usize) -> Option<Token<'a>> {
        while let Some(c) = self.next_byte() {
            if !is_whitespace(c) && !c.is_ascii_hexdigit() {
                break;
            }
        }
        let end = self.pos;
        if self.data.get(end - 1) != Some(&b'>') {
            // unterminated hex string
            return None;
        }
        Some(Token::HexString(self.data.get(start + 1..end - 1)?))
    }

    fn read_int(&mut self) -> Option<i64> {
        self.next().and_then(|t| match t {
            Token::Int(n) => Some(n),
            _ => None,
        })
    }

    fn read_num_as_int(&mut self) -> Option<i32> {
        match self.next()? {
            Token::Int(n) => Some(n as i32),
            // Note, FT calls PS_Conv_ToInt for fields that might contain
            // fractional bits but just ignores everything after the
            // initial integer, so we do the same
            Token::Raw(bytes) => decode_int_prefix(bytes, 0).map(|n| n.0 as i32),
            _ => None,
        }
    }

    fn read_string(&mut self) -> Option<String> {
        let bytes = match self.next()? {
            // FreeType accepts a name or a string here
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psobjs.c#L1123>
            Token::Name(bytes) | Token::LitString(bytes) => bytes,
            _ => return None,
        };
        Some(decode_name(bytes).into_owned())
    }
}

impl Parser<'_> {
    /// Parse a font matrix.
    ///
    /// Like FreeType, this is designed assuming a upem of 1000 and produces
    /// an identity matrix in that case. This is, the result is scaled such
    /// that 0.001 yields a value of 1.0.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1403>
    fn read_font_matrix(&mut self) -> Option<ScaledFontMatrix> {
        let mut components = [Fixed::ZERO; 6];
        // accept [ or { to match FreeType
        if !self.accept(Token::Raw(b"[")) {
            self.expect(Token::Raw(b"{"))?;
        }
        // read all components
        for component in &mut components {
            *component = match self.next()? {
                Token::Int(int) => Fixed::from_i32((int as i32).checked_mul(1000)?),
                Token::Raw(bytes) => decode_fixed(bytes, 3)?,
                _ => return None,
            }
        }
        // FreeType doesn't validate the closing delimiter, so just skip
        self.next()?;
        let temp_scale = components[3].abs();
        if temp_scale == Fixed::ZERO {
            return None;
        }
        let mut upem = 1000;
        if temp_scale != Fixed::ONE {
            upem = (Fixed::from_bits(1000) / temp_scale).to_bits();
            components[0] /= temp_scale;
            components[1] /= temp_scale;
            components[2] /= temp_scale;
            // don't scale components[3]
            components[4] /= temp_scale;
            components[5] /= temp_scale;
            if components[3] < Fixed::ZERO {
                components[3] = -Fixed::ONE;
            } else {
                components[3] = Fixed::ONE;
            }
        }
        // offsets must be expressed in integer font units
        for offset in components.iter_mut().skip(4) {
            *offset = Fixed::from_bits(offset.to_bits() >> 16);
        }
        let matrix = FontMatrix::from_elements(components);
        if transform::is_degenerate(&matrix) {
            return None;
        }
        Some(ScaledFontMatrix {
            matrix,
            scale: upem,
        })
    }

    /// Parse the set of subroutines.
    ///
    /// The `len_iv` parameter defines the number of prefix padding bytes for
    /// encrypted data. If < 0, then the data is not encrypted.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1720>
    fn read_subrs(&mut self, len_iv: i64) -> Option<Subrs> {
        let mut subrs = Subrs::default();
        let _: usize = match self.next()? {
            Token::Raw(b"[") => {
                // Just an empty array
                self.expect(Token::Raw(b"]"))?;
                return Some(subrs);
            }
            Token::Int(n) => n.try_into().ok()?,
            _ => return None,
        };
        self.expect(Token::Raw(b"array"))?;
        let mut is_dense = true;
        // The pattern for each subroutine is `dup <subr_num> <data>`
        while self.accept(Token::Raw(b"dup")) {
            let (Token::Int(n), Token::Binary(data)) = (self.next()?, self.next()?) else {
                return None;
            };
            // Skip the NP, | or noaccess. FreeType just skips whatever happens
            // to be here
            self.next();
            // There might be an additional put token following the binary data
            self.accept(Token::Raw(b"put"));
            let subr_num: u32 = n.try_into().ok()?;
            if subr_num as usize != subrs.index.len() {
                is_dense = false;
            }
            let start = subrs.data.len();
            if len_iv >= 0 {
                // use decryption; skip first len_iv bytes
                subrs
                    .data
                    .extend(decrypt(data.iter().copied(), CHARSTRING_SEED).skip(len_iv as usize));
            } else {
                // just add the data
                subrs.data.extend_from_slice(data);
            }
            let end = subrs.data.len();
            subrs.index.push((subr_num, start..end));
        }
        // If we don't have a dense set, sort the index by number
        if !is_dense {
            subrs.index.sort_unstable_by_key(|(n, ..)| *n);
        }
        subrs.is_dense = is_dense;
        subrs.data.shrink_to_fit();
        subrs.index.shrink_to_fit();
        Some(subrs)
    }

    /// Parse the set of charstrings.
    ///
    /// The `len_iv` parameter defines the number of prefix padding bytes for
    /// encrypted data. If < 0, then the data is not encrypted.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1919>
    fn read_charstrings(&mut self, len_iv: i64) -> Option<Charstrings> {
        let mut charstrings = Charstrings::default();
        let _: usize = match self.next()? {
            Token::Int(n) => n.try_into().ok()?,
            _ => return None,
        };
        let mut notdef_idx = None;
        while let Some(token) = self.next() {
            let name = match token {
                // Stop when we find a `def` or `end` keyword.
                // The ugliness matches the FT logic to handle some malformed
                // fonts:
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L2006>
                Token::Raw(b"end") => {
                    if self
                        .peek_byte()
                        .map(is_special_or_whitespace)
                        .unwrap_or_default()
                    {
                        break;
                    } else {
                        continue;
                    }
                }
                Token::Raw(b"def") => {
                    // but ignore `def` if no charstring has been seen
                    if self
                        .peek_byte()
                        .map(is_special_or_whitespace)
                        .unwrap_or_default()
                        && !charstrings.index.is_empty()
                    {
                        break;
                    } else {
                        continue;
                    }
                }
                Token::Name(name) => name,
                _ => continue,
            };
            if name == b".notdef" {
                notdef_idx = Some(charstrings.index.len());
            }
            let Token::Binary(data) = self.next()? else {
                return None;
            };
            charstrings.push(name, data, len_iv);
        }
        match notdef_idx {
            Some(0) => {
                // .notdef found and at correct location
            }
            Some(idx) => {
                // .notdef found but at incorrect location. Swap with the
                // glyph at 0
                charstrings.index.swap(0, idx);
                charstrings.orig_notdef_index = Some(idx);
            }
            None => {
                // .notdef not found. Add it to the end and then swap with
                // the glyph at 0
                let idx = charstrings.index.len();
                charstrings.push(b".notdef", NOTDEF_GLYPH, -1);
                charstrings.index.swap(0, idx);
                charstrings.orig_notdef_index = Some(idx);
            }
        }
        charstrings.data.shrink_to_fit();
        charstrings.names.shrink_to_fit();
        charstrings.index.shrink_to_fit();
        Some(charstrings)
    }

    /// Reads a `/FontBBox`, each component rounded to a whole unit as
    /// FreeType rounds it.
    ///
    /// In the `/Blend` dictionary of a multiple master font, each component
    /// is an array with a value for each master. FreeType stores master 0's
    /// box over the font's own, so this returns master 0's values.
    fn read_font_bbox(&mut self) -> Option<[Fixed; 4]> {
        let mut bbox = [Fixed::ZERO; 4];
        // accept [ or { to match FreeType
        // Note that we parse { as a procedure so this needs some special
        // handling
        let mut parser;
        let parser = if self.accept(Token::Raw(b"[")) {
            self
        } else if let Token::Proc(proc) = self.next()? {
            parser = Parser::new(proc);
            &mut parser
        } else {
            return None;
        };
        // read all components
        for component in &mut bbox {
            let value = match parser.next()? {
                Token::Proc(masters) => Parser::new(masters).read_fixed()?,
                Token::Raw(b"[") => {
                    let first = parser.read_fixed()?;
                    while parser.next()? != Token::Raw(b"]") {}
                    first
                }
                Token::Int(int) => Fixed::from_i32(int as i32),
                Token::Raw(bytes) => decode_fixed(bytes, 0)?,
                _ => return None,
            };
            *component = round_fixed(value);
        }
        Some(bbox)
    }

    fn read_weight_vector(&mut self) -> Option<Vec<Fixed>> {
        self.accept(Token::Raw(b"["));
        let mut weights = Vec::new();
        while let Some(token) = self.next() {
            match token {
                Token::Raw(b"]") => break,
                Token::Int(val) => weights.push(Fixed::from_i32(val as _)),
                Token::Raw(raw) => weights.push(decode_fixed(raw, 0)?),
                _ => return None,
            }
        }
        Some(weights)
    }

    /// Reads a number as a fixed point value.
    fn read_fixed(&mut self) -> Option<Fixed> {
        match self.next()? {
            Token::Int(int) => Some(Fixed::from_i32(int as i32)),
            Token::Raw(raw) => decode_fixed(raw, 0),
            _ => None,
        }
    }

    /// Reads the names of the blend axes, as in `[/Weight /Width]`.
    fn read_blend_axis_types(&mut self) -> Option<Vec<String>> {
        self.expect(Token::Raw(b"["))?;
        let mut names = Vec::new();
        loop {
            match self.next()? {
                Token::Raw(b"]") => return Some(names),
                Token::Name(name) if !name.is_empty() => {
                    names.push(decode_name(name).into_owned());
                }
                _ => return None,
            }
        }
    }

    /// Reads the position of each master in the blend space, as in
    /// `[[0 0] [1 0] [0 1] [1 1]]`.
    fn read_blend_design_positions(&mut self) -> Option<Vec<Vec<Fixed>>> {
        self.expect(Token::Raw(b"["))?;
        let mut positions = Vec::new();
        loop {
            match self.next()? {
                Token::Raw(b"]") => return Some(positions),
                Token::Raw(b"[") => {
                    let mut position = Vec::new();
                    loop {
                        let mut peek = self.clone();
                        if peek.next()? == Token::Raw(b"]") {
                            *self = peek;
                            break;
                        }
                        position.push(self.read_fixed()?);
                    }
                    positions.push(position);
                }
                _ => return None,
            }
        }
    }

    /// Reads the map from design coordinates to blend coordinates for each
    /// axis, as in `[[[50 0] [1450 1]] [[50 0] [1450 1]]]`.
    fn read_blend_design_map(&mut self) -> Option<Vec<Vec<(i32, Fixed)>>> {
        self.expect(Token::Raw(b"["))?;
        let mut maps = Vec::new();
        loop {
            match self.next()? {
                Token::Raw(b"]") => return Some(maps),
                Token::Raw(b"[") => {
                    let mut map = Vec::new();
                    loop {
                        match self.next()? {
                            Token::Raw(b"]") => break,
                            Token::Raw(b"[") => {
                                // The design point is read as an integer,
                                // ignoring any fraction, as FreeType reads
                                // it.
                                let design = self.read_num_as_int()?;
                                let blend = self.read_fixed()?;
                                self.expect(Token::Raw(b"]"))?;
                                map.push((design, blend));
                            }
                            _ => return None,
                        }
                    }
                    maps.push(map);
                }
                _ => return None,
            }
        }
    }

    /// Reads an array and returns the number of elements in it.
    fn read_array_len(&mut self) -> Option<usize> {
        self.expect(Token::Raw(b"["))?;
        let mut len = 0;
        loop {
            match self.next()? {
                Token::Raw(b"]") => return Some(len),
                _ => len += 1,
            }
        }
    }

    /// Parse the encoding.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1474>
    fn read_encoding(&mut self, charstrings: &Charstrings) -> Option<RawEncoding> {
        match self.next()? {
            // Array of names where index == character code
            Token::Raw(b"[") => {
                let mut map = Vec::new();
                // Should always be 256 entries but preset values to notdef
                map.resize(256, GlyphId::NOTDEF);
                self.read_dense_encoding(|idx, name| {
                    if let Some((slot, gid)) = map
                        .get_mut(idx as usize)
                        .zip(charstrings.index_for_name(name))
                    {
                        *slot = gid.into();
                    }
                });
                Some(RawEncoding::Custom(map))
            }
            // Map of index to glyph name
            Token::Int(count) => {
                // We're limited to 256 character codes
                // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1518>
                let count: usize = count.clamp(0, 256) as usize;
                let mut map = Vec::new();
                // Start with all glyphs mapped to notdef
                map.resize(count, GlyphId::NOTDEF);
                self.read_sparse_encoding(|idx, name| {
                    if let Some((slot, gid)) = map
                        .get_mut(idx as usize)
                        .zip(charstrings.index_for_name(name))
                    {
                        *slot = gid.into();
                    }
                });
                Some(RawEncoding::Custom(map))
            }
            Token::Raw(b"StandardEncoding") => {
                Some(RawEncoding::Predefined(PredefinedEncoding::Standard))
            }
            Token::Raw(b"ExpertEncoding") => {
                Some(RawEncoding::Predefined(PredefinedEncoding::Expert))
            }
            Token::Raw(b"ISOLatin1Encoding") => {
                Some(RawEncoding::Predefined(PredefinedEncoding::IsoLatin1))
            }
            _ => None,
        }
    }

    /// Returns a custom encoding defined by an array of `/<name>`, where
    /// the index represents the character code, and invokes the given
    /// callback for each.
    fn read_dense_encoding(&mut self, mut f: impl FnMut(i64, &str)) -> Option<()> {
        // Eat the opening brace if present
        self.accept(Token::Raw(b"["));
        // Always expect 256 entries
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/type1/t1load.c#L1510>
        let mut idx = 0;
        while let Some(token) = self.next() {
            match token {
                Token::Raw(b"]") => break,
                Token::Name(name) => {
                    let code = idx;
                    idx += 1;
                    f(code, &decode_name(name));
                }
                _ => {
                    // FreeType fails if missing a literal name here
                    return None;
                }
            }
        }
        Some(())
    }

    /// Reads a custom encoding defined by a map of `<charcode> /<name>`
    /// and invokes the given callback for each.
    fn read_sparse_encoding(&mut self, mut f: impl FnMut(i64, &str)) -> Option<()> {
        while let Some(token) = self.next() {
            match token {
                // The 'def' keyword ends the mapping
                Token::Raw(b"def") => break,
                Token::Int(code) => {
                    // read the name
                    let Some(Token::Name(name)) = self.next() else {
                        continue;
                    };
                    f(code, &decode_name(name));
                }
                _ => {}
            }
        }
        Some(())
    }
}

/// Decode an integer, optionally with a base.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L161>
fn decode_int(bytes: &[u8]) -> Option<i64> {
    let s = std::str::from_utf8(bytes).ok()?;
    if let Some(hash_idx) = s.find('#') {
        if hash_idx == 1 || hash_idx == 2 {
            // It's a radix number, like 8#40.
            let radix_str = s.get(0..hash_idx)?;
            let number_str = s.get(hash_idx + 1..)?;
            let radix = radix_str
                .parse::<u32>()
                .ok()
                .filter(|n| (2..=36).contains(n))?;
            i64::from_str_radix(number_str, radix).ok()
        } else {
            s.parse::<i64>().ok()
        }
    } else {
        s.parse::<i64>().ok()
    }
}

/// Decode an integer at the given position, returning the value and the
/// index of the position following the decoded integer.
fn decode_int_prefix(bytes: &[u8], start: usize) -> Option<(i64, usize)> {
    let tail = bytes.get(start..)?;
    let end = tail
        .iter()
        .position(|c| *c != b'-' && !c.is_ascii_digit())
        .unwrap_or(tail.len());
    let int = decode_int(tail.get(..end)?)?;
    Some((int, start + end))
}

/// Rounds to a whole unit, halves away from zero, as FreeType's
/// `FT_RoundFix` does.
///
/// A value from 32767.5 up rounds to 32768, which `Fixed` can't hold, so
/// it saturates to `Fixed::MAX`. FreeType keeps 32768 in a 64-bit value;
/// both give 32768 once rounded up to whole units.
fn round_fixed(value: Fixed) -> Fixed {
    let bits = value.to_bits() as i64;
    let rounded = (bits + 0x8000 - (bits < 0) as i64) & !0xFFFF;
    Fixed::from_bits(rounded.min(i32::MAX as i64) as i32)
}

/// Decode a fixed point value, scaling to a specific power of
/// ten.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psaux/psconv.c#L195>
fn decode_fixed(bytes: &[u8], mut power_ten: i32) -> Option<Fixed> {
    const LIMIT: i32 = 0xCCCCCCC;
    let mut idx = 0;
    let &first = bytes.get(idx)?;
    let sign = if first == b'-' || first == b'+' {
        idx += 1;
        if first == b'-' {
            -1
        } else {
            1
        }
    } else {
        1
    };
    let overflow = || Some(Fixed::from_bits(0x7FFFFFFF * sign));
    let mut integral = 0;
    if *bytes.get(idx)? != b'.' {
        let (int, end_idx) = decode_int_prefix(bytes, idx)?;
        if int > 0x7FFF {
            return overflow();
        }
        integral = (int << 16) as i32;
        idx = end_idx;
    }
    let mut decimal = 0;
    let mut divider = 1;
    if bytes.get(idx) == Some(&b'.') {
        idx += 1;
        while let Some(byte) = bytes.get(idx).copied() {
            if !byte.is_ascii_digit() {
                break;
            }
            let digit = (byte - b'0') as i32;
            if divider < LIMIT && decimal < LIMIT {
                decimal = decimal * 10 + digit;
                if integral == 0 && power_ten > 0 {
                    power_ten -= 1;
                } else {
                    divider *= 10;
                }
            }
            idx += 1;
        }
    }
    if bytes.get(idx).map(|b| b.to_ascii_lowercase()) == Some(b'e') {
        idx += 1;
        let (exponent, _) = decode_int_prefix(bytes, idx)?;
        if exponent > 1000 {
            return overflow();
        } else if exponent < -1000 {
            // underflow
            return Some(Fixed::ZERO);
        } else {
            power_ten = power_ten.checked_add(exponent as i32)?;
        }
    }
    if integral == 0 && decimal == 0 {
        return Some(Fixed::ZERO);
    }
    while power_ten > 0 {
        if integral >= LIMIT {
            return overflow();
        }
        integral *= 10;
        if decimal >= LIMIT {
            if divider == 1 {
                return overflow();
            }
            divider /= 10;
        } else {
            decimal *= 10;
        }
        power_ten -= 1;
    }
    while power_ten < 0 {
        integral /= 10;
        if divider < LIMIT {
            divider *= 10;
        } else {
            decimal /= 10;
        }
        if integral == 0 && decimal == 0 {
            return Some(Fixed::ZERO);
        }
        power_ten += 1;
    }
    if decimal != 0 {
        decimal = (Fixed::from_bits(decimal) / Fixed::from_bits(divider)).to_bits();
        integral += decimal;
    }
    Some(Fixed::from_bits(integral * sign))
}

#[cfg(test)]
mod tests {
    use super::*;
    use charstring::test_helpers::*;

    #[test]
    fn pfb_tags() {
        // Text segment tag
        let data = [0x80, 0x01, 0x01, 0x02, 0x00, 0x00];
        let (tag, len) = decode_pfb_tag(&data, 0).unwrap();
        assert_eq!(tag, PFB_TEXT_SEGMENT_TAG);
        assert_eq!(len, 513);
        // Binary segment tag
        let data = [0x80, 0x02, 0x01, 0x03, 0x00, 0x00];
        let (tag, len) = decode_pfb_tag(&data, 0).unwrap();
        assert_eq!(tag, PFB_BINARY_SEGMENT_TAG);
        assert_eq!(len, 769);
        // Invalid tag
        let data = [0x00; 6];
        assert!(decode_pfb_tag(&data, 0).is_none());
        // Not enough data
        let data = [0x00; 5];
        assert!(decode_pfb_tag(&data, 0).is_none());
    }

    #[test]
    fn pfb_segments() {
        let segments = [
            vec![0x01; 8],
            vec![0x02; 10],
            vec![0x03; 4],
            vec![0x04; 255],
        ];
        // Write each segment to a buffer
        let mut buf = vec![];
        for segment in &segments {
            buf.push(0x80);
            buf.push(0x02);
            buf.push(segment.len() as u8);
            buf.extend_from_slice(&[0; 3]);
            for byte in segment {
                buf.push(*byte);
            }
        }
        // Now parse and compare
        let mut parsed_count = 0;
        for (parsed, expected) in decode_pfb_binary_segments(&buf).zip(&segments) {
            assert_eq!(parsed, expected);
            parsed_count += 1;
        }
        assert_eq!(parsed_count, segments.len());
    }

    #[test]
    fn hex_decode() {
        check_hex_decode(
            b"743F8413F3636CA85A9FFEFB50B4BB27",
            &[
                116, 63, 132, 19, 243, 99, 108, 168, 90, 159, 254, 251, 80, 180, 187, 39,
            ],
        );
    }

    #[test]
    fn hex_decode_ignores_whitespace() {
        check_hex_decode(
            b"743F 8413F3636C\nA85A9FFEF\tB50B     4BB27",
            &[
                116, 63, 132, 19, 243, 99, 108, 168, 90, 159, 254, 251, 80, 180, 187, 39,
            ],
        );
    }

    #[test]
    fn hex_decode_truncate() {
        check_hex_decode(b"743F.8413F3636CA85A9FFEFB50B4BB27", &[116, 63]);
    }

    #[test]
    fn hex_decode_odd_chars() {
        check_hex_decode(b"743", &[116, 48]);
    }

    #[track_caller]
    fn check_hex_decode(hex: &[u8], expected: &[u8]) {
        let decoded = decode_hex(hex.iter().copied()).collect::<Vec<_>>();
        assert_eq!(decoded, expected);
    }

    #[test]
    fn decrypt_bytes() {
        let cipher = [
            0x74, 0x3f, 0x84, 0x13, 0xf3, 0x63, 0x6c, 0xa8, 0x5a, 0x9f, 0xfe, 0xfb, 0x50, 0xb4,
            0xbb, 0x27,
        ];
        let plain = decrypt(cipher.iter().copied(), EEXEC_SEED).collect::<Vec<_>>();
        // First 4 bytes are random garbage
        assert_eq!(&plain[4..], b"dup\n/Private");
    }

    #[test]
    fn find_eexec() {
        // Just a space
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec *&&FW"),
            Some(31)
        );
        // Multiple spaces
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec     *&&FW"),
            Some(35)
        );
        // New lines
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec\n\n*&&FW"),
            Some(32)
        );
        // Only skip \r when it precedes \n
        assert_eq!(
            find_eexec_data(b"dup\n/Private\ncurrentfile eexec\r\n\r*&&FW"),
            Some(32)
        );
        // Skip eexec in comments and strings
        assert_eq!(
            find_eexec_data(b"% eexec in comment\n(eexec in string) currentfile eexec $$$$"),
            Some(55)
        );
        // No eexec
        assert!(find_eexec_data(b"% eexec in comment\n(eexec in string) currentfile").is_none());
    }

    #[test]
    fn read_pfb_raw_dicts() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB).unwrap();
        check_noto_serif_base(dicts.base);
        check_noto_serif_private(&dicts.private);
    }

    #[test]
    fn read_pfa_raw_dicts() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        check_noto_serif_base(dicts.base);
        check_noto_serif_private(&dicts.private);
    }

    fn check_noto_serif_base(base: &[u8]) {
        const EXPECTED_PREFIX: &str = r#"%!PS-AdobeFont-1.0: NotoSerif-Regular 2.007; ttfautohint (v1.8) -l 8 -r 50 -G 200 -x 14 -D latn -f none -a qsq -X ""
%%Title: NotoSerif-Regular
%Version: 2.007; ttfautohint (v1.8) -l 8 -r 50 -G 200 -x 14 -D latn -f none -a qsq -X ""
%%CreationDate: Tue Feb 10 16:07:25 2026
%%Creator: www-data
%Copyright: Copyright 2015-2021 Google LLC. All Rights Reserved.
% Generated by FontForge 20190801 (http://fontforge.sf.net/)
%%EndComments

10 dict begin
/FontType 1 def
/FontMatrix [0.001 0 0 0.001 0 0 ]readonly def
/FontName /NotoSerif-Regular def
/FontBBox {5 0 989 775 }readonly def
"#;
        // git will replace \r\n with \n in pfa files on windows so strip
        // \r out to ensure a robust comparison
        let mut base = base.to_vec();
        base.retain(|&b| b != b'\r');
        assert!(base.starts_with(EXPECTED_PREFIX.as_bytes()));
    }

    fn check_noto_serif_private(private: &[u8]) {
        const EXPECTED_PREFIX: &str = r#"dup
/Private 8 dict dup begin
/RD{string currentfile exch readstring pop}executeonly def
/ND{noaccess def}executeonly def
/NP{noaccess put}executeonly def
/MinFeature{16 16}ND
/password 5839 def
/BlueValues [0 0 536 536 714 714 770 770 ]ND
/OtherSubrs"#;
        assert!(private.starts_with(EXPECTED_PREFIX.as_bytes()))
    }

    #[test]
    fn parse_ints() {
        check_tokens(
            "% a comment\n20 -30 2#1011 10#-5 %another!\r 16#fC",
            &[
                Token::Int(20),
                Token::Int(-30),
                Token::Int(11),
                Token::Int(-5),
                Token::Int(252),
            ],
        );
    }

    #[test]
    fn parse_num_to_int() {
        let mut parser =
            Parser::new(b"102 102.1 102.4 102.5 102.9 -102.1 -102.5 -102.9 8#146 16#66");
        for _ in 0..10 {
            assert_eq!(parser.read_num_as_int().unwrap().abs(), 102);
        }
        assert!(parser.next().is_none());
    }

    #[test]
    fn parse_strings() {
        check_tokens(
            "(string (nested) 1) % and a hex string:\n <DEAD BEEF>",
            &[
                Token::LitString(b"string (nested) 1"),
                Token::HexString(b"DEAD BEEF"),
            ],
        );
    }

    #[test]
    fn font_info_strings_use_utf8_then_latin1() {
        let mut parser = Parser::new(b"(Caf\xc3\xa9) (Caf\xe9) /Caf\xe9");
        assert_eq!(parser.read_string().as_deref(), Some("Café"));
        assert_eq!(parser.read_string().as_deref(), Some("Café"));
        assert_eq!(parser.read_string().as_deref(), Some("Café"));
    }

    #[test]
    fn latin1_glyph_and_encoding_names_match() {
        let mut charstrings = Charstrings::default();
        charstrings.push(b"Caf\xe9", b"", -1);
        assert_eq!(charstrings.name(0), Some("Café"));
        assert_eq!(charstrings.index_for_name("Café"), Some(0));

        let mut dense = Parser::new(b"[/Caf\xe9]");
        let mut dense_names = Vec::new();
        dense
            .read_dense_encoding(|code, name| dense_names.push((code, name.to_owned())))
            .unwrap();
        assert_eq!(dense_names, [(0, "Café".into())]);

        let mut sparse = Parser::new(b"65 /Caf\xe9 def");
        let mut sparse_names = Vec::new();
        sparse
            .read_sparse_encoding(|code, name| sparse_names.push((code, name.to_owned())))
            .unwrap();
        assert_eq!(sparse_names, [(65, "Café".into())]);
    }

    #[test]
    fn parse_unterminated_strings() {
        check_tokens("(string (nested) 1", &[]);
        check_tokens("<DEAD BEEF", &[]);
    }

    #[test]
    fn parse_procs() {
        check_tokens(
            "{a {nested 20 % comment\n} proc } % and a\n {simple proc}",
            &[
                Token::Proc(b"a {nested 20 % comment\n} proc "),
                Token::Proc(b"simple proc"),
            ],
        );
    }

    #[test]
    fn parse_procs_with_string_containing_unbalanced_braces() {
        check_tokens(
            "{a proc with (string {with braces}} {) }",
            &[Token::Proc(b"a proc with (string {with braces}} {) ")],
        );
    }

    #[test]
    fn parse_proc_with_single_int() {
        check_tokens(
            "dup 3 {3} executeonly put",
            &[
                Token::Raw(b"dup"),
                Token::Int(3),
                Token::Proc(b"3"),
                Token::Raw(b"executeonly"),
                Token::Raw(b"put"),
            ],
        );
    }

    #[test]
    fn parse_unterminated_procs() {
        check_tokens("{a {nested 20} proc", &[]);
    }

    #[test]
    fn parse_aggregate_tokens_without_whitespace() {
        check_tokens(
            "{(string)}3(string1)(string2)",
            &[
                Token::Proc(b"(string)"),
                Token::Int(3),
                Token::LitString(b"string1"),
                Token::LitString(b"string2"),
            ],
        );
    }

    #[test]
    fn parse_names() {
        check_tokens(
            "/FontMatrix\r %comment\n /CharStrings",
            &[Token::Name(b"FontMatrix"), Token::Name(b"CharStrings")],
        );
    }

    #[test]
    fn parse_binary_blobs() {
        check_tokens(
            "/.notdef 4 RD abcd \n5 11\n \t-| a83jnshf7 3 ",
            &[
                // simulates a charstring: name followed by data
                Token::Name(b".notdef"),
                Token::Binary(b"abcd"),
                // simulates a subr: index followed by data
                Token::Int(5),
                Token::Binary(b"a83jnshf7 3"),
            ],
        )
    }

    #[test]
    fn parse_base_dict_prefix() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let ts = parse_to_tokens(dicts.base);
        assert_eq!(
            &ts[..19],
            &[
                Token::Int(10),
                Token::Raw(b"dict"),
                Token::Raw(b"begin"),
                Token::Name(b"FontType"),
                Token::Int(1),
                Token::Raw(b"def"),
                Token::Name(b"FontMatrix"),
                Token::Raw(b"["),
                Token::Raw(b"0.001"),
                Token::Int(0),
                Token::Int(0),
                Token::Raw(b"0.001"),
                Token::Int(0),
                Token::Int(0),
                Token::Raw(b"]"),
                Token::Raw(b"readonly"),
                Token::Raw(b"def"),
                Token::Name(b"FontName"),
                Token::Name(b"NotoSerif-Regular"),
            ]
        );
    }

    #[track_caller]
    fn check_tokens(source: &str, expected: &[Token]) {
        let ts = parse_to_tokens(source.as_bytes());
        assert_eq!(ts, expected);
    }

    fn parse_to_tokens(data: &'_ [u8]) -> Vec<Token<'_>> {
        let mut tokens = vec![];
        let mut parser = Parser::new(data);
        while let Some(token) = parser.next() {
            tokens.push(token);
        }
        tokens
    }

    #[test]
    fn parse_fixed() {
        // Direct conversions (power_ten = 0)
        assert_eq!(decode_fixed(b"42.5", 0).unwrap(), Fixed::from_f64(42.5));
        assert_eq!(
            decode_fixed(b"0.0015", 0).unwrap(),
            Fixed::from_f64(0.001495361328125)
        );
        assert_eq!(
            decode_fixed(b"425.000e-1", 0).unwrap(),
            Fixed::from_f64(42.5)
        );
        assert_eq!(
            decode_fixed(b"1.5e-3", 0).unwrap(),
            Fixed::from_f64(0.001495361328125)
        );
        // Scaled by 1000 (power_ten = 3)
        assert_eq!(decode_fixed(b"1.5", 3).unwrap(), Fixed::from_f64(1500.0));
        assert_eq!(decode_fixed(b"0.001", 3).unwrap(), Fixed::from_f64(1.0));
        assert_eq!(
            decode_fixed(b"15000e-4", 3).unwrap(),
            Fixed::from_f64(1500.0)
        );
        assert_eq!(decode_fixed(b"1.000e-3", 3).unwrap(), Fixed::from_f64(1.0));
    }

    #[test]
    fn parse_font_matrix() {
        // Standard matrix for 1000 upem
        assert_eq!(
            Parser::new(b"[0.001 0 0 0.001 0 0]")
                .read_font_matrix()
                .unwrap()
                .matrix,
            FontMatrix::IDENTITY,
        );
        // Matrix with a stretch along the x axis and a small
        // offset
        assert_eq!(
            Parser::new(b"[0.002 0 0 0.001 1 2e1]")
                .read_font_matrix()
                .unwrap()
                .matrix,
            FontMatrix::from_elements([
                Fixed::from_i32(2),
                Fixed::ZERO,
                Fixed::ZERO,
                Fixed::ONE,
                Fixed::from_bits(1000),
                Fixed::from_bits(20000)
            ])
        );
        // Matrix with modified upem
        assert_eq!(
            Parser::new(b"[0.001 0 0 0.0005 0.0 0.0]")
                .read_font_matrix()
                .unwrap(),
            ScaledFontMatrix {
                matrix: FontMatrix::from_elements([
                    Fixed::from_i32(2),
                    Fixed::ZERO,
                    Fixed::ZERO,
                    Fixed::ONE,
                    Fixed::from_i32(0),
                    Fixed::from_i32(0)
                ]),
                scale: 2000,
            }
        );
    }

    #[test]
    fn parse_subrs() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let mut parser = Parser::new(&dicts.private);
        let mut subrs = None;
        while let Some(token) = parser.next() {
            if let Token::Name(b"Subrs") = token {
                subrs = parser.read_subrs(4);
                break;
            }
        }
        let mut subrs = subrs.unwrap();
        // The decrypted subroutines extracted from FreeType
        let expected_subrs: [&[u8]; 5] = [
            &[142, 139, 12, 16, 12, 17, 12, 17, 12, 33, 11],
            &[139, 140, 12, 16, 11],
            &[139, 141, 12, 16, 11],
            &[11],
            &[140, 142, 12, 16, 12, 17, 10, 11],
        ];
        assert_eq!(subrs.index.len(), expected_subrs.len());
        assert!(subrs.is_dense);
        // These subrs are densely allocated but check binary search mode
        // as well
        for is_dense in [true, false] {
            subrs.is_dense = is_dense;
            for (idx, &expected) in expected_subrs.iter().enumerate() {
                let subr = subrs.get(idx as u32).unwrap();
                assert_eq!(subr, expected);
            }
        }
    }

    #[test]
    fn parse_empty_array_subrs() {
        let subrs = Parser::new(b"[ ]").read_subrs(4).unwrap();
        assert!(subrs.data.is_empty());
        assert!(subrs.index.is_empty());
    }

    #[test]
    fn parse_empty_subrs() {
        let subrs = Parser::new(b" 0 array\nND\n").read_subrs(4).unwrap();
        assert!(subrs.data.is_empty());
        assert!(subrs.index.is_empty());
    }

    #[test]
    fn parse_malformed_subrs() {
        assert!(Parser::new(b" 20 \nND\n").read_subrs(4).is_none());
    }

    #[test]
    fn parse_subrs_duplicate_def() {
        // Two definitions of subrs.. we want to keep the first
        // one which has two entries at 5 and 42
        let private = b"/Subrs 2 array dup 5 2 RD nd NP dup 42 2 RD ab NP ND\n/Subrs 1 array dup 0 2 RD xy NP ND /CharStrings 0";
        let font = Type1Font::from_dicts(b"", private).unwrap();
        assert_eq!(font.subrs.index.len(), 2);
        assert_eq!(font.subrs.index[0].0, 5);
        assert_eq!(font.subrs.index[1].0, 42);
    }

    #[test]
    fn parse_charstrings() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let mut parser = Parser::new(&dicts.private);
        let mut charstrings = None;
        while let Some(token) = parser.next() {
            if let Token::Name(b"CharStrings") = token {
                charstrings = parser.read_charstrings(4);
                break;
            }
        }
        let charstrings = charstrings.unwrap();
        assert_eq!(charstrings.num_glyphs(), 9);
        assert!(charstrings.orig_notdef_index.is_none());
        let expected_names = [
            ".notdef",
            "H",
            "f",
            "i",
            "x",
            "f_f.liga",
            "f_f_i.liga",
            "f_i.liga",
            "H.c2sc",
        ];
        let names = (0..charstrings.num_glyphs())
            .map(|idx| charstrings.name(idx).unwrap())
            .collect::<Vec<_>>();
        assert_eq!(names, expected_names);
        // Prefix (up to 8 bytes), extracted from FreeType
        let expected_charstrings_prefix: [&[u8]; 9] = [
            &[139, 248, 236, 13, 14],
            &[177, 249, 173, 13, 139, 4, 247, 183],
            &[166, 248, 5, 13, 139, 4, 247, 201],
            &[162, 247, 212, 13, 247, 30, 249, 16],
            &[144, 248, 214, 13, 139, 4, 247, 130],
            &[166, 249, 88, 13, 139, 4, 247, 181],
            &[166, 250, 126, 13, 139, 4, 247, 181],
            &[166, 249, 43, 13, 139, 4, 247, 181],
            &[180, 249, 60, 13, 139, 4, 247, 141],
        ];
        for (idx, &expected) in expected_charstrings_prefix.iter().enumerate() {
            let charstring = charstrings.get(idx as u32).unwrap();
            assert_eq!(&charstring[..expected.len()], expected);
        }
    }

    #[test]
    fn parse_charstrings_duplicate_def() {
        // Two definitions of charstrings.. we want to keep the first
        // one which has three glyphs: .notdef, H and I
        let private = b"/CharStrings 2 /.notdef 2 RD nd ND /H 2 RD ab ND /I 2 RD cd ND def\n/CharStrings 1 /B 2 RD xy ND def";
        let font = Type1Font::from_dicts(b"", private).unwrap();
        assert_eq!(font.num_glyphs(), 3);
        assert_eq!(font.charstrings.name(0).unwrap(), ".notdef");
        assert_eq!(font.charstrings.name(1).unwrap(), "H");
        assert_eq!(font.charstrings.name(2).unwrap(), "I");
    }

    #[test]
    fn parse_charstrings_missing_notdef() {
        let mut parser = Parser::new(b"1 /H 2 RD ab ND /B 2 RD xy ND");
        let charstrings = parser.read_charstrings(-1).unwrap();
        assert_eq!(charstrings.num_glyphs(), 3);
        assert_eq!(charstrings.orig_notdef_index, Some(2));
        let expected_glyphs: &[(&str, &[u8])] =
            &[(".notdef", NOTDEF_GLYPH), ("B", b"xy"), ("H", b"ab")];
        check_charstrings(&charstrings, expected_glyphs);
        let mut font = Type1Font::empty();
        font.charstrings = charstrings;
        assert_eq!(font.remapped_gid(GlyphId::new(0)), GlyphId::new(2));
        assert_eq!(font.remapped_gid(GlyphId::new(1)), GlyphId::new(1));
        assert_eq!(font.remapped_gid(GlyphId::new(2)), GlyphId::new(0));
    }

    #[test]
    fn parse_charstrings_notdef_moved() {
        let mut parser = Parser::new(b"1 /H 2 RD ab ND /.notdef 2 RD nd ND /B 2 RD xy ND");
        let charstrings = parser.read_charstrings(-1).unwrap();
        assert_eq!(charstrings.num_glyphs(), 3);
        assert_eq!(charstrings.orig_notdef_index, Some(1));
        let expected_glyphs: &[(&str, &[u8])] = &[(".notdef", b"nd"), ("H", b"ab"), ("B", b"xy")];
        check_charstrings(&charstrings, expected_glyphs);
        let mut font = Type1Font::empty();
        font.charstrings = charstrings;
        assert_eq!(font.remapped_gid(GlyphId::new(0)), GlyphId::new(1));
        assert_eq!(font.remapped_gid(GlyphId::new(1)), GlyphId::new(0));
        assert_eq!(font.remapped_gid(GlyphId::new(2)), GlyphId::new(2));
    }

    #[track_caller]
    fn check_charstrings(charstrings: &Charstrings, expected_glyphs: &[(&str, &[u8])]) {
        for (idx, expected) in expected_glyphs.iter().enumerate() {
            let idx = idx as u32;
            let name = charstrings.name(idx).unwrap();
            let data = charstrings.get(idx).unwrap();
            assert_eq!((name, data), *expected);
        }
    }

    #[test]
    fn parse_weight_vector() {
        let mut parser = Parser::new(b"[0 0.125, 1.25 -0.87]");
        let weights = parser
            .read_weight_vector()
            .unwrap()
            .drain(..)
            .map(|w| w.to_f32())
            .collect::<Vec<_>>();
        assert_eq!(weights, &[0.0, 0.125, 1.25, -0.8699951]);
    }

    #[test]
    fn parse_type1_font_pfb() {
        check_type1_font(
            &Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB).unwrap(),
        );
    }

    #[test]
    fn parse_type1_font_pfa() {
        check_type1_font(
            &Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap(),
        );
    }

    #[track_caller]
    fn check_type1_font(font: &Type1Font) {
        assert_eq!(font.name(), Some("NotoSerif-Regular"));
        assert_eq!(
            font.version(),
            Some(
                "2.007; ttfautohint (v1.8) -l 8 -r 50 -G 200 -x 14 -D latn -f none -a qsq -X \"\""
            )
        );
        assert_eq!(
            font.notice(),
            Some("Copyright 2015-2021 Google LLC. All Rights Reserved.")
        );
        assert_eq!(font.full_name(), Some("Noto Serif Regular"));
        assert_eq!(font.family_name(), Some("Noto Serif"));
        assert_eq!(font.weight(), Some("Book"));
        assert_eq!(font.italic_angle(), 0);
        assert!(!font.is_fixed_pitch());
        assert_eq!(font.underline_position(), -125);
        assert_eq!(font.underline_thickness(), 50);
        assert_eq!(
            font.bbox(),
            BoundingBox {
                x_min: Fixed::from_i32(5),
                y_min: Fixed::ZERO,
                x_max: Fixed::from_i32(989),
                y_max: Fixed::from_i32(775)
            }
        );
        assert_eq!(font.num_glyphs(), 9);
        assert_eq!(font.subrs.index.len(), 5);
        assert_eq!(
            font.matrix,
            ScaledFontMatrix {
                matrix: FontMatrix::IDENTITY,
                scale: 1000
            }
        );
        assert!(font
            .glyph_names()
            .map(|(_, name)| name)
            .take(4)
            .eq([".notdef", "H", "f", "i"].into_iter()))
    }

    #[test]
    fn parses_lowercase_is_fixed_pitch() {
        let dicts = RawDicts::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let mut base = dicts.base.to_vec();
        base.extend_from_slice(b"\n/isFixedPitch true def\n");
        let font = Type1Font::from_dicts(&base, &dicts.private).unwrap();
        assert!(font.is_fixed_pitch());
    }

    #[test]
    fn parse_encoding() {
        assert!(matches!(
            Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA)
                .unwrap()
                .encoding,
            Some(RawEncoding::Predefined(PredefinedEncoding::Standard)),
        ));
    }

    #[test]
    fn parse_known_encodings() {
        for (blob, encoding) in [
            (
                "StandardEncoding",
                RawEncoding::Predefined(PredefinedEncoding::Standard),
            ),
            (
                "ExpertEncoding",
                RawEncoding::Predefined(PredefinedEncoding::Expert),
            ),
            (
                "ISOLatin1Encoding",
                RawEncoding::Predefined(PredefinedEncoding::IsoLatin1),
            ),
        ] {
            assert_eq!(
                Parser::new(blob.as_bytes())
                    .read_encoding(&Charstrings::default())
                    .unwrap(),
                encoding
            );
        }
    }

    #[test]
    fn parse_custom_dense_encoding() {
        let mut map = Vec::new();
        map.resize(256, ".notdef".to_string());
        let mut parser = Parser::new(b"[/.notdef /A /b /.notdef /comma /at]");
        parser.read_dense_encoding(|idx, name| {
            map[idx as usize] = name.to_string();
        });
        for (ch, entry) in map.iter().enumerate() {
            let expected = match ch {
                1 => "A",
                2 => "b",
                4 => "comma",
                5 => "at",
                _ => ".notdef",
            };
            assert_eq!(entry, expected);
        }
    }

    #[test]
    fn parse_custom_sparse_encoding() {
        let mut map = Vec::new();
        map.resize(256, ".notdef".to_string());
        let mut parser = Parser::new(CUSTOM_SPARSE_ENCODING.as_bytes());
        parser.read_sparse_encoding(|idx, name| {
            map[idx as usize] = name.to_string();
        });
        for (ch, entry) in map.iter().enumerate() {
            let expected = match ch {
                66 => "B",
                97 => "a",
                64 => "at",
                44 => "comma",
                56 => "eight",
                _ => ".notdef",
            };
            assert_eq!(entry, expected);
        }
    }

    const CUSTOM_SPARSE_ENCODING: &str = r#"
        array
        0 1 255 {1 index exch /.notdef put} for
        dup 66 /B put
        dup 97 /a put
        dup 64 /at put
        dup 44 /comma put
        dup 56 /eight put
        readonly def    
    "#;

    #[test]
    fn eval_charstrings() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let expected_eval_prefix = [
            "M38,0 L329,0 L329,42 L316,42 C293,42 274,46 258,54 C242,63 234,83 234,114",
            "M27,0 L336,0 L336,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M161,636 C176,636 190,641 201,650 C212,659 218,675 218,698 C218,721 212,738 201,746",
            "M5,0 L243,0 L243,42 L240,42 C218,42 202,44 192,50 C183,54 178,62 178,73",
            "M27,0 L316,0 L316,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M27,0 L316,0 L316,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M27,0 L316,0 L316,42 L298,42 C275,42 256,46 240,54 C224,63 216,83 216,114",
            "M41,0 L290,0 L290,42 L269,42 C254,42 240,45 229,52 C218,58 212,73 212,98",
        ];
        // -1 to ignore the .notdef glyph
        assert_eq!(font.num_glyphs() as usize - 1, expected_eval_prefix.len());
        let mut commands = CaptureCommandSink::default();
        for (gid, expected_prefix) in (1..font.num_glyphs()).zip(&expected_eval_prefix) {
            commands.0.clear();
            font.evaluate_charstring(gid.into(), &mut commands).unwrap();
            assert!(commands.to_svg().starts_with(expected_prefix));
        }
    }

    #[test]
    fn eval_charstring_widths() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let expected_widths = [
            600.0, 793.0, 369.0, 320.0, 578.0, 708.0, 1002.0, 663.0, 680.0,
        ];
        let mut commands = CaptureCommandSink::default();
        let widths = (0..font.num_glyphs())
            .map(|gid| {
                commands.0.clear();
                font.evaluate_charstring(gid.into(), &mut commands)
                    .unwrap()
                    .unwrap()
                    .to_f32()
            })
            .collect::<Vec<_>>();
        assert_eq!(widths, expected_widths);
    }

    #[test]
    fn csctx_seac_components() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        // Standard encoding for 'x' and 'i'
        let x_code = 120;
        let i_code = 105;
        let [x_data, i_data] = font.seac_components(x_code, i_code).unwrap();
        let name_to_gid = |name| {
            font.glyph_names()
                .find_map(|(gid, gname)| (name == gname).then_some(gid.to_u32()))
                .unwrap()
        };
        assert_eq!(x_data, font.charstrings.get(name_to_gid("x")).unwrap());
        assert_eq!(i_data, font.charstrings.get(name_to_gid("i")).unwrap());
    }

    #[test]
    fn csctx_subrs() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        assert!(!font.subrs.index.is_empty());
        for subr_idx in 0..font.subrs.index.len() {
            assert_eq!(
                font.subrs.get(subr_idx as u32).unwrap(),
                font.subr(subr_idx as _).unwrap()
            )
        }
    }

    #[test]
    fn encoding_mapping() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        let encoding = font.encoding().unwrap();
        let expected = [
            // code, gid, name
            (0, 0, ".notdef"),
            (72, 1, "H"),
            (102, 2, "f"),
            (105, 3, "i"),
            (120, 4, "x"),
        ];
        for (code, gid, name) in expected {
            assert_eq!(encoding.glyph_name(code).unwrap(), name);
            assert_eq!(encoding.map(code).unwrap().to_u32(), gid);
        }
    }
}

#[cfg(test)]
mod width_only_tests {
    use super::*;
    use crate::model::glyph::outline::NullSink;

    const FONTS: [&[u8]; 2] = [
        font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA,
        font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB,
    ];

    #[test]
    fn the_quick_width_is_what_a_full_run_reports() {
        for data in FONTS {
            let font = Type1Font::new(data).unwrap();
            assert!(font.num_glyphs() > 1);
            // `evaluate_charstring` answers in the charstring's own space and
            // `evaluate_width` in design units, so the matrix goes on the
            // full run before the two can be compared. It maps one onto the
            // other unchanged in these fonts, so what this pins is the width
            // rather than the mapping; the CFF side has a font where the two
            // spaces differ.
            let transform = font.transform(None);
            for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                let full = font
                    .evaluate_charstring(gid, &mut charstring::NullSink)
                    .unwrap()
                    .map(|width| transform.transform_h_metric(width));
                let quick = font.evaluate_width(gid).unwrap();
                assert_eq!(full, quick, "glyph {gid}");
            }
        }
    }

    #[test]
    fn the_quick_advance_is_what_drawing_reports() {
        // Drawing scales the width by the font matrix, and by ppem where one
        // is given, so this is what catches an advance that skipped either.
        for data in FONTS {
            let font = Type1Font::new(data).unwrap();
            for ppem in [None, Some(16.0), Some(72.0)] {
                for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                    let drawn = font.draw(gid, ppem, &mut NullSink).unwrap();
                    let quick = font.advance(gid, ppem).unwrap();
                    assert_eq!(drawn, quick, "glyph {gid} at {ppem:?}");
                }
            }
        }
    }

    #[test]
    fn every_glyph_states_a_width() {
        // A Type 1 charstring must open with hsbw or sbw, so one that
        // reports nothing is malformed rather than merely silent.
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA).unwrap();
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            assert!(font.evaluate_width(gid).unwrap().is_some(), "glyph {gid}");
        }
    }
}

#[cfg(test)]
mod extents_tests {
    use super::*;
    use crate::model::glyph::outline::PathSink;

    const FONTS: [&[u8]; 2] = [
        font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA,
        font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB,
    ];

    /// Collects the same box from the other side, through a pen.
    #[derive(Default)]
    struct BoundsPen(Option<[f32; 4]>);

    impl BoundsPen {
        fn add(&mut self, x: f32, y: f32) {
            self.0 = Some(match self.0 {
                Some([a, b, c, d]) => [a.min(x), b.min(y), c.max(x), d.max(y)],
                None => [x, y, x, y],
            });
        }
    }

    impl PathSink for BoundsPen {
        fn move_to(&mut self, x: f32, y: f32) {
            self.add(x, y);
        }
        fn line_to(&mut self, x: f32, y: f32) {
            self.add(x, y);
        }
        fn quad_to(&mut self, cx: f32, cy: f32, x: f32, y: f32) {
            self.add(cx, cy);
            self.add(x, y);
        }
        fn curve_to(&mut self, cx0: f32, cy0: f32, cx1: f32, cy1: f32, x: f32, y: f32) {
            self.add(cx0, cy0);
            self.add(cx1, cy1);
            self.add(x, y);
        }
        fn close(&mut self) {}
    }

    fn corners(b: BoundingBox<f32>) -> [f32; 4] {
        [b.x_min, b.y_min, b.x_max, b.y_max]
    }

    #[test]
    fn the_extents_are_the_box_drawing_stays_within() {
        // The same pipeline either way, so the two agree exactly. A
        // difference would mean the box was measured somewhere the drawing
        // was not, which is what transforming a finished box would do.
        for data in FONTS {
            let font = Type1Font::new(data).unwrap();
            for ppem in [None, Some(16.0), Some(72.0)] {
                let mut drew = 0;
                for gid in (0..font.num_glyphs()).map(GlyphId::new) {
                    let mut pen = BoundsPen::default();
                    font.draw(gid, ppem, &mut pen).unwrap();
                    let measured = font.extents(gid, ppem).unwrap();
                    assert_eq!(measured.map(corners), pen.0, "glyph {gid} at {ppem:?}");
                    drew += pen.0.is_some() as u32;
                }
                assert!(drew > 1, "almost nothing drew, so this proves little");
            }
        }
    }

    #[test]
    fn the_exact_box_and_the_sized_one_agree_at_no_size() {
        // Both apply the matrix; only the size and the number type differ.
        let font = Type1Font::new(FONTS[0]).unwrap();
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let exact = font.evaluate_extents(gid).unwrap();
            let sized = font.extents(gid, None).unwrap();
            assert_eq!(
                exact.map(|b| [
                    b.x_min.to_f32(),
                    b.y_min.to_f32(),
                    b.x_max.to_f32(),
                    b.y_max.to_f32()
                ]),
                sized.map(corners),
                "glyph {gid}"
            );
            checked += exact.is_some() as u32;
        }
        assert!(checked > 1, "almost nothing drew, so this proves little");
    }

    #[test]
    fn a_size_scales_the_box() {
        // `evaluate_extents` measures before the matrix and any size;
        // `extents` measures after both. This font's matrix maps its
        // charstring space onto design units unchanged, so what separates
        // the two here is the size. A matrix that does more than that is
        // covered where one exists, on the CFF side.
        let font = Type1Font::new(FONTS[0]).unwrap();
        let mut checked = 0;
        for gid in (0..font.num_glyphs()).map(GlyphId::new) {
            let Some(unscaled) = font.extents(gid, None).unwrap() else {
                continue;
            };
            let scaled = font.extents(gid, Some(16.0)).unwrap().unwrap();
            let factor = 16.0 / font.upem() as f32;
            for (a, b) in corners(scaled).iter().zip(corners(unscaled)) {
                assert!(
                    (a - b * factor).abs() < 0.05,
                    "glyph {gid}: {a} against {}",
                    b * factor
                );
            }
            checked += 1;
        }
        assert!(checked > 1, "almost nothing drew, so this proves little");
    }

    #[test]
    fn a_glyph_that_draws_nothing_has_no_extents() {
        // A box of no size at the origin would say something different from
        // drawing nothing at all.
        let font = Type1Font::new(FONTS[0]).unwrap();
        let empty = (0..font.num_glyphs())
            .map(GlyphId::new)
            .find(|gid| font.extents(*gid, None).unwrap().is_none());
        let gid = empty.expect("this font has no empty glyph to check");
        assert!(font.evaluate_extents(gid).unwrap().is_none());
    }
}

#[cfg(test)]
pub(crate) mod multiple_master_tests {
    use super::*;
    use crate::model::glyph::outline::PathElement;
    use alloc::{format, vec};

    /// Encodes `value` as a Type 1 charstring number.
    fn num(value: i32) -> Vec<u8> {
        match value {
            -107..=107 => vec![(value + 139) as u8],
            108..=1131 => {
                let v = value - 108;
                vec![(v / 256 + 247) as u8, (v % 256) as u8]
            }
            -1131..=-108 => {
                let v = -value - 108;
                vec![(v / 256 + 251) as u8, (v % 256) as u8]
            }
            _ => {
                let mut bytes = vec![255];
                bytes.extend_from_slice(&value.to_be_bytes());
                bytes
            }
        }
    }

    /// Builds a charstring from numbers and operators, where an operator is
    /// given as its bytes.
    fn charstring(parts: &[&[i32]]) -> Vec<u8> {
        let mut bytes = Vec::new();
        for part in parts {
            match part {
                [op] if *op < 0 => bytes.push((-op) as u8),
                [12, op] if *op < 0 => bytes.extend_from_slice(&[12, (-op) as u8]),
                values => values.iter().for_each(|&v| bytes.extend(num(v))),
            }
        }
        bytes
    }

    const HSBW: &[i32] = &[-13];
    const RMOVETO: &[i32] = &[-21];
    const RLINETO: &[i32] = &[-5];
    const CLOSEPATH: &[i32] = &[-9];
    const ENDCHAR: &[i32] = &[-14];
    const CALLOTHERSUBR: &[i32] = &[12, -16];
    const POP: &[i32] = &[12, -17];

    /// Reads the font that [`pfa_with_blend`] makes.
    fn font_with_blend(blend: &str) -> Option<Type1Font> {
        Type1Font::new(&pfa_with_blend(blend)).ok()
    }

    /// Returns a PFA font with `blend` in place of the blend entries of its
    /// font dictionary.
    ///
    /// Glyph 1 moves to x = 100 + 200 * w1, where w1 is the weight of the
    /// second master, then draws a triangle.
    pub(crate) fn pfa_with_blend(blend: &str) -> Vec<u8> {
        let base = format!(
            "%!PS-AdobeFont-1.0: Test\n/FontName /Test def \
             /FontMatrix [0.001 0 0 0.001 0 0] readonly def {blend}\n\
             currentfile eexec\n"
        );
        let notdef = charstring(&[&[0, 500], HSBW, ENDCHAR]);
        let glyph = charstring(&[
            &[0, 500],
            HSBW,
            &[100, 50, 200, 0, 4, 15],
            CALLOTHERSUBR,
            POP,
            POP,
            RMOVETO,
            &[300, 0],
            RLINETO,
            &[0, 300],
            RLINETO,
            CLOSEPATH,
            ENDCHAR,
        ]);
        let mut private = b"/lenIV -1 def /CharStrings 2 dict dup begin\n".to_vec();
        for (name, bytes) in [("/.notdef", notdef), ("/a", glyph)] {
            private.extend_from_slice(format!("{name} {} RD ", bytes.len()).as_bytes());
            private.extend_from_slice(&bytes);
            private.extend_from_slice(b" ND\n");
        }
        private.extend_from_slice(b"end");
        // Encrypted as eexec data, after four bytes of padding, and written
        // as hex.
        let mut seed = EEXEC_SEED;
        let mut pfa = base.into_bytes();
        for plain in [0u8; 4].iter().chain(&private) {
            let cipher = (*plain as u32 ^ (seed >> 8)) as u8;
            seed = (cipher as u32)
                .wrapping_add(seed)
                .wrapping_mul(52845)
                .wrapping_add(22719)
                & 0xFFFF;
            pfa.extend_from_slice(format!("{cipher:02x}").as_bytes());
        }
        pfa
    }

    pub(crate) const ONE_AXIS: &str = "/BlendAxisTypes [/Weight] def \
        /BlendDesignPositions [[0] [1]] def \
        /BlendDesignMap [[[100 0] [900 1]]] def \
        /WeightVector [0.75 0.25] def";

    fn bbox(font: &Type1Font) -> [f32; 4] {
        let b = font.bbox();
        [b.x_min, b.y_min, b.x_max, b.y_max].map(|v| v.to_f32())
    }

    // Every box is rounded to whole units, halves away from zero.
    #[test]
    fn boxes_are_rounded() {
        let font = font_with_blend("/FontBBox {-10.4 -20.5 1000.5 900.25} readonly def").unwrap();
        assert_eq!(bbox(&font), [-10.0, -21.0, 1001.0, 900.0]);
    }

    // A bound that rounds past the largest fixed value saturates instead of
    // overflowing.
    #[test]
    fn box_rounding_saturates() {
        let font = font_with_blend("/FontBBox [-32768 0 32767.5 900] def").unwrap();
        assert_eq!(font.bbox().x_max, Fixed::MAX);
        assert_eq!(font.bbox().x_min, Fixed::from_i32(-32768));
    }

    // The /Blend dictionary holds a box for each master, and master 0's
    // replaces the font's own.
    #[test]
    fn box_is_master_zeros() {
        let font = font_with_blend(&format!(
            "{ONE_AXIS} /FontBBox {{-157 -257 1194 872}} readonly def \
             /Blend 3 dict dup begin \
             /FontBBox {{{{-55 -112}} {{-261 -262}} {{424.4 1156}} {{811 970}}}} def \
             end def"
        ))
        .unwrap();
        assert_eq!(bbox(&font), [-55.0, -261.0, 424.0, 811.0]);
        let bracketed = font_with_blend(&format!(
            "{ONE_AXIS} /Blend 3 dict dup begin \
             /FontBBox [[-55 -112] [-261 -262] [424 1156] [811 970]] def end def"
        ))
        .unwrap();
        assert_eq!(bbox(&bracketed), [-55.0, -261.0, 424.0, 811.0]);
    }

    // The /Blend dictionary's font info holds a value for each master, and
    // the font keeps its own.
    #[test]
    fn font_info_keeps_its_own_values() {
        let font = font_with_blend(&format!(
            "{ONE_AXIS} /ItalicAngle -12 def /isFixedPitch true def \
             /UnderlinePosition -100 def /UnderlineThickness 50 def \
             /Blend 3 dict dup begin /FontInfo 4 dict dup begin \
             /ItalicAngle [-12 0] def /isFixedPitch [false false] def \
             /UnderlinePosition [-80 -120] def /UnderlineThickness [40 60] def \
             end def end def"
        ))
        .unwrap();
        assert_eq!(font.italic_angle(), -12);
        assert!(font.is_fixed_pitch());
        assert_eq!(font.underline_position(), -100);
        assert_eq!(font.underline_thickness(), 50);
    }

    fn first_move(instance: Type1Instance) -> f32 {
        let mut path = Vec::<PathElement>::new();
        instance.draw(GlyphId::new(1), None, &mut path).unwrap();
        match path[0] {
            PathElement::MoveTo { x, .. } => x,
            _ => panic!("expected a move"),
        }
    }

    #[test]
    fn reads_the_design_space() {
        let font = font_with_blend(ONE_AXIS).unwrap();
        let mm = font.multiple_master().unwrap();
        assert_eq!(mm.num_masters(), 2);
        let [axis] = mm.axes() else {
            panic!("expected one axis")
        };
        assert_eq!(axis.name(), Some("Weight"));
        assert_eq!((axis.min_design(), axis.max_design()), (100, 900));
        let mut blend = [Fixed::ZERO];
        mm.default_blend(&mut blend);
        assert_eq!(blend, [Fixed::from_f64(0.25)]);
        assert_eq!(axis.blend_to_design(blend[0]), 300);
    }

    #[test]
    fn draws_with_a_blend_of_the_masters() {
        let font = font_with_blend(ONE_AXIS).unwrap();
        let mm = font.multiple_master().unwrap();
        // The font's own weights.
        assert_eq!(first_move(font.default_instance()), 150.0);
        let mut blend = [Fixed::ZERO];
        let mut weights = [Fixed::ZERO; 2];
        for (design, x) in [(100.0, 100.0), (500.0, 200.0), (900.0, 300.0)] {
            mm.design_to_blend(&[Fixed::from_f64(design)], &mut blend);
            mm.blend_to_weights(&blend, &mut weights);
            assert_eq!(first_move(font.instance(&weights)), x, "{design}");
        }
    }

    // Blends that FreeType sets aside leave an ordinary font, whose glyphs
    // fail where they blend.
    #[test]
    fn inconsistent_blends_are_set_aside() {
        for blend in [
            // Three masters cannot sit at the corners of one axis.
            "/BlendDesignPositions [[0] [1] [0.5]] def \
             /BlendDesignMap [[[100 0] [900 1]]] def \
             /WeightVector [0.5 0.25 0.25] def",
            // No design map.
            "/BlendDesignPositions [[0] [1]] def /WeightVector [0.75 0.25] def",
            // Only weights, as in a font made from one instance.
            "/WeightVector [0.75 0.25] def",
        ] {
            let font = font_with_blend(blend).unwrap();
            assert!(font.multiple_master().is_none(), "{blend}");
            let mut path = Vec::<PathElement>::new();
            assert!(font.draw(GlyphId::new(1), None, &mut path).is_err());
        }
    }

    // Counts that disagree make FreeType refuse the font.
    #[test]
    fn conflicting_counts_refuse_the_font() {
        for blend in [
            "/BlendDesignPositions [[0] [1]] def /WeightVector [0.5 0.25 0.25] def",
            "/BlendAxisTypes [/Weight /Width] def /BlendDesignPositions [[0] [1]] def",
            "/BlendDesignMap [[[100 0] [900 1]]] def /BlendDesignMap [[[100 0] [900 1]]] def",
            "/BlendAxisTypes [/A /B /C /D /E] def",
        ] {
            assert!(font_with_blend(blend).is_none(), "{blend}");
        }
    }

    /// The design space of a font with two axes, each mapping 50 to 1450,
    /// whose default is the last master.
    fn two_axes() -> MultipleMaster {
        let axis = |name: &str| MasterAxis {
            name: Some(name.into()),
            design_points: vec![50, 1450],
            blend_points: vec![Fixed::ZERO, Fixed::ONE],
        };
        MultipleMaster {
            axes: vec![axis("Weight"), axis("Width")],
            num_masters: 4,
            default_weights: vec![Fixed::ZERO, Fixed::ZERO, Fixed::ZERO, Fixed::ONE],
        }
    }

    // The blends and weights FreeType computes for these design coordinates
    // in a font with this design space.
    #[test]
    fn design_coordinates_convert_as_in_freetype() {
        let mm = two_axes();
        for (design, blend, weights) in [
            (
                &[471.5, 819.4][..],
                [19754, 35998],
                [20635, 8903, 25147, 10851],
            ),
            (
                &[470.49, 820.51],
                [19661, 36092],
                [20611, 8833, 25264, 10828],
            ),
            (&[1000.0, 333.0], [44471, 13248], [16807, 35481, 4258, 8990]),
            (&[-100.0, 2000.0], [0, 65536], [0, 0, 65536, 0]),
            // The missing axis takes (1450 - 50) / 2 = 700.
            (&[300.0], [11703, 30427], [28839, 6270, 24994, 5433]),
        ] {
            let design: Vec<_> = design
                .iter()
                .map(|v| Fixed::from_bits((v * 65536.0) as i32))
                .collect();
            let mut actual_blend = [Fixed::ZERO; 2];
            mm.design_to_blend(&design, &mut actual_blend);
            assert_eq!(actual_blend.map(Fixed::to_bits), blend, "{design:?}");
            let mut actual_weights = [Fixed::ZERO; 4];
            mm.blend_to_weights(&actual_blend, &mut actual_weights);
            assert_eq!(actual_weights.map(Fixed::to_bits), weights, "{design:?}");
        }
        let mut default = [Fixed::ZERO; 2];
        mm.default_blend(&mut default);
        assert_eq!(default, [Fixed::ONE; 2]);
        assert!(mm
            .axes()
            .iter()
            .all(|axis| axis.blend_to_design(Fixed::ONE) == 1450));
    }

    // Between two points that are not the first, FreeType's interpolation
    // leaves out the blend at the lower point.
    #[test]
    fn three_point_maps_interpolate_as_in_freetype() {
        let axis = MasterAxis {
            name: None,
            design_points: vec![0, 500, 1000],
            blend_points: vec![Fixed::ZERO, Fixed::from_f64(0.6), Fixed::ONE],
        };
        assert_eq!(axis.design_to_blend(250), Fixed::from_f64(0.3));
        assert_eq!(axis.design_to_blend(500), Fixed::from_f64(0.6));
        // The specification gives 0.8.
        assert_eq!(axis.design_to_blend(750), Fixed::from_f64(0.2));
        assert_eq!(axis.design_to_blend(2000), Fixed::ONE);
    }

    // Design coordinates round to whole numbers, a half away from zero.
    #[test]
    fn design_coordinates_round_away_from_zero() {
        let axis = MasterAxis {
            name: None,
            design_points: vec![-10, 10],
            blend_points: vec![Fixed::ZERO, Fixed::ONE],
        };
        let mm = MultipleMaster {
            axes: vec![axis],
            num_masters: 2,
            default_weights: vec![Fixed::ONE, Fixed::ZERO],
        };
        let mut blend = [Fixed::ZERO];
        for (design, rounded) in [(0.5, 1), (-0.5, -1), (0.49, 0), (-1.5, -2)] {
            mm.design_to_blend(&[Fixed::from_f64(design)], &mut blend);
            assert_eq!(blend[0], mm.axes()[0].design_to_blend(rounded), "{design}");
        }
    }

    // A map can span every design coordinate without overflowing.
    #[test]
    fn widest_design_maps_convert() {
        let axis = MasterAxis {
            name: None,
            design_points: vec![i32::MIN, i32::MAX],
            blend_points: vec![Fixed::ZERO, Fixed::ONE],
        };
        assert_eq!(axis.design_to_blend(0), Fixed::from_f64(0.5));
        assert_eq!(axis.blend_to_design(Fixed::from_f64(0.5)), 0);
        assert_eq!(axis.blend_to_design(Fixed::ONE), i32::MAX);
        assert_eq!(axis.blend_to_design(Fixed::ZERO), i32::MIN);
        let mm = MultipleMaster {
            axes: vec![axis],
            num_masters: 2,
            default_weights: vec![Fixed::from_f64(0.5), Fixed::from_f64(0.5)],
        };
        // A missing coordinate is half the range, which is beyond the map.
        let mut blend = [Fixed::ZERO];
        mm.design_to_blend(&[], &mut blend);
        assert_eq!(blend[0], Fixed::ONE);
        let font = font_with_blend(
            "/BlendAxisTypes [/Weight] def /BlendDesignPositions [[0] [1]] def              /BlendDesignMap [[[-2147483648 0] [2147483647 1]]] def              /WeightVector [0.5 0.5] def",
        )
        .unwrap();
        assert!(font.multiple_master().is_some());
    }
}
