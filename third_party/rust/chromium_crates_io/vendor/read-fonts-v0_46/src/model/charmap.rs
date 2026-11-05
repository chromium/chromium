//! Character mappings for a font.

use crate::{
    model::{font::SharedFont, Font},
    ps::type1::Type1Font,
    ps::PredefinedEncoding,
    tables::{
        cmap::{
            Cmap, CmapIterLimits, CmapSubtable, EncodingRecord, MapVariant, PlatformId, UvsSubtable,
        },
        name::MacRomanMapping,
    },
    FontData, TableProvider,
};
use alloc::vec::Vec;
use types::{GlyphId, Tag};
use yoke::Yokeable;

/// Character mappings for a font.
///
/// Obtain this facade with [`Font::charmap`]. It offers two ways to map
/// characters, depending on what the caller knows about its input.
///
/// # Unicode lookup
///
/// [`map_unicode`](Self::map_unicode) accepts a Unicode codepoint (or `char`).
/// For an SFNT font, it uses one selected `cmap` subtable, with a preference
/// for a Microsoft symbol subtable, then full Unicode, then BMP Unicode, and
/// finally Mac Roman. A symbol subtable retries codes in U+0000..U+00FF at
/// U+F000..U+F0FF when the direct lookup fails; [`unicodes`](Self::unicodes)
/// lists only the codes the subtable holds. A Mac Roman subtable converts
/// Unicode codepoints to Mac Roman character codes before lookup. For Type 1 and standalone CFF
/// fonts, the Unicode map is derived from glyph names when the `agl` feature is
/// enabled. Glyph ID 0 is treated as unmapped.
/// An optional `DMAP` table takes precedence, with missing mappings falling
/// back to `cmap` (ISO/IEC 14496-22:2026, 5.6.15).
/// [`map_unicode_batched`](Self::map_unicode_batched) writes several results
/// while reusing the selected subtable and, for formats 4 and 12, the preceding
/// segment or group when possible.
///
/// [`map_unicode_variant`](Self::map_unicode_variant) uses the last SFNT
/// `cmap` variation sequences subtable, preferring format 15 to format 14. A non-default variation sequence names its glyph
/// directly; a default sequence resolves through the selected Unicode map.
/// Type 1 and standalone CFF fonts have no variation-sequence map.
///
/// The corresponding iterators are [`unicodes`](Self::unicodes) and
/// [`unicode_variants`](Self::unicode_variants).
///
/// # Individual encodings
///
/// [`encodings`](Self::encodings) lists each supported SFNT `cmap` encoding
/// record separately, including multiple records with the same
/// [`EncodingKind`]. It also lists native and glyph-name-derived Unicode
/// encodings for Type 1 and standalone CFF fonts when available. Keep the complete
/// [`Encoding`] value to select that specific mapping with
/// [`map_code`](Self::map_code) or [`codes`](Self::codes).
/// These methods use character codes in the selected encoding as stored in
/// the font. They do not convert Mac Roman codes from Unicode or apply the
/// Microsoft symbol fallback. A mapping to glyph ID 0 is treated as unmapped.
/// DMAP overrides are applied to matching encodings, and encodings supplied
/// only by DMAP are also available.
///
/// The selected Unicode subtable and the individual encoding records are
/// parsed and cached separately, only when their respective methods need them.
#[derive(Clone, Copy)]
pub struct Charmap<'a> {
    font: &'a SharedFont,
}

impl<'a> Charmap<'a> {
    pub(crate) fn new(font: &'a Font) -> Self {
        Self {
            font: font.shared(),
        }
    }

    /// Returns whether this font has a selected Unicode mapping.
    ///
    /// For Type 1 fonts, this requires a nonempty glyph-name-derived map.
    pub fn has_unicode(&self) -> bool {
        match self.font.kind() {
            crate::model::Kind::Type1(font) => font.unicode_charmap().iter().next().is_some(),
            crate::model::Kind::Cff(_, _) => self
                .font
                .standalone_cff_unicode_charmap()
                .is_some_and(|map| map.iter().next().is_some()),
            _ => self
                .font
                .unicode_charmap()
                .is_some_and(|map| map.dmap.subtable.is_some() || map.cmap.subtable.is_some()),
        }
    }

    /// Returns whether Unicode lookup uses a Microsoft symbol subtable.
    pub fn unicode_is_symbol(&self) -> bool {
        self.font
            .unicode_charmap()
            .is_some_and(|map| map.dmap.is_symbol || map.cmap.is_symbol)
    }

    /// Returns whether Unicode lookup uses a Mac Roman subtable.
    pub fn unicode_is_mac_roman(&self) -> bool {
        self.font
            .unicode_charmap()
            .is_some_and(|map| map.dmap.is_mac_roman || map.cmap.is_mac_roman)
    }

    /// Maps a Unicode codepoint to a glyph.
    ///
    /// Type 1 and standalone CFF fonts use a glyph-name-based map when the
    /// `agl` feature is enabled.
    /// A mapping to glyph 0 is treated as unmapped.
    pub fn map_unicode(&self, codepoint: impl Into<u32>) -> Option<GlyphId> {
        let codepoint = codepoint.into();
        let glyph = match self.font.kind() {
            crate::model::Kind::Type1(font) => font.unicode_charmap().map(codepoint),
            crate::model::Kind::Cff(_, _) => {
                self.font.standalone_cff_unicode_charmap()?.map(codepoint)
            }
            _ => self.font.unicode_charmap()?.map(codepoint),
        };
        glyph.filter(|glyph| *glyph != GlyphId::NOTDEF)
    }

    /// Writes the Unicode mapping of each codepoint to its slot, in order.
    ///
    /// Unmapped codepoints, including mappings to glyph 0, receive
    /// [`GlyphId::NOTDEF`]. Returns the number of initial consecutive nonzero
    /// mappings, while still writing every output. This uses
    /// the same selected mapping, Mac Roman conversion, and Microsoft symbol
    /// fallback as [`map_unicode`](Self::map_unicode).
    pub fn map_unicode_batched<'o>(
        &self,
        codepoints: impl Iterator<Item = (u32, &'o mut GlyphId)>,
    ) -> usize {
        match self.font.kind() {
            crate::model::Kind::Type1(font) => {
                map_ps_batched(Some(font.unicode_charmap()), codepoints)
            }
            crate::model::Kind::Cff(_, _) => {
                map_ps_batched(self.font.standalone_cff_unicode_charmap(), codepoints)
            }
            _ => {
                if let Some(map) = self.font.unicode_charmap() {
                    map.map_batched(codepoints)
                } else {
                    for (_, out) in codepoints {
                        *out = GlyphId::NOTDEF;
                    }
                    0
                }
            }
        }
    }

    /// Iterates over Unicode codepoints and their resolved glyph IDs.
    ///
    /// A Microsoft symbol subtable yields only the codes it contains.
    /// [`map_unicode`](Self::map_unicode) also maps U+0000..U+00FF through
    /// U+F000..U+F0FF, but those codes are not included.
    ///
    /// Type 1 and standalone CFF mappings are derived from glyph names when
    /// `agl` is enabled.
    pub fn unicodes(&self) -> impl Iterator<Item = (u32, GlyphId)> + 'a {
        let this = *self;
        let type1 = match this.font.kind() {
            crate::model::Kind::Type1(font) => Some(font),
            _ => None,
        };
        let type1 = type1.into_iter().flat_map(iter_type1_unicodes);
        let standalone_cff = this
            .font
            .standalone_cff_unicode_charmap()
            .into_iter()
            .flat_map(iter_ps_unicodes);
        let sfnt = this.font.unicode_charmap().into_iter().flat_map(|map| {
            map.iter(CmapIterLimits {
                max_char: char::MAX as u32,
                glyph_count: this.font.num_glyphs(),
            })
        });
        type1
            .chain(standalone_cff)
            .chain(sfnt)
            .filter(|(_, glyph)| *glyph != GlyphId::NOTDEF)
    }

    /// Returns whether this font has a Unicode variation-sequence subtable.
    pub fn has_unicode_variants(&self) -> bool {
        self.font
            .unicode_charmap()
            .is_some_and(|map| map.dmap.vs_subtable.is_some() || map.cmap.vs_subtable.is_some())
    }

    /// Maps a Unicode codepoint and variation selector to a glyph.
    ///
    /// A default UVS entry resolves through [`map_unicode`](Self::map_unicode).
    /// A mapping to glyph 0 is treated as unmapped.
    pub fn map_unicode_variant(
        &self,
        codepoint: impl Into<u32>,
        selector: impl Into<u32>,
    ) -> Option<GlyphId> {
        self.font
            .unicode_charmap()?
            .map_variant(codepoint.into(), selector.into())
            .filter(|glyph| *glyph != GlyphId::NOTDEF)
    }

    /// Iterates over Unicode variation sequences and their resolved glyph IDs.
    ///
    /// Default variation entries use the selected Unicode mapping, just as
    /// [`map_unicode_variant`](Self::map_unicode_variant) does.
    ///
    /// Yields `(codepoint, selector, glyph)` tuples, where `glyph` is the
    /// resolved glyph ID.
    pub fn unicode_variants(&self) -> impl Iterator<Item = (u32, u32, GlyphId)> + 'a {
        let this = *self;
        this.font.unicode_charmap().into_iter().flat_map(|map| {
            map.variant_mappings(CmapIterLimits {
                max_char: char::MAX as u32,
                glyph_count: this.font.num_glyphs(),
            })
            .filter_map(move |(codepoint, selector, variant)| {
                let glyph = match variant {
                    MapVariant::UseDefault => map.map(codepoint)?,
                    MapVariant::Variant(glyph) => glyph,
                };
                (glyph != GlyphId::NOTDEF).then_some((codepoint, selector, glyph))
            })
        })
    }

    /// Returns the selectable character mappings of this font.
    ///
    /// More than one mapping can have the same kind. Keep the full
    /// [`Encoding`] value to select a mapping with [`map_code`](Self::map_code).
    pub fn encodings(&self) -> impl Iterator<Item = Encoding> + 'a {
        let this = *self;
        let sfnt = this
            .font
            .encodings()
            .into_iter()
            .flat_map(EncodingTables::iter);
        let type1 = match this.font.kind() {
            crate::model::Kind::Type1(font) => Some(font),
            _ => None,
        };
        let unicode = type1
            .filter(|font| font.unicode_charmap().iter().next().is_some())
            .map(|_| Encoding::type1_unicode());
        let native = type1
            .and_then(|font| font.encoding())
            .map(|encoding| Encoding::type1_native(encoding.predefined()));
        let standalone_cff = match this.font.kind() {
            crate::model::Kind::Cff(font, _) => Some(font),
            _ => None,
        };
        let standalone_cff_unicode = standalone_cff
            .as_ref()
            .filter(|_| this.has_unicode())
            .map(|_| Encoding::standalone_cff_unicode());
        let standalone_cff_native = standalone_cff
            .as_ref()
            .filter(|font| !font.is_cid())
            .and_then(|font| font.encoding())
            .map(|encoding| Encoding::standalone_cff_native(encoding.predefined()));
        sfnt.chain(unicode)
            .chain(native)
            .chain(standalone_cff_unicode)
            .chain(standalone_cff_native)
    }

    /// Maps a character code through a mapping returned by [`encodings`](Self::encodings).
    ///
    /// The code is interpreted in the selected encoding, without Unicode
    /// conversion. The encoding must come from this font. A mapping to glyph 0
    /// is treated as unmapped.
    pub fn map_code(&self, encoding: Encoding, code: u32) -> Option<GlyphId> {
        let glyph = match self.font.kind() {
            crate::model::Kind::Type1(font) if encoding == Encoding::type1_unicode() => {
                font.unicode_charmap().map(code)
            }
            crate::model::Kind::Type1(font) => {
                let native = font.encoding()?;
                if encoding != Encoding::type1_native(native.predefined()) {
                    return None;
                }
                native.map(u8::try_from(code).ok()?)
            }
            crate::model::Kind::Cff(_, _) if encoding == Encoding::standalone_cff_unicode() => {
                self.font.standalone_cff_unicode_charmap()?.map(code)
            }
            crate::model::Kind::Cff(font, _) => {
                if font.is_cid() {
                    return None;
                }
                let native = font.encoding()?;
                if encoding != Encoding::standalone_cff_native(native.predefined()) {
                    return None;
                }
                native.map(u8::try_from(code).ok()?)
            }
            _ => self.font.encodings()?.map(encoding, code),
        };
        glyph.filter(|glyph| *glyph != GlyphId::NOTDEF)
    }

    /// Iterates over character codes and glyph IDs in the selected encoding.
    ///
    /// The `encoding` must come from this font's [`encodings`](Self::encodings).
    /// Codes are returned as stored in the encoding; Mac Roman conversion and
    /// symbol fallback are only used by the Unicode methods. Mappings to glyph
    /// 0 are omitted.
    pub fn codes(&self, encoding: Encoding) -> impl Iterator<Item = (u32, GlyphId)> + 'a {
        let this = *self;
        let type1 = match this.font.kind() {
            crate::model::Kind::Type1(font) => Some(font),
            _ => None,
        };
        let type1_unicode = type1
            .filter(|_| encoding == Encoding::type1_unicode())
            .into_iter()
            .flat_map(iter_type1_unicodes);
        let type1_native = type1
            .and_then(|font| font.encoding())
            .filter(|native| encoding == Encoding::type1_native(native.predefined()))
            .into_iter()
            .flat_map(|native| {
                (0_u32..=255)
                    .filter_map(move |code| native.map(code as u8).map(|glyph| (code, glyph)))
            });
        let standalone_cff = match this.font.kind() {
            crate::model::Kind::Cff(font, _) => Some(font),
            _ => None,
        };
        let standalone_cff_unicode = (encoding == Encoding::standalone_cff_unicode())
            .then(|| this.font.standalone_cff_unicode_charmap())
            .flatten()
            .into_iter()
            .flat_map(iter_ps_unicodes);
        let standalone_cff_native = standalone_cff
            .as_ref()
            .filter(|font| !font.is_cid())
            .and_then(|font| font.encoding())
            .filter(|native| encoding == Encoding::standalone_cff_native(native.predefined()))
            .into_iter()
            .flat_map(|native| {
                (0_u32..=255)
                    .filter_map(move |code| native.map(code as u8).map(|glyph| (code, glyph)))
            });
        let sfnt = this.font.encodings().into_iter().flat_map(move |tables| {
            tables.codes(
                encoding,
                CmapIterLimits {
                    max_char: u32::MAX,
                    glyph_count: this.font.num_glyphs(),
                },
            )
        });
        type1_unicode
            .chain(type1_native)
            .chain(standalone_cff_unicode)
            .chain(standalone_cff_native)
            .chain(sfnt)
            .filter(|(_, glyph)| *glyph != GlyphId::NOTDEF)
    }
}

fn map_ps_batched<'o>(
    map: Option<&crate::ps::UnicodeCharmap>,
    codepoints: impl Iterator<Item = (u32, &'o mut GlyphId)>,
) -> usize {
    let mut mapped = 0;
    let mut initial = true;
    for (codepoint, out) in codepoints {
        *out = map
            .and_then(|map| map.map(codepoint))
            .unwrap_or(GlyphId::NOTDEF);
        initial &= *out != GlyphId::NOTDEF;
        mapped += usize::from(initial);
    }
    mapped
}

fn iter_type1_unicodes(font: &Type1Font) -> impl Iterator<Item = (u32, GlyphId)> + '_ {
    iter_ps_unicodes(font.unicode_charmap())
}

fn iter_ps_unicodes(map: &crate::ps::UnicodeCharmap) -> impl Iterator<Item = (u32, GlyphId)> + '_ {
    // Each character after the last, as FreeType steps through a charmap
    // built from glyph names. No glyph maps to zero, so the first is the
    // one after it. A character mapped to glyph zero ends the walk, as it
    // ends FreeType's.
    let mut codepoint = 0;
    core::iter::from_fn(move || {
        let (next, glyph) = map.next_char(codepoint)?;
        codepoint = next;
        (glyph != GlyphId::NOTDEF).then_some((next, glyph))
    })
}

/// The character encoding of a font mapping.
///
/// Values match FreeType's `FT_Encoding` tags. Multiple mappings can have
/// the same kind, so use the full [`Encoding`] to select one.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u32)]
pub enum EncodingKind {
    /// An unclassified mapping.
    None = 0,
    /// Microsoft symbol encoding.
    MsSymbol = 0x7379_6d62,
    /// Unicode encoding.
    Unicode = 0x756e_6963,
    /// Shift JIS encoding.
    Sjis = 0x736a_6973,
    /// Simplified Chinese encoding.
    Prc = 0x6762_2020,
    /// Traditional Chinese encoding.
    Big5 = 0x6269_6735,
    /// Wansung encoding.
    Wansung = 0x7761_6e73,
    /// Johab encoding.
    Johab = 0x6a6f_6861,
    /// Adobe Standard encoding.
    AdobeStandard = 0x4144_4f42,
    /// Adobe Expert encoding.
    AdobeExpert = 0x4144_4245,
    /// Adobe Custom encoding.
    AdobeCustom = 0x4144_4243,
    /// Adobe Latin 1 encoding.
    AdobeLatin1 = 0x6c61_7431,
    /// Apple Roman encoding.
    AppleRoman = 0x6172_6d6e,
}

/// A selectable character mapping of a font.
///
/// This fixed-size value contains no borrowed data. It is a key for the font
/// that produced it; passing it to another font has no defined mapping.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(C)]
pub struct Encoding {
    kind: EncodingKind,
    index: u32,
    platform_id: u16,
    encoding_id: u16,
}

impl Encoding {
    const DMAP: u32 = 1 << 31;
    const BARE_CFF_UNICODE: u32 = u32::MAX - 3;
    const BARE_CFF_NATIVE: u32 = u32::MAX - 2;
    const TYPE1_UNICODE: u32 = u32::MAX - 1;
    const TYPE1_NATIVE: u32 = u32::MAX;

    /// Returns the encoding kind.
    pub fn kind(self) -> EncodingKind {
        self.kind
    }

    /// Returns the tag of the table that holds this encoding's record, `cmap`
    /// or `DMAP`, and the index of the record within that table.
    ///
    /// Returns `None` for an encoding from a Type 1 or standalone CFF font.
    pub fn sfnt_index(self) -> Option<(Tag, u32)> {
        // The DMAP bit is set on the standalone CFF and Type 1 values too, so
        // records end below them.
        if self.index >= Self::BARE_CFF_UNICODE {
            return None;
        }
        let tag = if self.index & Self::DMAP != 0 {
            Tag::new(b"DMAP")
        } else {
            Tag::new(b"cmap")
        };
        Some((tag, self.index & !Self::DMAP))
    }

    /// Returns the SFNT platform and encoding identifiers, if applicable.
    pub fn sfnt_ids(self) -> Option<(u16, u16)> {
        self.sfnt_index()
            .map(|_| (self.platform_id, self.encoding_id))
    }

    pub(crate) fn type1_unicode() -> Self {
        Self {
            kind: EncodingKind::Unicode,
            index: Self::TYPE1_UNICODE,
            platform_id: 0,
            encoding_id: 0,
        }
    }

    pub(crate) fn type1_native(predefined: Option<PredefinedEncoding>) -> Self {
        let kind = match predefined {
            Some(PredefinedEncoding::Standard) => EncodingKind::AdobeStandard,
            Some(PredefinedEncoding::Expert) => EncodingKind::AdobeExpert,
            Some(PredefinedEncoding::IsoLatin1) => EncodingKind::AdobeLatin1,
            None => EncodingKind::AdobeCustom,
        };
        Self {
            kind,
            index: Self::TYPE1_NATIVE,
            platform_id: 0,
            encoding_id: 0,
        }
    }

    pub(crate) fn standalone_cff_unicode() -> Self {
        Self {
            index: Self::BARE_CFF_UNICODE,
            ..Self::type1_unicode()
        }
    }

    pub(crate) fn standalone_cff_native(predefined: Option<PredefinedEncoding>) -> Self {
        Self {
            index: Self::BARE_CFF_NATIVE,
            ..Self::type1_native(predefined)
        }
    }
}

/// Parsed, selectable SFNT character mappings, indexed by table and encoding record.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct EncodingTables<'a> {
    cmap: Vec<Option<EncodingTable<'a>>>,
    dmap: Vec<Option<EncodingTable<'a>>>,
}

#[derive(Clone, Yokeable)]
struct EncodingTable<'a> {
    encoding: Encoding,
    subtable: CmapSubtable<'a>,
    delta: Option<CmapSubtable<'a>>,
}

impl<'a> EncodingTables<'a> {
    pub(crate) fn read(font: &impl TableProvider<'a>) -> Self {
        let read = |cmap: Option<Cmap<'a>>, source| {
            cmap.into_iter()
                .flat_map(|cmap| {
                    cmap.encoding_records()
                        .iter()
                        .enumerate()
                        .map(move |(index, record)| {
                            EncodingTable::read(index as u32 | source, record, cmap.offset_data())
                        })
                })
                .collect::<Vec<_>>()
        };
        let dmap = font.dmap().ok().map(|table| table.as_cmap());
        let mut tables = Self {
            cmap: read(font.cmap().ok(), 0),
            dmap: read(dmap.clone(), Encoding::DMAP),
        };
        let unicode_delta = dmap
            .and_then(|table| table.best_subtable())
            .and_then(|(index, _, _)| tables.dmap.get(index as usize)?.as_ref())
            .filter(|table| table.encoding.kind == EncodingKind::Unicode);
        for base in tables.cmap.iter_mut().flatten() {
            let matches = |delta: &&EncodingTable<'_>| {
                delta.encoding.kind == base.encoding.kind
                    && (base.encoding.kind != EncodingKind::None
                        || delta.encoding.sfnt_ids() == base.encoding.sfnt_ids())
            };
            let delta = (base.encoding.kind == EncodingKind::Unicode)
                .then_some(unicode_delta)
                .flatten()
                .or_else(|| {
                    tables
                        .dmap
                        .iter()
                        .flatten()
                        .find(|delta| delta.encoding.sfnt_ids() == base.encoding.sfnt_ids())
                })
                .or_else(|| tables.dmap.iter().flatten().find(matches));
            base.delta = delta.map(|table| table.subtable.clone());
        }
        tables
    }

    pub(crate) fn iter(&self) -> impl Iterator<Item = Encoding> + '_ {
        self.cmap
            .iter()
            .flatten()
            .chain(self.dmap.iter().flatten().filter(|delta| {
                !self.cmap.iter().flatten().any(|base| {
                    base.encoding.kind == delta.encoding.kind
                        && (base.encoding.kind != EncodingKind::None
                            || base.encoding.sfnt_ids() == delta.encoding.sfnt_ids())
                })
            }))
            .map(|table| table.encoding)
    }

    fn table(&self, encoding: Encoding) -> Option<&EncodingTable<'a>> {
        let (tag, index) = encoding.sfnt_index()?;
        let tables = if tag == Tag::new(b"DMAP") {
            &self.dmap
        } else {
            &self.cmap
        };
        tables
            .get(index as usize)?
            .as_ref()
            .filter(|table| table.encoding == encoding)
    }

    pub(crate) fn map(&self, encoding: Encoding, code: u32) -> Option<GlyphId> {
        let table = self.table(encoding)?;
        table
            .delta
            .as_ref()
            .and_then(|delta| delta.map_codepoint(code))
            .filter(|glyph| *glyph != GlyphId::NOTDEF)
            .or_else(|| table.subtable.map_codepoint(code))
    }

    fn codes(
        &self,
        encoding: Encoding,
        limits: CmapIterLimits,
    ) -> impl Iterator<Item = (u32, GlyphId)> + '_ {
        self.table(encoding).into_iter().flat_map(move |table| {
            table
                .delta
                .as_ref()
                .into_iter()
                .flat_map(move |delta| iter_subtable(delta, limits))
                .filter(|(_, glyph)| *glyph != GlyphId::NOTDEF)
                .chain(iter_subtable(&table.subtable, limits).filter(|(code, _)| {
                    table
                        .delta
                        .as_ref()
                        .and_then(|delta| delta.map_codepoint(*code))
                        .is_none_or(|glyph| glyph == GlyphId::NOTDEF)
                }))
        })
    }
}

fn iter_subtable<'a>(
    subtable: &'a CmapSubtable<'a>,
    limits: CmapIterLimits,
) -> impl Iterator<Item = (u32, GlyphId)> + 'a {
    subtable
        .iter_with_limits(limits)
        .chain((0_u32..=255).filter_map(move |code| {
            matches!(subtable, CmapSubtable::Format0(_))
                .then(|| subtable.map_codepoint(code))
                .flatten()
                .map(|glyph| (code, glyph))
        }))
}

impl<'a> EncodingTable<'a> {
    fn read(index: u32, record: &EncodingRecord, data: FontData<'a>) -> Option<Self> {
        let subtable = record.subtable(data).ok()?;
        match subtable {
            CmapSubtable::Format0(_)
            | CmapSubtable::Format4(_)
            | CmapSubtable::Format6(_)
            | CmapSubtable::Format10(_)
            | CmapSubtable::Format12(_)
            | CmapSubtable::Format13(_) => {}
            _ => return None,
        }
        let platform = record.platform_id();
        let id = record.encoding_id();
        let kind = match platform {
            PlatformId::Unicode | PlatformId::ISO => EncodingKind::Unicode,
            PlatformId::Macintosh if id == 0 => EncodingKind::AppleRoman,
            PlatformId::Windows => match id {
                0 => EncodingKind::MsSymbol,
                1 | 10 => EncodingKind::Unicode,
                2 => EncodingKind::Sjis,
                3 => EncodingKind::Prc,
                4 => EncodingKind::Big5,
                5 => EncodingKind::Wansung,
                6 => EncodingKind::Johab,
                _ => EncodingKind::None,
            },
            _ => EncodingKind::None,
        };
        Some(EncodingTable {
            encoding: Encoding {
                kind,
                index,
                platform_id: platform as u16,
                encoding_id: id,
            },
            subtable,
            delta: None,
        })
    }
}

/// The selected cmap subtables and the metadata needed to use them.
#[derive(Clone, Default, Yokeable)]
pub(crate) struct SfntUnicodeCharmap<'a> {
    cmap: UnicodeSubtables<'a>,
    dmap: UnicodeSubtables<'a>,
}

#[derive(Clone, Default, Yokeable)]
struct UnicodeSubtables<'a> {
    subtable: Option<CmapSubtable<'a>>,
    vs_subtable: Option<UvsSubtable<'a>>,
    is_symbol: bool,
    is_mac_roman: bool,
}

impl<'a> SfntUnicodeCharmap<'a> {
    pub(crate) fn read(font: &impl TableProvider<'a>) -> Self {
        Self {
            cmap: UnicodeSubtables::read(font.cmap().ok()),
            dmap: UnicodeSubtables::read(font.dmap().ok().map(|table| table.as_cmap())),
        }
    }

    pub(crate) fn map(&self, codepoint: u32) -> Option<GlyphId> {
        self.dmap
            .map(codepoint)
            .filter(|glyph| *glyph != GlyphId::NOTDEF)
            .or_else(|| self.cmap.map(codepoint))
    }

    /// Writes the mapping of each codepoint to its slot and returns the
    /// number of initial consecutive nonzero mappings.
    ///
    /// `DMAP` overrides `cmap` codepoint by codepoint, so only a font without
    /// one runs through the cached lookups of a single subtable.
    fn map_batched<'o>(&self, codepoints: impl Iterator<Item = (u32, &'o mut GlyphId)>) -> usize {
        if self.dmap.subtable.is_none() {
            return self.cmap.map_batched(codepoints);
        }
        let mut mapped = 0;
        let mut initial = true;
        for (codepoint, out) in codepoints {
            *out = self.map(codepoint).unwrap_or(GlyphId::NOTDEF);
            initial &= *out != GlyphId::NOTDEF;
            mapped += usize::from(initial);
        }
        mapped
    }

    pub(crate) fn map_variant(&self, codepoint: u32, selector: u32) -> Option<GlyphId> {
        let variant = self
            .dmap
            .vs_subtable
            .as_ref()
            .and_then(|table| table.map_variant(codepoint, selector))
            .or_else(|| {
                self.cmap
                    .vs_subtable
                    .as_ref()?
                    .map_variant(codepoint, selector)
            })?;
        match variant {
            MapVariant::UseDefault => self.map(codepoint),
            MapVariant::Variant(glyph) => Some(glyph),
        }
    }

    fn iter(&self, limits: CmapIterLimits) -> impl Iterator<Item = (u32, GlyphId)> + '_ {
        self.dmap
            .iter(limits)
            .filter(|(_, glyph)| *glyph != GlyphId::NOTDEF)
            .chain(self.cmap.iter(limits).filter(|(codepoint, _)| {
                self.dmap
                    .map(*codepoint)
                    .is_none_or(|glyph| glyph == GlyphId::NOTDEF)
            }))
    }

    fn variant_mappings(
        &self,
        limits: CmapIterLimits,
    ) -> impl Iterator<Item = (u32, u32, MapVariant)> + '_ {
        self.dmap
            .vs_subtable
            .as_ref()
            .into_iter()
            .flat_map(move |table| table.iter_with_limits(limits))
            .chain(
                self.cmap
                    .vs_subtable
                    .as_ref()
                    .into_iter()
                    .flat_map(move |table| table.iter_with_limits(limits))
                    .filter(|(codepoint, selector, _)| {
                        self.dmap
                            .vs_subtable
                            .as_ref()
                            .and_then(|table| table.map_variant(*codepoint, *selector))
                            .is_none()
                    }),
            )
    }
}

impl<'a> UnicodeSubtables<'a> {
    fn read(cmap: Option<Cmap<'a>>) -> Self {
        let Some(cmap) = cmap else {
            return Self::default();
        };
        let (subtable, is_symbol, is_mac_roman) = cmap
            .best_subtable()
            .map(|(_, record, subtable)| {
                (Some(subtable), record.is_symbol(), record.is_mac_roman())
            })
            .unwrap_or((None, false, false));
        Self {
            subtable,
            vs_subtable: cmap.uvs_subtable().map(|(_, subtable)| subtable),
            is_symbol,
            is_mac_roman,
        }
    }

    pub(crate) fn map(&self, codepoint: u32) -> Option<GlyphId> {
        let subtable = self.subtable.as_ref()?;
        self.map_with(codepoint, |codepoint| subtable.map_codepoint(codepoint))
    }

    fn map_with(
        &self,
        mut codepoint: u32,
        mut lookup: impl FnMut(u32) -> Option<GlyphId>,
    ) -> Option<GlyphId> {
        // A codepoint Mac Roman cannot encode maps to nothing.
        if self.is_mac_roman && codepoint > 0x7f {
            codepoint = char::from_u32(codepoint)
                .and_then(|value| MacRomanMapping.encode(value))
                .map(u32::from)?;
        }
        let result = lookup(codepoint);
        if result.is_none_or(|glyph| glyph == GlyphId::NOTDEF)
            && self.is_symbol
            && codepoint <= 0xff
        {
            return lookup(0xf000 + codepoint);
        }
        result
    }

    fn map_batched<'o>(&self, codepoints: impl Iterator<Item = (u32, &'o mut GlyphId)>) -> usize {
        let mut mapped = 0;
        let mut initial = true;
        match self.subtable.as_ref() {
            Some(CmapSubtable::Format4(subtable)) => {
                let mut cached = None;
                for (codepoint, out) in codepoints {
                    *out = self
                        .map_with(codepoint, |codepoint| {
                            subtable.map_codepoint_cached(codepoint, &mut cached)
                        })
                        .unwrap_or(GlyphId::NOTDEF);
                    initial &= *out != GlyphId::NOTDEF;
                    mapped += usize::from(initial);
                }
            }
            Some(CmapSubtable::Format12(subtable)) => {
                let mut cached = None;
                for (codepoint, out) in codepoints {
                    *out = self
                        .map_with(codepoint, |codepoint| {
                            subtable.map_codepoint_cached(codepoint, &mut cached)
                        })
                        .unwrap_or(GlyphId::NOTDEF);
                    initial &= *out != GlyphId::NOTDEF;
                    mapped += usize::from(initial);
                }
            }
            Some(subtable) => {
                for (codepoint, out) in codepoints {
                    *out = self
                        .map_with(codepoint, |codepoint| subtable.map_codepoint(codepoint))
                        .unwrap_or(GlyphId::NOTDEF);
                    initial &= *out != GlyphId::NOTDEF;
                    mapped += usize::from(initial);
                }
            }
            None => {
                for (_, out) in codepoints {
                    *out = GlyphId::NOTDEF;
                }
            }
        }
        mapped
    }

    fn iter(&self, limits: CmapIterLimits) -> impl Iterator<Item = (u32, GlyphId)> + '_ {
        let roman = self
            .is_mac_roman
            .then_some(self)
            .into_iter()
            .flat_map(|map| {
                (u8::MIN..=u8::MAX)
                    .map(|code| MacRomanMapping.decode(code) as u32)
                    .filter_map(move |codepoint| map.map(codepoint).map(|glyph| (codepoint, glyph)))
            });
        let normal = (!self.is_mac_roman)
            .then_some(self)
            .into_iter()
            .flat_map(move |map| {
                map.subtable
                    .as_ref()
                    .into_iter()
                    .flat_map(move |subtable| subtable.iter_with_limits(limits))
            });
        roman.chain(normal)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        model::{Blob, Font},
        FontRef,
    };
    use alloc::{sync::Arc, vec};
    use core::sync::atomic::{AtomicUsize, Ordering};
    use types::Tag;

    // The iterators borrow the font, not the charmap they came from, so
    // they outlive it.
    #[test]
    fn iterators_outlive_the_charmap() {
        let font = Font::new(font_test_data::CMAP14_FONT1, 0).unwrap();
        let unicodes = font.charmap().unicodes();
        let variants = font.charmap().unicode_variants();
        let encodings: Vec<_> = font.charmap().encodings().collect();
        let codes = font.charmap().codes(encodings[0]);
        assert!(unicodes.count() > 0);
        assert!(variants.count() > 0);
        assert!(codes.count() > 0);
    }

    fn check_unicode_batch(data: &'static [u8], codepoints: &[u32]) {
        let font = Font::new(data, 0).unwrap();
        let charmap = font.charmap();
        let expected: Vec<_> = codepoints
            .iter()
            .map(|&codepoint| charmap.map_unicode(codepoint).unwrap_or(GlyphId::NOTDEF))
            .collect();
        let mut actual = vec![GlyphId::new(123); codepoints.len()];
        let mapped = charmap.map_unicode_batched(codepoints.iter().copied().zip(actual.iter_mut()));
        assert_eq!(actual, expected);
        assert_eq!(
            mapped,
            expected
                .iter()
                .take_while(|&&glyph| glyph != GlyphId::NOTDEF)
                .count()
        );
    }

    #[test]
    fn batched_unicode_matches_scalar_across_cmap_formats() {
        let codepoints = [
            0, 0x20, 0x41, 0x42, 0xe9, 0x400, 0xf000, 0xffff, 0x10000, 0x10ffff, 0x110000, 0x41,
            0x20, 0,
        ];
        for data in [
            font_test_data::TINOS_SUBSET,
            font_test_data::CMAP12_FONT1,
            font_test_data::CMAP4_SYMBOL_PUA,
            font_test_data::CMAP6,
            font_test_data::NOTO_SERIF_DISPLAY_TRIMMED,
            font_test_data::NOTO_COLOR_EMOJI_FLAGS,
        ] {
            check_unicode_batch(data, &codepoints);
        }
        check_unicode_batch(
            font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB,
            &codepoints,
        );

        let font = Font::new(font_test_data::TINOS_SUBSET, 0).unwrap();
        let mut from_chars = [GlyphId::NOTDEF; 3];
        let mapped = font.charmap().map_unicode_batched(
            ['A', 'B', 'C']
                .into_iter()
                .map(u32::from)
                .zip(from_chars.iter_mut()),
        );
        assert_eq!(
            from_chars,
            ['A', 'B', 'C'].map(|ch| font.charmap().map_unicode(ch).unwrap_or(GlyphId::NOTDEF))
        );
        assert_eq!(
            mapped,
            from_chars
                .iter()
                .take_while(|&&glyph| glyph != GlyphId::NOTDEF)
                .count()
        );
    }

    #[test]
    fn batched_unicode_handles_large_ordered_and_reversed_inputs() {
        let codepoints: Vec<_> = (0..=0x11000).step_by(7).collect();
        for data in [font_test_data::TINOS_SUBSET, font_test_data::CMAP12_FONT1] {
            check_unicode_batch(data, &codepoints);
            check_unicode_batch(data, &codepoints.iter().copied().rev().collect::<Vec<_>>());
        }
    }

    // DMAP overrides cmap codepoint by codepoint and supplies mappings cmap
    // lacks, so a batch has to agree with single lookups when it is present.
    #[test]
    fn batched_unicode_honors_dmap() {
        use font_test_data::cmap::{font_with_cmaps, format12, table};
        let cmap = format12(&[(65, 1), (66, 2), (67, 3)]);
        let dmap = format12(&[(66, 9), (68, 10)]);
        let data = font_with_cmaps(
            Some(&table(&[(3, 10, cmap.as_slice())])),
            Some(&table(&[(3, 10, dmap.as_slice())])),
        );
        let font = Font::new(data, 0).unwrap();
        let charmap = font.charmap();
        let codepoints = [65, 66, 67, 68, 69, 66];
        let mut actual = [GlyphId::new(123); 6];
        let mapped = charmap.map_unicode_batched(codepoints.iter().copied().zip(actual.iter_mut()));
        let expected = [1, 9, 3, 10, 0, 9].map(GlyphId::new);
        assert_eq!(actual, expected);
        assert_eq!(mapped, 4);
        for (codepoint, glyph) in codepoints.iter().zip(expected) {
            assert_eq!(
                charmap.map_unicode(*codepoint).unwrap_or(GlyphId::NOTDEF),
                glyph
            );
        }
    }

    #[test]
    fn cmap15_selection_and_dmap_variants() {
        use font_test_data::cmap::{font_with_cmaps, format12, format14, format15, table};
        let nominal = format12(&[(65, 1)]);
        let old = format14(0xfe0f, &[], &[(65, 2), (67, 3)]);
        let new = format15(0xfe0f, &[65], &[(66, 70000), (68, 0xffffff)]);
        for records in [
            vec![
                (3, 10, nominal.as_slice()),
                (0, 5, old.as_slice()),
                (0, 5, new.as_slice()),
            ],
            vec![
                (3, 10, nominal.as_slice()),
                (0, 5, new.as_slice()),
                (0, 5, old.as_slice()),
            ],
        ] {
            let base = table(&records);
            let delta = table(&[(0, 5, old.as_slice())]);
            for (dmap, expected) in [
                (None, vec![(65, 1), (66, 70000), (68, 0xffffff)]),
                (
                    Some(delta.as_slice()),
                    vec![(65, 2), (66, 70000), (67, 3), (68, 0xffffff)],
                ),
            ] {
                let data = font_with_cmaps(Some(&base), dmap);
                let font = Font::new(data, 0).unwrap();
                let charmap = font.charmap();
                assert!(charmap.has_unicode_variants());
                let mut mappings: Vec<_> = charmap.unicode_variants().collect();
                mappings.sort_unstable();
                assert_eq!(
                    mappings,
                    expected
                        .iter()
                        .map(|&(ch, glyph)| (ch, 0xfe0f, GlyphId::new(glyph)))
                        .collect::<Vec<_>>()
                );
                for (ch, selector, glyph) in mappings {
                    assert_eq!(charmap.map_unicode_variant(ch, selector), Some(glyph));
                }
            }
        }
        let delta = table(&[(0, 5, new.as_slice())]);
        let data = font_with_cmaps(None, Some(&delta));
        let font = Font::new(data, 0).unwrap();
        assert_eq!(
            font.charmap().map_unicode_variant(66u32, 0xfe0fu32),
            Some(GlyphId::new(70000))
        );
    }

    #[test]
    fn dmap_unicode_lookups_and_iterators() {
        use font_test_data::cmap::{font_with_cmaps, format12, format14, table};
        let base = table(&[
            (3, 10, &format12(&[(65, 1), (66, 2), (68, 5)])),
            (0, 5, &format14(0xfe0f, &[66], &[(65, 5), (67, 6)])),
        ]);
        let delta = table(&[
            (0, 4, &format12(&[(65, 3), (66, 0), (67, 4)])),
            (0, 5, &format14(0xfe0f, &[65], &[(66, 4), (68, 7)])),
        ]);
        let data = font_with_cmaps(Some(&base), Some(&delta));
        let font = Font::new(data, 0).unwrap();
        let charmap = font.charmap();
        assert!(charmap.has_unicode());
        assert!(charmap.has_unicode_variants());
        let mut mappings: Vec<_> = charmap.unicodes().collect();
        mappings.sort_unstable();
        assert_eq!(
            mappings,
            vec![
                (65, GlyphId::new(3)),
                (66, GlyphId::new(2)),
                (67, GlyphId::new(4)),
                (68, GlyphId::new(5))
            ]
        );
        for (ch, glyph) in mappings {
            assert_eq!(charmap.map_unicode(ch), Some(glyph));
        }
        for encoding in charmap.encodings() {
            assert_eq!(encoding.sfnt_index().unwrap().0, Tag::new(b"cmap"));
            assert_eq!(charmap.map_code(encoding, 65), Some(GlyphId::new(3)));
            let mut codes: Vec<_> = charmap.codes(encoding).collect();
            codes.sort_unstable();
            assert_eq!(
                codes,
                vec![
                    (65, GlyphId::new(3)),
                    (66, GlyphId::new(2)),
                    (67, GlyphId::new(4)),
                    (68, GlyphId::new(5))
                ]
            );
        }
        let mut variants: Vec<_> = charmap.unicode_variants().collect();
        variants.sort_unstable();
        assert_eq!(
            variants,
            vec![
                (65, 0xfe0f, GlyphId::new(3)),
                (66, 0xfe0f, GlyphId::new(4)),
                (67, 0xfe0f, GlyphId::new(6)),
                (68, 0xfe0f, GlyphId::new(7))
            ]
        );
        for (ch, selector, glyph) in variants {
            assert_eq!(charmap.map_unicode_variant(ch, selector), Some(glyph));
        }
        let data = font_with_cmaps(None, Some(&delta));
        let font = Font::new(data, 0).unwrap();
        assert!(font.charmap().has_unicode());
        assert_eq!(font.charmap().map_unicode('A'), Some(GlyphId::new(3)));
        assert_eq!(
            font.charmap().map_unicode_variant('A', 0xfe0fu32),
            Some(GlyphId::new(3))
        );
        let encoding = font.charmap().encodings().next().unwrap();
        // The index is into DMAP, not cmap.
        assert_eq!(encoding.sfnt_index(), Some((Tag::new(b"DMAP"), 0)));
        assert_eq!(font.charmap().map_code(encoding, 65), Some(GlyphId::new(3)));
    }

    #[test]
    fn standalone_cff_batch_matches_scalar_and_writes_after_a_miss() {
        let sfnt = FontRef::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED).unwrap();
        let cff = sfnt.cff().unwrap().offset_data().as_bytes();
        let codepoints = ['i' as u32, 0x10ffff, 'i' as u32];
        check_unicode_batch(cff, &codepoints);

        let font = Font::new(cff, 0).unwrap();
        let mut glyphs = [GlyphId::new(123); 3];
        let mapped = font
            .charmap()
            .map_unicode_batched(codepoints.into_iter().zip(glyphs.iter_mut()));
        #[cfg(feature = "agl")]
        {
            assert_eq!(mapped, 1);
            assert_eq!(glyphs, [GlyphId::new(1), GlyphId::NOTDEF, GlyphId::new(1)]);
        }
        #[cfg(not(feature = "agl"))]
        {
            assert_eq!(mapped, 0);
            assert_eq!(glyphs, [GlyphId::NOTDEF; 3]);
        }
    }

    #[test]
    fn cid_cff_has_no_glyph_name_unicode_mapping() {
        let sfnt = FontRef::new(font_test_data::NOTO_SANS_JP_CFF).unwrap();
        let cff = sfnt.cff().unwrap().offset_data().as_bytes();
        let font = Font::new(cff, 0).unwrap();
        let charmap = font.charmap();
        assert!(!charmap.has_unicode());
        assert_eq!(charmap.map_unicode('A'), None);
        assert_eq!(charmap.encodings().count(), 0);
        assert_eq!(
            charmap.map_code(Encoding::standalone_cff_native(None), 65),
            None
        );
        let mut glyphs = [GlyphId::new(123); 2];
        assert_eq!(
            charmap.map_unicode_batched([0x41, 0x42].into_iter().zip(glyphs.iter_mut())),
            0
        );
        assert_eq!(glyphs, [GlyphId::NOTDEF; 2]);
    }

    #[test]
    fn maps_a_selected_unicode_subtable() {
        let font = Font::new(font_test_data::VAZIRMATN_VAR, 0).unwrap();
        assert!(font.charmap().has_unicode());
        assert!(!font.charmap().unicode_is_symbol());
        assert!(!font.charmap().unicode_is_mac_roman());
        assert_eq!(font.charmap().map_unicode('A'), Some(GlyphId::new(1)));
        assert_eq!(
            font.charmap().map_unicode('A' as u32),
            Some(GlyphId::new(1))
        );
        assert_eq!(font.charmap().map_unicode('B'), None);
        assert_eq!(font.charmap().map_unicode(0xffff_u32), None);
        assert!(font
            .charmap()
            .unicodes()
            .any(|entry| entry == ('A' as u32, GlyphId::new(1))));
    }

    #[test]
    fn selectable_mappings_are_fixed_size_and_keep_record_identity() {
        assert_eq!(core::mem::size_of::<Encoding>(), 12);
        assert_eq!(EncodingKind::Unicode as u32, u32::from_be_bytes(*b"unic"));
        assert_eq!(
            EncodingKind::AppleRoman as u32,
            u32::from_be_bytes(*b"armn")
        );

        let data = font_test_data::VORG;
        let font = Font::new(data, 0).unwrap();
        let direct = FontRef::new(data).unwrap();
        let cmap = direct.cmap().unwrap();
        let encodings: Vec<_> = font.charmap().encodings().collect();
        for encoding in &encodings {
            let (tag, index) = encoding.sfnt_index().unwrap();
            assert_eq!(tag, Tag::new(b"cmap"));
            let index = index as usize;
            let record = &cmap.encoding_records()[index];
            assert_eq!(
                encoding.sfnt_ids(),
                Some((record.platform_id() as u16, record.encoding_id()))
            );
            let subtable = record.subtable(cmap.offset_data()).unwrap();
            assert_eq!(
                font.charmap().map_code(*encoding, 'A' as u32),
                subtable.map_codepoint('A')
            );
            for (code, glyph) in font.charmap().codes(*encoding).take(3) {
                assert_eq!(font.charmap().map_code(*encoding, code), Some(glyph));
            }
        }
        assert!(
            encodings
                .iter()
                .filter(|encoding| encoding.kind() == EncodingKind::Unicode)
                .count()
                > 1
        );
        assert!(encodings.windows(2).all(|pair| pair[0] != pair[1]));
    }

    #[test]
    #[cfg(feature = "agl")]
    fn type1_exposes_unicode_and_native_mappings() {
        let font = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB, 0).unwrap();
        let encodings: Vec<_> = font.charmap().encodings().collect();
        let unicode = encodings
            .iter()
            .find(|encoding| encoding.kind() == EncodingKind::Unicode)
            .unwrap();
        assert_eq!(unicode.sfnt_index(), None);
        assert_eq!(
            font.charmap().map_code(*unicode, 'H' as u32),
            font.charmap().map_unicode('H')
        );
        let native = encodings
            .iter()
            .find(|encoding| encoding.kind() != EncodingKind::Unicode)
            .unwrap();
        assert_eq!(native.sfnt_index(), None);
        assert_eq!(
            font.charmap().map_code(*native, b'H' as u32),
            Some(GlyphId::new(1))
        );
        assert_eq!(font.charmap().map_code(*native, 256), None);
        assert!(font
            .charmap()
            .unicodes()
            .any(|entry| entry == ('H' as u32, GlyphId::new(1))));
        assert!(font
            .charmap()
            .codes(*native)
            .any(|entry| entry == (b'H' as u32, GlyphId::new(1))));
    }

    #[test]
    fn selected_subtables_are_cached_across_font_instances() {
        let cmap_reads = Arc::new(AtomicUsize::new(0));
        let count = cmap_reads.clone();
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"cmap") {
                count.fetch_add(1, Ordering::Relaxed);
            }
            let font = FontRef::new(font_test_data::VAZIRMATN_VAR).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let charmap = font.charmap();
        assert_eq!(cmap_reads.load(Ordering::Relaxed), 0);
        assert_eq!(charmap.map_unicode('A'), Some(GlyphId::new(1)));
        let first_reads = cmap_reads.load(Ordering::Relaxed);
        assert!(first_reads > 0);
        assert_eq!(font.charmap().map_unicode('A'), Some(GlyphId::new(1)));
        assert_eq!(
            font.default_instance().charmap().map_unicode('A'),
            Some(GlyphId::new(1))
        );
        let varied = font
            .instance_builder()
            .variations([("wght", 500.0)])
            .build();
        assert!(!varied.normalized_coords().is_empty());
        assert_eq!(varied.charmap().map_unicode('A'), Some(GlyphId::new(1)));
        assert_eq!(cmap_reads.load(Ordering::Relaxed), first_reads);
    }

    #[test]
    fn resolves_default_and_non_default_variants() {
        let font = Font::new(font_test_data::CMAP14_FONT1, 0).unwrap();
        assert!(font.charmap().has_unicode_variants());
        let selector = '\u{e0100}';
        assert_eq!(
            font.charmap().map_unicode_variant('\u{4e00}', selector),
            font.charmap().map_unicode('\u{4e00}')
        );
        assert_eq!(
            font.charmap().map_unicode_variant('\u{4e08}', selector),
            Some(GlyphId::new(25))
        );
        assert_eq!(
            font.charmap()
                .map_unicode_variant(0x4e08_u32, selector as u32),
            Some(GlyphId::new(25))
        );
        assert_eq!(font.charmap().map_unicode_variant('a', selector), None);
        let variants: Vec<_> = font.charmap().unicode_variants().collect();
        assert!(variants.contains(&(
            0x4e00,
            selector as u32,
            font.charmap().map_unicode(0x4e00_u32).unwrap()
        )));
        assert!(variants.contains(&(0x4e08, selector as u32, GlyphId::new(25))));
    }

    #[test]
    fn symbol_encoding_falls_back_to_the_private_use_page() {
        let data = font_test_data::CMAP4_SYMBOL_PUA;
        let direct = FontRef::new(data).unwrap();
        let (_, _, subtable) = direct.cmap().unwrap().best_subtable().unwrap();
        let (codepoint, glyph) = (0xf000..=0xf0ff)
            .find_map(|codepoint| {
                subtable
                    .map_codepoint(codepoint)
                    .map(|glyph| (codepoint, glyph))
            })
            .unwrap();
        let character = char::from_u32(codepoint - 0xf000).unwrap();
        assert_eq!(subtable.map_codepoint(character), None);
        let font = Font::new(data, 0).unwrap();
        assert!(font.charmap().has_unicode());
        assert!(font.charmap().unicode_is_symbol());
        assert!(!font.charmap().unicode_is_mac_roman());
        assert_eq!(font.charmap().map_unicode(character), Some(glyph));
        let unicodes: Vec<_> = font.charmap().unicodes().collect();
        assert!(unicodes.contains(&(codepoint, glyph)));
        assert!(!unicodes.contains(&(character as u32, glyph)));
        let encoding = font.charmap().encodings().next().unwrap();
        assert_eq!(font.charmap().map_code(encoding, codepoint), Some(glyph));
        assert!(font
            .charmap()
            .codes(encoding)
            .any(|entry| entry == (codepoint, glyph)));
    }

    #[test]
    fn symbol_encoding_retries_when_direct_mapping_is_notdef() {
        let mut cmap = vec![0, 0, 0, 1, 0, 3, 0, 0, 0, 0, 0, 12];
        // Format 12: U+0041 maps to .notdef, while U+F041 maps to glyph 3.
        cmap.extend_from_slice(&[0, 12, 0, 0]);
        cmap.extend_from_slice(&40_u32.to_be_bytes());
        cmap.extend_from_slice(&0_u32.to_be_bytes());
        cmap.extend_from_slice(&2_u32.to_be_bytes());
        for (codepoint, glyph) in [(0x41_u32, 0_u32), (0xf041, 3)] {
            cmap.extend_from_slice(&codepoint.to_be_bytes());
            cmap.extend_from_slice(&codepoint.to_be_bytes());
            cmap.extend_from_slice(&glyph.to_be_bytes());
        }
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"cmap") {
                return Some(Blob::from(cmap.clone()));
            }
            let font = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let charmap = font.charmap();
        assert_eq!(charmap.map_unicode('A'), Some(GlyphId::new(3)));
        let mut batched = [GlyphId::NOTDEF; 3];
        let mapped = charmap.map_unicode_batched(
            ['A', 'B', 'A']
                .into_iter()
                .map(u32::from)
                .zip(batched.iter_mut()),
        );
        assert_eq!(batched, [GlyphId::new(3), GlyphId::NOTDEF, GlyphId::new(3)]);
        assert_eq!(mapped, 1);
        let unicodes: Vec<_> = charmap.unicodes().collect();
        assert!(unicodes.contains(&(0xf041, GlyphId::new(3))));
        assert!(!unicodes.iter().any(|(code, _)| *code == 0x41));
        let encoding = charmap.encodings().next().unwrap();
        assert_eq!(charmap.map_code(encoding, 0x41), None);
        assert!(!charmap.codes(encoding).any(|(code, _)| code == 0x41));
    }

    /// Creates a font with Tinos's tables and one format 0 `cmap` subtable.
    ///
    /// The subtable has the given `platform` and `encoding` IDs and maps
    /// each code to the glyph at that position in `glyphs`.
    fn font_with_format0_cmap(platform: u8, encoding: u8, glyphs: [u8; 256]) -> Font {
        let mut cmap = vec![0, 0, 0, 1, 0, platform, 0, encoding, 0, 0, 0, 12];
        cmap.extend_from_slice(&[0, 0, 1, 6, 0, 0]);
        cmap.extend_from_slice(&glyphs);
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"cmap") {
                return Some(Blob::from(cmap.clone()));
            }
            let font = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        Font::new(source, 0).unwrap()
    }

    #[test]
    fn mac_roman_encoding_translates_unicode() {
        let mut glyphs = [0u8; 256];
        glyphs[0x8e] = 3; // U+00E9, encoded as Mac Roman 0x8E.
        glyphs[0] = 5;
        let font = font_with_format0_cmap(1, 0, glyphs);
        assert!(font.charmap().has_unicode());
        assert!(!font.charmap().unicode_is_symbol());
        assert!(font.charmap().unicode_is_mac_roman());
        assert_eq!(font.charmap().map_unicode('é'), Some(GlyphId::new(3)));
        assert_eq!(font.charmap().map_unicode('A'), None);
        // Mac Roman cannot encode U+0100, so it maps to nothing rather than
        // to what code 0 maps to.
        assert_eq!(font.charmap().map_unicode('\u{100}'), None);
        let mut batch = [GlyphId::new(9)];
        font.charmap()
            .map_unicode_batched([0x100u32].into_iter().zip(batch.iter_mut()));
        assert_eq!(batch, [GlyphId::NOTDEF]);
        assert!(font
            .charmap()
            .unicodes()
            .any(|entry| entry == ('é' as u32, GlyphId::new(3))));
        let encoding = font.charmap().encodings().next().unwrap();
        assert_eq!(
            font.charmap().map_code(encoding, 0x8e),
            Some(GlyphId::new(3))
        );
        assert!(font
            .charmap()
            .codes(encoding)
            .any(|entry| entry == (0x8e, GlyphId::new(3))));
    }

    #[test]
    fn format0_mappings_are_listed_once() {
        let mut glyphs = [0u8; 256];
        glyphs[0x41] = 3;
        glyphs[0x42] = 4;
        let font = font_with_format0_cmap(3, 1, glyphs);
        assert!(!font.charmap().unicode_is_mac_roman());
        let mappings: Vec<_> = font.charmap().unicodes().collect();
        let mut unique = mappings.clone();
        unique.sort();
        unique.dedup();
        assert_eq!(mappings.len(), unique.len());
        assert!(mappings.contains(&(0x41, GlyphId::new(3))));
        assert!(mappings.contains(&(0x42, GlyphId::new(4))));
    }

    #[test]
    #[cfg(feature = "agl")]
    fn type1_uses_its_unicode_charmap() {
        let font = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB, 0).unwrap();
        assert!(font.charmap().has_unicode());
        assert!(!font.charmap().unicode_is_symbol());
        assert!(!font.charmap().unicode_is_mac_roman());
        assert!(!font.charmap().has_unicode_variants());
        assert_eq!(font.charmap().map_unicode('H'), Some(GlyphId::new(1)));
        assert_eq!(font.charmap().map_unicode('a'), None);
        assert_eq!(font.charmap().map_unicode_variant('H', '\u{fe0f}'), None);
    }
}
