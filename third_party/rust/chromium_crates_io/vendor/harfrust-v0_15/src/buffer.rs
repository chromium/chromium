use super::ot::layout::MAX_SYLLABLE_LENGTH;
use super::Mask;
use crate::set_digest::SetDigest;
use crate::unicode::{CharExt, Codepoint};
use crate::U32Set;
use crate::{BufferFlags, ClusterLevel, Direction, Language, Script, SerializeFlags};
#[cfg(feature = "tracing")]
use alloc::boxed::Box;
use alloc::{string::String, vec::Vec};
use core::cmp::min;
use read_fonts::types::{GlyphId, GlyphId16};

use crate::hint::Likely;

const CONTEXT_LENGTH: usize = 5;

/// The advances and offsets of a shaped glyph.
///
/// Values use the scale of the shaping font. Offsets move a glyph relative to
/// the current point without changing the position of the next glyph.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GlyphPosition {
    /// Horizontal advance after drawing this glyph.
    pub x_advance: i32,
    /// Vertical advance after drawing this glyph.
    pub y_advance: i32,
    /// Horizontal offset from the current point.
    pub x_offset: i32,
    /// Vertical offset from the current point.
    pub y_offset: i32,
    pub(crate) var: u32,
}

impl GlyphPosition {
    #[inline]
    pub(crate) fn attach_chain(&self) -> i16 {
        // glyph to which this attaches to, relative to current glyphs;
        // negative for going back, positive for forward.
        let v: &[i16; 2] = bytemuck::cast_ref(&self.var);
        v[0]
    }

    #[inline]
    pub(crate) fn set_attach_chain(&mut self, n: i16) {
        let v: &mut [i16; 2] = bytemuck::cast_mut(&mut self.var);
        v[0] = n;
    }

    #[inline]
    pub(crate) fn attach_type(&self) -> u8 {
        // attachment type
        // Note! if attach_chain() is zero, the value of attach_type() is irrelevant.
        let v: &[u8; 4] = bytemuck::cast_ref(&self.var);
        v[2]
    }

    #[inline]
    pub(crate) fn set_attach_type(&mut self, n: u8) {
        let v: &mut [u8; 4] = bytemuck::cast_mut(&mut self.var);
        v[2] = n;
    }
}

/// A glyph and its relationship to the input text.
///
/// Before shaping, each entry holds a Unicode codepoint. After shaping, it
/// holds a glyph ID and the cluster of input text that produced it.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GlyphInfo {
    // NOTE: Stores a Unicode codepoint before shaping and a glyph ID after.
    //       Just like harfbuzz, we are using the same variable for two purposes.
    /// Unicode codepoint before shaping; glyph ID after shaping.
    ///
    /// Glyph IDs are not confined to `u16`. User-provided glyphs (e.g.
    /// `Buffer::set_invisible_glyph`,
    /// `Buffer::set_not_found_variation_selector_glyph`, or custom
    /// `FontFuncs` (see crate::FontFuncs)) are provided as is and can be larger than `u16::MAX`
    /// or the number of glyphs in the font.
    pub glyph_id: u32,
    pub(crate) mask: Mask,
    /// Index into the original input text for this glyph's cluster.
    ///
    /// [Read more on clusters](https://harfbuzz.github.io/clusters.html).
    pub cluster: u32,
    pub(crate) vars: [u32; 2],
}

#[allow(dead_code)]
pub(crate) struct buffer_var_shape {
    pub(crate) width: u8,
    pub(crate) var_index: u8,
    pub(crate) index: u8,
}

impl buffer_var_shape {
    #[inline]
    pub fn start(&self) -> u8 {
        (self.var_index - 1) * 4 + self.index * self.width
    }

    #[inline]
    pub fn count(&self) -> u8 {
        self.width
    }

    #[inline]
    pub fn bits(&self) -> u8 {
        let start = self.start();
        let end = start + self.count();
        debug_assert!(end <= 8);
        ((1u16 << end) - (1u16 << start)) as u8
    }
}

macro_rules! declare_buffer_var {
    ($ty:ty, $var_index:expr, $index:expr, $var_name:ident, $getter:ident, $setter:ident) => {
        #[allow(dead_code)]
        pub(crate) const $var_name: buffer_var_shape = buffer_var_shape {
            width: core::mem::size_of::<$ty>() as u8,
            var_index: $var_index,
            index: $index,
        };

        #[inline]
        #[allow(dead_code)]
        pub(crate) fn $getter(&self) -> $ty {
            const LEN: usize = core::mem::size_of::<u32>() / core::mem::size_of::<$ty>();
            let v: &[$ty; LEN] = bytemuck::cast_ref(&self.vars[$var_index - 1usize]);
            v[$index]
        }

        #[inline]
        #[allow(dead_code)]
        pub(crate) fn $setter(&mut self, value: $ty) {
            const LEN: usize = core::mem::size_of::<u32>() / core::mem::size_of::<$ty>();
            let v: &mut [$ty; LEN] = bytemuck::cast_mut(&mut self.vars[$var_index - 1usize]);
            v[$index] = value;
        }
    };
}

macro_rules! declare_buffer_var_alias {
    ($alias_var:ident, $ty:ty, $var_name:ident, $getter:ident, $setter:ident) => {
        #[allow(dead_code)]
        pub(crate) const $var_name: buffer_var_shape = GlyphInfo::$alias_var;

        #[inline]
        pub(crate) fn $getter(&self) -> $ty {
            const { assert!(GlyphInfo::$alias_var.width == core::mem::size_of::<$ty>() as u8) };
            const LEN: usize = core::mem::size_of::<u32>() / core::mem::size_of::<$ty>();
            let v: &[$ty; LEN] =
                bytemuck::cast_ref(&self.vars[GlyphInfo::$alias_var.var_index as usize - 1usize]);
            v[GlyphInfo::$alias_var.index as usize]
        }

        #[inline]
        pub(crate) fn $setter(&mut self, value: $ty) {
            const { assert!(GlyphInfo::$alias_var.width == core::mem::size_of::<$ty>() as u8) };
            const LEN: usize = core::mem::size_of::<u32>() / core::mem::size_of::<$ty>();
            let v: &mut [$ty; LEN] = bytemuck::cast_mut(
                &mut self.vars[GlyphInfo::$alias_var.var_index as usize - 1usize],
            );
            v[GlyphInfo::$alias_var.index as usize] = value;
        }
    };
}

/// Flags that describe the properties of a glyph.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub struct GlyphFlags(pub(crate) u32);

impl GlyphFlags {
    /// Breaking before this cluster may change the shaping on either side.
    ///
    /// On the flip side, it means that when this flag is not present,
    /// then it's safe to break the glyph-run at the beginning of this cluster,
    /// and the two sides represent the exact same result one would get if breaking input text
    /// at the beginning of this cluster and shaping the two sides separately.
    /// This can be used to optimize paragraph layout, by avoiding re-shaping of each line
    /// after line-breaking, or limiting the reshaping to a small piece around
    /// the breaking point only.
    pub const UNSAFE_TO_BREAK: Self = Self(0x0000_0001);

    /// Changing text beside this cluster may affect shaping across its boundary.
    /// Note that the absence of this flag will NOT by itself mean that it IS safe to concat
    /// text. Only two pieces of text both of which clear of this flag can be concatenated
    /// safely.
    ///
    /// This can be used to optimize paragraph layout, by avoiding re-shaping of each line after
    /// line-breaking, by limiting the reshaping to a small piece around the breaking position
    /// only, even if the breaking position carries the unsafe-to-break flag or when hyphenation
    /// or other text transformation happens at line-break position, in the following way:
    /// 1. Iterate back from the line-break position until the first cluster start position
    ///    that is NOT unsafe-to-concat.
    /// 2. Shape the segment from there till the end of line.
    /// 3. Check whether the resulting glyph-run also is clear of the unsafe-to-concat at its
    ///    start-of-text position; if it is, just splice it into place and the line is shaped;
    ///    If not, move on to a position further back that is clear of unsafe-to-concat and retry
    ///    from there, and repeat.
    ///
    /// At the start of next line a similar algorithm can be implemented. That is:
    /// 1. Iterate forward from the line-break position until the first cluster start position that is
    ///    NOT unsafe-to-concat.
    /// 2. Shape the segment from beginning of the line to that position.
    /// 3. Check whether the resulting glyph-run also is clear of the unsafe-to-concat at its end-of-text
    ///    position; if it is, just splice it into place and the beginning is shaped; If not, move on to a
    ///    position further forward that is clear of unsafe-to-concat and retry up to there, and repeat. A
    ///    slight complication will arise in the implementation of the algorithm above, because while our
    ///    buffer API has a way to return flags for position corresponding to start-of-text, there is currently
    ///    no position corresponding to end-of-text. This limitation can be alleviated by shaping more text
    ///    than needed and looking for unsafe-to-concat flag within text clusters. The unsafe-to-break flag will
    ///    always imply this flag. To use this flag, you must enable the buffer flag [`BufferFlags::PRODUCE_UNSAFE_TO_CONCAT`]
    ///    during shaping, otherwise the buffer flag will not be reliably produced.
    pub const UNSAFE_TO_CONCAT: Self = Self(0x0000_0002);

    /// A U+0640 TATWEEL can safely be inserted before this cluster.
    ///
    /// In scripts that use elongation (Arabic, Mongolian, Syriac, etc.), this flag does not
    /// determine the script-specific elongation places, but only when it is safe to do the elongation
    /// without interrupting text shaping.
    pub const SAFE_TO_INSERT_TATWEEL: Self = Self(0x0000_0004);

    /// All the currently defined flags.
    pub const ALL: Self = Self(Self::DEFINED_BITS);

    pub(crate) const DEFINED_BITS: u32 = 0x0000_0007; // OR of all defined flags

    /// Creates a `GlyphFlags` from the given bits, ignoring any bits that are not defined.
    pub const fn from_bits_truncate(bits: u32) -> Self {
        Self(bits & Self::DEFINED_BITS)
    }

    /// Returns the underlying flag bits.
    #[inline(always)]
    pub const fn to_bits(self) -> u32 {
        self.0
    }

    /// Indicates that if input text is broken at the beginning of the cluster this glyph
    /// is part of, then both sides need to be re-shaped, as the result might be different.
    ///
    /// See [Self::UNSAFE_TO_BREAK] for more details.
    #[inline(always)]
    pub const fn is_unsafe_to_break(self) -> bool {
        self.intersects(Self::UNSAFE_TO_BREAK)
    }

    /// Indicates that if input text is changed on one side of the beginning of the cluster
    /// this glyph is part of, then the shaping results for the other side might change.
    ///
    /// See [Self::UNSAFE_TO_CONCAT] for more details.
    #[inline(always)]
    pub const fn is_unsafe_to_concat(self) -> bool {
        self.intersects(Self::UNSAFE_TO_CONCAT)
    }

    /// In scripts that use elongation (Arabic, Mongolian, Syriac, etc.), this flag signifies that it is
    /// safe to insert a U+0640 TATWEEL character before this cluster for elongation.
    ///
    /// See [Self::SAFE_TO_INSERT_TATWEEL] for more details.
    #[inline(always)]
    pub const fn is_safe_to_insert_tatweel(self) -> bool {
        self.intersects(Self::SAFE_TO_INSERT_TATWEEL)
    }

    /// Returns `true` if all of the flags in `other` are contained within `self`.
    #[inline(always)]
    pub const fn contains(self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }

    /// Returns `true` if any of the flags in `other` are contained within `self`.
    #[inline(always)]
    pub const fn intersects(self, other: Self) -> bool {
        (self.0 & other.0) != 0
    }
}

impl core::ops::BitOr for GlyphFlags {
    type Output = Self;

    #[inline(always)]
    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

impl core::ops::BitOrAssign for GlyphFlags {
    #[inline(always)]
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl core::ops::BitAnd for GlyphFlags {
    type Output = Self;

    #[inline(always)]
    fn bitand(self, rhs: Self) -> Self::Output {
        Self(self.0 & rhs.0)
    }
}

impl core::ops::BitAndAssign for GlyphFlags {
    #[inline(always)]
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0;
    }
}

impl core::ops::Not for GlyphFlags {
    type Output = Self;

    #[inline(always)]
    fn not(self) -> Self::Output {
        Self(!self.0 & Self::DEFINED_BITS)
    }
}

impl GlyphInfo {
    /// Returns the flags for this glyph.
    #[inline(always)]
    pub const fn flags(&self) -> GlyphFlags {
        GlyphFlags(self.mask)
    }

    /// Indicates that if input text is broken at the beginning of the cluster this glyph
    /// is part of, then both sides need to be re-shaped, as the result might be different.
    ///
    /// On the flip side, it means that when this flag is not present,
    /// then it's safe to break the glyph-run at the beginning of this cluster,
    /// and the two sides represent the exact same result one would get if breaking input text
    /// at the beginning of this cluster and shaping the two sides separately.
    /// This can be used to optimize paragraph layout, by avoiding re-shaping of each line
    /// after line-breaking, or limiting the reshaping to a small piece around
    /// the breaking point only.
    #[inline(always)]
    pub fn unsafe_to_break(&self) -> bool {
        self.flags().is_unsafe_to_break()
    }

    /// Indicates that if input text is changed on one side of the beginning of the cluster
    /// this glyph is part of, then the shaping results for the other side might change.
    /// Note that the absence of this flag will NOT by itself mean that it IS safe to concat
    /// text. Only two pieces of text both of which clear of this flag can be concatenated
    /// safely.
    ///
    /// This can be used to optimize paragraph layout, by avoiding re-shaping of each line after
    /// line-breaking, by limiting the reshaping to a small piece around the breaking position
    /// only, even if the breaking position carries the unsafe-to-break flag or when hyphenation
    /// or other text transformation happens at line-break position, in the following way:
    /// 1. Iterate back from the line-break position until the first cluster start position
    ///    that is NOT unsafe-to-concat.
    /// 2. Shape the segment from there till the end of line.
    /// 3. Check whether the resulting glyph-run also is clear of the unsafe-to-concat at its
    ///    start-of-text position; if it is, just splice it into place and the line is shaped;
    ///    If not, move on to a position further back that is clear of unsafe-to-concat and retry
    ///    from there, and repeat.
    ///
    /// At the start of next line a similar algorithm can be implemented. That is:
    /// 1. Iterate forward from the line-break position until the first cluster start position that is
    ///    NOT unsafe-to-concat.
    /// 2. Shape the segment from beginning of the line to that position.
    /// 3. Check whether the resulting glyph-run also is clear of the unsafe-to-concat at its end-of-text
    ///    position; if it is, just splice it into place and the beginning is shaped; If not, move on to a
    ///    position further forward that is clear of unsafe-to-concat and retry up to there, and repeat. A
    ///    slight complication will arise in the implementation of the algorithm above, because while our
    ///    buffer API has a way to return flags for position corresponding to start-of-text, there is currently
    ///    no position corresponding to end-of-text. This limitation can be alleviated by shaping more text
    ///    than needed and looking for unsafe-to-concat flag within text clusters. The unsafe-to-break flag will
    ///    always imply this flag. To use this flag, you must enable the buffer flag [`BufferFlags::PRODUCE_UNSAFE_TO_CONCAT`]
    ///    during shaping, otherwise the buffer flag will not be reliably produced.
    #[inline(always)]
    pub fn unsafe_to_concat(&self) -> bool {
        self.flags().is_unsafe_to_concat()
    }

    /// In scripts that use elongation (Arabic, Mongolian, Syriac, etc.), this flag signifies that it is
    /// safe to insert a U+0640 TATWEEL character before this cluster for elongation. This flag does not
    /// determine the script-specific elongation places, but only when it is safe to do the elongation
    /// without interrupting text shaping.
    #[inline(always)]
    pub fn safe_to_insert_tatweel(&self) -> bool {
        self.flags().is_safe_to_insert_tatweel()
    }

    pub(crate) fn as_codepoint(&self) -> Codepoint {
        self.glyph_id
    }

    #[inline]
    pub(crate) fn as_glyph(&self) -> GlyphId {
        GlyphId::new(self.glyph_id)
    }

    #[inline]
    pub(crate) fn as_gid16(&self) -> Option<GlyphId16> {
        let gid: u16 = self.glyph_id.try_into().ok()?;
        Some(gid.into())
    }

    pub(crate) fn init_unicode_props(&mut self, scratch_flags: &mut ScratchFlags) {
        let u = self.as_codepoint();
        let gc = u.general_category();
        let mut props = gc.0 as u16;

        if u >= 0x80 {
            if u.is_default_ignorable() {
                props |= UnicodeProps::IGNORABLE.bits();
                *scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_DEFAULT_IGNORABLES;

                match u {
                    0x200C => props |= UnicodeProps::CF_ZWNJ.bits(),
                    0x200D => props |= UnicodeProps::CF_ZWJ.bits(),

                    // Mongolian Free Variation Selectors need to be remembered
                    // because although we need to hide them like default-ignorables,
                    // they need to non-ignorable during shaping.  This is similar to
                    // what we do for joiners in Indic-like shapers, but since the
                    // FVSes are GC=Mn, we have use a separate bit to remember them.
                    // Fixes:
                    // https://github.com/harfbuzz/harfbuzz/issues/234
                    0x180B..=0x180D | 0x180F => props |= UnicodeProps::HIDDEN.bits(),

                    // TAG characters need similar treatment. Fixes:
                    // https://github.com/harfbuzz/harfbuzz/issues/463
                    0xE0020..=0xE007F => props |= UnicodeProps::HIDDEN.bits(),

                    // COMBINING GRAPHEME JOINER should not be skipped during GSUB either.
                    // https://github.com/harfbuzz/harfbuzz/issues/554
                    0x034F => {
                        props |= UnicodeProps::HIDDEN.bits();
                        *scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_CGJ;
                    }

                    _ => {}
                }
            }

            if gc.is_mark() {
                *scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_CONTINUATIONS;
                props |= UnicodeProps::CONTINUATION.bits();
                props |= (u.modified_combining_class() as u16) << 8;
            }
        }

        self.set_unicode_props(props);
    }

    #[inline]
    pub(crate) fn unhide(&mut self) {
        let mut n = self.unicode_props();
        n &= !UnicodeProps::HIDDEN.bits();
        self.set_unicode_props(n);
    }
}

pub type ClusterLevelCode = u32;
pub const HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES: u32 = 0;
pub const HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS: u32 = 1;
pub const HB_BUFFER_CLUSTER_LEVEL_CHARACTERS: u32 = 2;
pub const HB_BUFFER_CLUSTER_LEVEL_GRAPHEMES: u32 = 3;
pub const HB_BUFFER_CLUSTER_LEVEL_DEFAULT: u32 = HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES;

/// The type of contents currently held by a [`Buffer`].
///
/// This matches HarfBuzz's `hb_buffer_content_type_t`.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum ContentType {
    /// The buffer holds input characters, ready for shaping.
    Unicode,
    /// The buffer holds the glyphs produced by shaping.
    Glyphs,
}

/// A callback used to trace shaping operations.
///
/// See [`Buffer::set_message_function`] for the callback's arguments and return value.
#[cfg(feature = "tracing")]
pub type MessageFunction =
    dyn FnMut(&Buffer, &crate::ShaperFont<'_, '_>, &str) -> bool + Send + Sync;

/// A buffer of text to be shaped, and of the glyphs that shaping produces.
///
/// This is the unified buffer type, matching HarfBuzz's `hb_buffer_t`. It
/// carries a [`ContentType`] describing which of the two states it is
/// currently in.
pub struct Buffer {
    // Information about how the text in the buffer should be treated.
    pub(crate) flags: BufferFlags,
    pub(crate) cluster_level: ClusterLevelCode,
    pub(crate) invisible: Option<GlyphId>,
    pub(crate) not_found_variation_selector: Option<u32>,

    // Buffer contents.
    pub(crate) content_type: Option<ContentType>,
    pub(crate) direction: Direction,
    pub(crate) script: Option<Script>,
    pub(crate) language: Option<Language>,

    /// Allocations successful.
    pub(crate) successful: bool,
    /// Whether we have an output buffer going on.
    pub(crate) have_output: bool,
    pub(crate) have_separate_output: bool,
    /// Whether we have positions
    pub(crate) have_positions: bool,

    pub(crate) idx: usize,
    pub(crate) len: usize,
    pub(crate) out_len: usize,

    pub(crate) info: Vec<GlyphInfo>,
    pub(crate) pos: Vec<GlyphPosition>,

    // Text before / after the main buffer contents.
    // Always in Unicode, and ordered outward.
    // Index 0 is for "pre-context", 1 for "post-context".
    pub(crate) context: [[Codepoint; CONTEXT_LENGTH]; 2],
    pub(crate) context_len: [usize; 2],

    pub(crate) digest: SetDigest,
    pub(crate) glyph_set: U32Set,

    // Managed by enter / leave
    pub(crate) allocated_var_bits: u8,
    pub(crate) serial: u8,
    pub(crate) scratch_flags: ScratchFlags,
    /// Maximum allowed len.
    pub(crate) max_len: usize,
    /// Maximum allowed operations.
    pub(crate) max_ops: i32,
    #[cfg(feature = "tracing")]
    message_function: Option<Box<MessageFunction>>,
}

impl Buffer {
    /// Creates a new `Buffer`.
    pub fn new() -> Self {
        Buffer {
            flags: BufferFlags::empty(),
            cluster_level: HB_BUFFER_CLUSTER_LEVEL_DEFAULT,
            invisible: None,
            scratch_flags: HB_BUFFER_SCRATCH_FLAG_DEFAULT,
            not_found_variation_selector: None,
            max_len: Self::MAX_LEN_DEFAULT,
            max_ops: Self::MAX_OPS_DEFAULT,
            content_type: None,
            direction: Direction::Invalid,
            script: None,
            language: None,
            successful: true,
            have_output: false,
            have_positions: false,
            idx: 0,
            len: 0,
            out_len: 0,
            info: Vec::new(),
            pos: Vec::new(),
            have_separate_output: false,
            allocated_var_bits: 0,
            serial: 0,
            context: Default::default(),
            context_len: [0, 0],
            digest: SetDigest::new(),
            glyph_set: U32Set::default(),
            #[cfg(feature = "tracing")]
            message_function: None,
        }
    }

    /// Clears the contents of the buffer, retaining its allocation.
    ///
    /// This matches HarfBuzz's `hb_buffer_clear_contents`.
    pub fn clear(&mut self) {
        self.content_type = None;
        self.direction = Direction::Invalid;
        self.script = None;
        self.language = None;

        self.successful = true;
        self.have_output = false;
        self.have_positions = false;

        self.idx = 0;
        self.info.clear();
        self.pos.clear();
        self.len = 0;
        self.out_len = 0;
        self.have_separate_output = false;

        self.context = Default::default();
        self.context_len = [0, 0];

        self.serial = 0;
        self.scratch_flags = HB_BUFFER_SCRATCH_FLAG_DEFAULT;
        // How the buffer is configured -- its flags, cluster level, invisible
        // glyph and variation-selector fallback -- outlives its contents, as
        // it does in `hb_buffer_clear_contents`. `Buffer::reset` is what puts
        // those back.
    }

    /// Resets the buffer to its default state, clearing its contents along
    /// with the flags and cluster level.
    pub fn reset(&mut self) {
        self.clear();
        self.flags = BufferFlags::empty();
        self.cluster_level = HB_BUFFER_CLUSTER_LEVEL_DEFAULT;
        self.invisible = None;
        self.not_found_variation_selector = None;
    }

    /// Returns the type of content currently held by the buffer, or `None`
    /// if the buffer is empty or has been cleared.
    #[inline]
    pub fn content_type(&self) -> Option<ContentType> {
        self.content_type
    }

    /// Sets the type of content held by the buffer.
    ///
    /// This only relabels the buffer; it never clears it. Shaping sets this to
    /// [`ContentType::Glyphs`] on its own, so most callers never need it.
    ///
    /// Setting it explicitly reinterprets the items already in the buffer, which
    /// is occasionally what you want: label a buffer of glyph ids as
    /// [`ContentType::Glyphs`] to serialize a run that was shaped
    /// elsewhere, or label a shaped buffer as [`ContentType::Unicode`] to
    /// force [`shape`](crate::shape) to run over it again.
    #[inline]
    pub fn set_content_type(&mut self, content_type: Option<ContentType>) {
        self.content_type = content_type;
    }

    /// Returns the number of items in the buffer.
    ///
    /// Before shaping this is the number of Unicode codepoints; after
    /// shaping it is the number of glyphs.
    #[inline]
    pub fn len(&self) -> usize {
        self.len
    }

    /// Checks that buffer contains no elements.
    /// Returns `true` if the buffer contains no items.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Sets the number of items in the buffer.
    ///
    /// Growing the buffer fills the new items with zeros. Returns `false` if
    /// the allocation failed, in which case the buffer is left unchanged.
    pub fn set_length(&mut self, len: usize) -> bool {
        if !self.ensure(len) {
            return false;
        }
        if len > self.len {
            if let Some(infos) = self.info.get_mut(self.len..len) {
                infos.fill(GlyphInfo::default());
            }
            if self.have_positions {
                if let Some(positions) = self.pos.get_mut(self.len..len) {
                    positions.fill(GlyphPosition::default());
                }
            }
        }
        self.len = len;
        true
    }

    /// Ensures that the buffer can hold at least `size` items.
    #[inline]
    pub fn reserve(&mut self, size: usize) -> bool {
        self.ensure(size)
    }

    /// Returns `true` if every allocation made on this buffer has succeeded.
    ///
    /// This goes false when filling the buffer runs out of memory, and when
    /// shaping needs more room, more operations or more nesting than the
    /// shaper allows.
    #[inline]
    pub fn allocation_successful(&self) -> bool {
        self.successful
    }

    /// Appends a codepoint to the buffer with the given cluster value.
    ///
    /// This matches HarfBuzz's `hb_buffer_add`.
    pub fn push(&mut self, codepoint: u32, cluster: u32) {
        self.content_type = Some(ContentType::Unicode);
        if !self.ensure(self.len + 1) {
            return;
        }
        let Some(slot) = self.info.get_mut(self.len) else {
            return;
        };
        *slot = GlyphInfo {
            glyph_id: codepoint,
            cluster,
            ..GlyphInfo::default()
        };
        self.len += 1;
    }

    /// Appends a string to the buffer.
    ///
    /// Cluster values are set to the UTF-8 byte offset of each character.
    pub fn push_str(&mut self, text: &str) {
        if !self.ensure(self.len + text.chars().count()) {
            return;
        }
        self.content_type = Some(ContentType::Unicode);

        let Some(slots) = self.info.get_mut(self.len..) else {
            return;
        };
        for (slot, (i, c)) in slots.iter_mut().zip(text.char_indices()) {
            *slot = GlyphInfo {
                glyph_id: c as u32,
                cluster: i as u32,
                ..GlyphInfo::default()
            };
            self.len += 1;
        }
    }

    /// Appends codepoints with cluster values starting at zero.
    ///
    /// Cluster indices restart at zero on each call.
    pub fn push_codepoints(&mut self, codepoints: &[u32]) {
        if !self.ensure(self.len + codepoints.len()) {
            return;
        }
        self.content_type = Some(ContentType::Unicode);
        let Some(slots) = self.info.get_mut(self.len..) else {
            return;
        };
        for (slot, (i, &c)) in slots.iter_mut().zip(codepoints.iter().enumerate()) {
            *slot = GlyphInfo {
                glyph_id: c,
                cluster: i as u32,
                ..GlyphInfo::default()
            };
            self.len += 1;
        }
    }

    /// Appends glyph infos to the buffer.
    pub fn push_glyph_infos(&mut self, infos: &[GlyphInfo]) -> bool {
        let len = infos.len();
        if !self.ensure(self.len + len) {
            return false;
        }
        let Some(slots) = self.info.get_mut(self.len..self.len + len) else {
            return false;
        };
        slots.copy_from_slice(infos);
        self.len += len;
        true
    }

    /// Sets the pre-context for this buffer.
    pub fn set_pre_context(&mut self, text: &str) {
        self.clear_context(0);
        for (i, c) in text.chars().rev().enumerate().take(CONTEXT_LENGTH) {
            self.context[0][i] = c as Codepoint;
            self.context_len[0] += 1;
        }
    }

    /// Sets the pre-context for this buffer from codepoints.
    ///
    /// The input is expected to be the Unicode codepoints in reverse order.
    /// This matches HarfBuzz's internal storage of pre-context, and serves
    /// as a low-overhead method to pass pre-context from HarfBuzz-HarfRust.
    pub fn set_pre_context_codepoints(&mut self, codepoints: &[u32]) {
        self.clear_context(0);
        for (i, &c) in codepoints.iter().take(CONTEXT_LENGTH).enumerate() {
            self.context[0][i] = c;
            self.context_len[0] += 1;
        }
    }

    /// Returns the pre-context, in the reverse order
    /// [`Buffer::set_pre_context_codepoints`] takes it.
    #[inline]
    pub fn pre_context_codepoints(&self) -> &[u32] {
        &self.context[0][..self.context_len[0]]
    }

    /// Sets the post-context for this buffer.
    pub fn set_post_context(&mut self, text: &str) {
        self.clear_context(1);
        for (i, c) in text.chars().enumerate().take(CONTEXT_LENGTH) {
            self.context[1][i] = c as Codepoint;
            self.context_len[1] += 1;
        }
    }

    /// Sets the post-context for this buffer from codepoints.
    pub fn set_post_context_codepoints(&mut self, codepoints: &[u32]) {
        self.clear_context(1);
        for (i, &c) in codepoints.iter().take(CONTEXT_LENGTH).enumerate() {
            self.context[1][i] = c;
            self.context_len[1] += 1;
        }
    }

    /// Returns the post-context.
    #[inline]
    pub fn post_context_codepoints(&self) -> &[u32] {
        &self.context[1][..self.context_len[1]]
    }

    /// Returns the buffer's text direction.
    #[inline]
    pub fn direction(&self) -> Direction {
        self.direction
    }

    /// Sets the buffer's text direction.
    #[inline]
    pub fn set_direction(&mut self, direction: Direction) {
        self.direction = direction;
    }

    /// Returns the buffer's ISO 15924 script, or `None` if it has none.
    ///
    /// A buffer with no script is not one whose script is `Zzzz`: that is a
    /// script a caller can ask for, meaning known to be unknown.
    #[inline]
    pub fn script(&self) -> Option<Script> {
        self.script
    }

    /// Sets the buffer's script from an ISO 15924 tag, or clears it.
    #[inline]
    pub fn set_script(&mut self, script: Option<Script>) {
        self.script = script;
    }

    /// Returns the buffer's language, or `None` if it has none.
    #[inline]
    pub fn language(&self) -> Option<&Language> {
        self.language.as_ref()
    }

    /// Sets the buffer's language, or clears it.
    #[inline]
    pub fn set_language(&mut self, language: Option<Language>) {
        self.language = language;
    }

    /// Guesses the direction, script and language of the buffer contents.
    ///
    /// Only properties that are still unset are filled in.
    pub fn guess_segment_properties(&mut self) {
        if self.script.is_none() {
            for info in &self.info {
                match info.as_codepoint().script() {
                    Script::COMMON | Script::INHERITED | Script::UNKNOWN => {}
                    s => {
                        self.script = Some(s);
                        break;
                    }
                }
            }
        }

        if self.direction == Direction::Invalid {
            if let Some(script) = self.script {
                self.direction = Direction::from_script(script).unwrap_or_default();
            }

            if self.direction == Direction::Invalid {
                self.direction = Direction::LeftToRight;
            }
        }

        // TODO: language must be set
    }

    /// Returns the buffer's flags.
    #[inline]
    pub fn flags(&self) -> BufferFlags {
        self.flags
    }

    /// Sets the buffer's flags.
    #[inline]
    pub fn set_flags(&mut self, flags: BufferFlags) {
        self.flags = flags;
    }

    /// Returns the buffer's cluster level.
    #[inline]
    pub fn cluster_level(&self) -> ClusterLevel {
        ClusterLevel::new(self.cluster_level)
    }

    /// Sets the buffer's cluster level.
    #[inline]
    pub fn set_cluster_level(&mut self, cluster_level: ClusterLevel) {
        self.cluster_level = match cluster_level {
            ClusterLevel::MonotoneGraphemes => HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES,
            ClusterLevel::MonotoneCharacters => HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS,
            ClusterLevel::Characters => HB_BUFFER_CLUSTER_LEVEL_CHARACTERS,
            ClusterLevel::Graphemes => HB_BUFFER_CLUSTER_LEVEL_GRAPHEMES,
        };
    }

    /// Returns the glyph used to replace invisible characters, if set.
    #[inline]
    pub fn invisible_glyph(&self) -> Option<GlyphId> {
        self.invisible
    }

    /// Sets the glyph used to replace invisible characters.
    #[inline]
    pub fn set_invisible_glyph(&mut self, glyph: Option<GlyphId>) {
        self.invisible = glyph;
    }

    /// Returns the glyph used to replace not-found variation selectors, if set.
    #[inline]
    pub fn not_found_variation_selector_glyph(&self) -> Option<u32> {
        self.not_found_variation_selector
    }

    /// Sets the glyph used to replace not-found variation-selector characters.
    ///
    /// A variation selector is not present if there is no glyph in the font for
    /// the combination with the previous character. If None then the
    /// selector is treated as a default ignorable character. If Some(glyph)
    /// then the selector is preserved but replaced with glyph with zero
    /// advance to allow callers to know about missing glyph combinations.
    /// E.g., fall back to another font.
    ///
    /// glyph is taken as is and does not have to be present in the font;
    /// any u32 value, even above u16::MAX, is accepted.
    #[inline]
    pub fn set_not_found_variation_selector_glyph(&mut self, glyph: Option<u32>) {
        self.not_found_variation_selector = glyph;
    }

    /// Returns the buffer contents.
    ///
    /// Before shaping, [`GlyphInfo::glyph_id`] holds the input Unicode
    /// codepoint rather than a glyph id.
    #[inline]
    pub fn glyph_infos(&self) -> &[GlyphInfo] {
        debug_assert!(self.len <= self.info.len());
        self.info.get(..self.len).unwrap_or_default()
    }

    /// Returns the buffer contents mutably.
    #[inline]
    pub fn glyph_infos_mut(&mut self) -> &mut [GlyphInfo] {
        debug_assert!(self.len <= self.info.len());
        self.info.get_mut(..self.len).unwrap_or_default()
    }

    /// Returns the glyph positions.
    ///
    /// Returns an empty slice if the buffer has not been shaped and has no
    /// positions allocated. Use
    /// [`glyph_positions_mut`](Self::glyph_positions_mut) to allocate them on
    /// demand.
    #[inline]
    pub fn glyph_positions(&self) -> &[GlyphPosition] {
        if !self.have_positions {
            return &[];
        }
        self.pos.get(..self.len).unwrap_or_default()
    }

    /// Returns the glyph positions mutably, allocating and zeroing them if
    /// the buffer does not have positions yet.
    #[inline]
    pub fn glyph_positions_mut(&mut self) -> &mut [GlyphPosition] {
        if !self.have_positions {
            self.clear_positions();
        }
        self.pos.get_mut(..self.len).unwrap_or_default()
    }

    /// Reverses the buffer contents.
    #[inline]
    pub fn reverse(&mut self) {
        if self.is_empty() {
            return;
        }

        self.reverse_range(0, self.len);
    }

    /// Reverses the buffer contents between `start` and `end`.
    pub fn reverse_range(&mut self, start: usize, end: usize) {
        if end - start < 2 {
            return;
        }

        if let Some(infos) = self.info.get_mut(start..end) {
            infos.reverse();
        }
        if self.have_positions {
            if let Some(positions) = self.pos.get_mut(start..end) {
                positions.reverse();
            }
        }
    }

    /// Reverses the buffer contents, keeping the items within each cluster in
    /// their original order.
    #[inline]
    pub fn reverse_clusters(&mut self) {
        self.reverse_groups(|a, b| a.cluster == b.cluster, false);
    }

    /// Resets the cluster value of each item to its index.
    #[inline]
    pub fn reset_clusters(&mut self) {
        for (i, info) in self.info.iter_mut().enumerate() {
            info.cluster = i as u32;
        }
    }

    /// Serializes the buffer contents into a string.
    pub fn serialize(
        &self,
        font: Option<&crate::ShaperFont<'_, '_>>,
        flags: SerializeFlags,
    ) -> String {
        self.serialize_impl(font, flags).unwrap_or_default()
    }
}

impl Buffer {
    pub(crate) const MAX_LEN_FACTOR: usize = 256;
    pub(crate) const MAX_LEN_MIN: usize = 65536;
    // Shaping more than a billion chars? Let us know!
    pub(crate) const MAX_LEN_DEFAULT: usize = 0x3FFF_FFFF;

    pub(crate) const MAX_OPS_FACTOR: i32 = 4096;
    pub(crate) const MAX_OPS_MIN: i32 = 65536;
    // Shaping more than a billion operations? Let us know!
    pub(crate) const MAX_OPS_DEFAULT: i32 = 0x1FFF_FFFF;

    #[inline]
    pub(crate) fn allocate_var(&mut self, shape: buffer_var_shape) {
        let bits = shape.bits();
        debug_assert_eq!(
            self.allocated_var_bits & bits,
            0,
            "Variable already allocated"
        );
        self.allocated_var_bits |= bits;
    }

    #[inline]
    pub(crate) fn try_allocate_var(&mut self, shape: buffer_var_shape) -> bool {
        let bits = shape.bits();
        if self.allocated_var_bits & bits != 0 {
            return false;
        }
        self.allocated_var_bits |= bits;
        true
    }

    #[inline]
    pub(crate) fn deallocate_var(&mut self, shape: buffer_var_shape) {
        let bits = shape.bits();
        debug_assert_eq!(
            self.allocated_var_bits & bits,
            bits,
            "Deallocating unallocated var"
        );
        self.allocated_var_bits &= !bits;
    }

    #[inline]
    pub(crate) fn assert_var(&self, shape: buffer_var_shape) {
        let bits = shape.bits();
        debug_assert_eq!(
            self.allocated_var_bits & bits,
            bits,
            "Variable not allocated"
        );
    }

    #[inline]
    pub(crate) fn info_slice_mut(&mut self) -> &mut [GlyphInfo] {
        debug_assert!(self.len <= self.info.len());
        self.info.get_mut(..self.len).unwrap_or_default()
    }

    /// Returns the glyph infos and positions together, for walking both at
    /// once.
    #[inline]
    pub(crate) fn infos_and_positions_mut(&mut self) -> (&mut [GlyphInfo], &mut [GlyphPosition]) {
        let len = self.len;
        debug_assert!(len <= self.info.len() && len <= self.pos.len());
        (
            self.info.get_mut(..len).unwrap_or_default(),
            self.pos.get_mut(..len).unwrap_or_default(),
        )
    }

    #[inline]
    pub(crate) fn out_info(&self) -> &[GlyphInfo] {
        if self.have_separate_output {
            bytemuck::cast_slice(self.pos.as_slice())
        } else {
            &self.info
        }
    }

    #[inline]
    pub(crate) fn out_info_mut(&mut self) -> &mut [GlyphInfo] {
        if self.have_separate_output {
            bytemuck::cast_slice_mut(self.pos.as_mut_slice())
        } else {
            &mut self.info
        }
    }

    /// Marks the buffer unsuccessful after an index fell outside it.
    ///
    /// Buffer operations skip the out-of-range access but keep their
    /// bookkeeping, so shaping loops still advance and then stop on the
    /// failure as they do when an allocation fails.
    #[cold]
    #[inline(never)]
    pub(crate) fn index_failed(&mut self) {
        debug_assert!(false, "buffer index out of bounds");
        self.successful = false;
    }

    #[inline(always)]
    fn set_out_info(&mut self, i: usize, info: GlyphInfo) {
        match self.out_info_mut().get_mut(i) {
            Some(slot) => *slot = info,
            None => self.index_failed(),
        }
    }

    /// Below this many elements, copy inline instead of calling memmove:
    /// AAT state machines move a couple of glyphs at a time, and the call
    /// plus memmove's size dispatch costs more cycles than the copy.
    const SMALL_COPY: usize = 16;

    /// Block-copies `info[src..src + count]` to `out_info[dst..dst + count]`.
    /// The caller must have made room in the out buffer already. In the
    /// shared-array case the destination never starts past the source
    /// (`dst <= src`), which makes the ascending inline copy overlap-safe.
    // Kept out of line so the callers' common no-copy path stays small enough
    // to inline.
    #[inline(never)]
    fn copy_infos_to_out(&mut self, src: usize, dst: usize, count: usize) {
        if self.try_copy_infos_to_out(src, dst, count).is_none() {
            self.index_failed();
        }
    }

    #[inline]
    fn try_copy_infos_to_out(&mut self, src: usize, dst: usize, count: usize) -> Option<()> {
        if self.have_separate_output {
            let out: &mut [GlyphInfo] = bytemuck::cast_slice_mut(self.pos.as_mut_slice());
            let to = out.get_mut(dst..dst + count)?;
            let from = self.info.get(src..src + count)?;
            if count < Self::SMALL_COPY {
                // Deliberately not copy_from_slice: avoiding the memmove
                // call for small counts is this fast path's entire point.
                for (to, from) in to.iter_mut().zip(from) {
                    *to = *from;
                }
            } else {
                to.copy_from_slice(from);
            }
        } else {
            debug_assert!(dst <= src);
            // Destination and source share the array; `dst <= src` makes the
            // ascending copy overlap-safe.
            let window = self.info.get_mut(dst..src.checked_add(count)?)?;
            let offset = src - dst;
            if count < Self::SMALL_COPY {
                for i in 0..count {
                    *window.get_mut(i)? = *window.get(offset + i)?;
                }
            } else {
                window.copy_within(offset.., 0);
            }
        }
        Some(())
    }

    /// Block-copies `out_info[src..src + count]` to `info[dst..dst + count]`.
    /// In the shared-array case the destination never starts before the
    /// source (`dst >= src`), so the inline copy must run descending to keep
    /// memmove semantics on overlap.
    #[inline]
    fn copy_out_to_infos(&mut self, src: usize, dst: usize, count: usize) {
        if self.try_copy_out_to_infos(src, dst, count).is_none() {
            self.index_failed();
        }
    }

    #[inline]
    fn try_copy_out_to_infos(&mut self, src: usize, dst: usize, count: usize) -> Option<()> {
        if self.have_separate_output {
            let out: &[GlyphInfo] = bytemuck::cast_slice(self.pos.as_slice());
            let from = out.get(src..src + count)?;
            let to = self.info.get_mut(dst..dst + count)?;
            if count < Self::SMALL_COPY {
                // Deliberately not copy_from_slice; see copy_infos_to_out.
                for (to, from) in to.iter_mut().zip(from) {
                    *to = *from;
                }
            } else {
                to.copy_from_slice(from);
            }
        } else {
            debug_assert!(dst >= src);
            // Destination and source share the array; `dst >= src` needs the
            // descending copy to stay overlap-safe.
            let window = self.info.get_mut(src..dst.checked_add(count)?)?;
            let offset = dst - src;
            if count < Self::SMALL_COPY {
                for i in (0..count).rev() {
                    *window.get_mut(offset + i)? = *window.get(i)?;
                }
            } else {
                window.copy_within(..count, offset);
            }
        }
        Some(())
    }

    #[inline]
    pub(crate) fn cur(&self, i: usize) -> Option<&GlyphInfo> {
        self.info.get(self.idx + i).likely()
    }

    #[inline]
    pub(crate) fn cur_mut(&mut self, i: usize) -> Option<&mut GlyphInfo> {
        let idx = self.idx + i;
        self.info.get_mut(idx).likely()
    }

    #[inline]
    pub(crate) fn cur_pos_mut(&mut self) -> Option<&mut GlyphPosition> {
        let idx = self.idx;
        self.pos.get_mut(idx).likely()
    }

    #[inline]
    pub(crate) fn prev(&self) -> Option<&GlyphInfo> {
        self.out_info().get(self.out_len.checked_sub(1)?).likely()
    }

    #[inline]
    pub(crate) fn prev_mut(&mut self) -> Option<&mut GlyphInfo> {
        let idx = self.out_len.checked_sub(1)?;
        self.out_info_mut().get_mut(idx).likely()
    }

    pub(crate) fn update_digest(&mut self) {
        self.digest = SetDigest::new();
        let len = self.len;
        self.digest.add_array(
            self.info
                .get(..len)
                .unwrap_or_default()
                .iter()
                .map(|i| i.glyph_id),
        );
    }
    pub(crate) fn update_glyph_set(&mut self) {
        self.glyph_set.clear();
        self.glyph_set
            .extend_unsorted(self.info.iter().map(|i| i.glyph_id));
    }

    #[inline]
    pub(crate) fn backtrack_len(&self) -> usize {
        if self.have_output {
            self.out_len
        } else {
            self.idx
        }
    }

    #[inline]
    pub(crate) fn lookahead_len(&self) -> usize {
        self.len - self.idx
    }

    #[inline]
    fn next_serial(&mut self) -> u8 {
        // A `serial` overflow/wrap-around here is perfectly fine.
        self.serial = self.serial.wrapping_add(1);

        if self.serial == 0 {
            self.serial += 1;
        }

        self.serial
    }

    pub(crate) fn reverse_groups<F>(&mut self, group: F, merge_clusters: bool)
    where
        F: Fn(&GlyphInfo, &GlyphInfo) -> bool,
    {
        if self.is_empty() {
            return;
        }

        let mut start = 0;
        let mut i = 1;

        while i < self.len {
            let (Some(a), Some(b)) = (self.info.get(i - 1), self.info.get(i)) else {
                self.index_failed();
                break;
            };
            if !group(a, b) {
                if merge_clusters {
                    self.merge_clusters(start, i);
                }

                self.reverse_range(start, i);
                start = i;
            }

            i += 1;
        }

        if merge_clusters {
            self.merge_clusters(start, i);
        }

        self.reverse_range(start, i);

        self.reverse();
    }

    pub(crate) fn group_end<F>(&self, mut start: usize, group: F) -> usize
    where
        F: Fn(&GlyphInfo, &GlyphInfo) -> bool,
    {
        start += 1;

        while start < self.len {
            let (Some(a), Some(b)) = (self.info.get(start - 1), self.info.get(start)) else {
                break;
            };
            if !group(a, b) {
                break;
            }
            start += 1;
        }

        start
    }

    pub(crate) fn sync(&mut self) -> bool {
        debug_assert!(self.have_output);
        debug_assert!(self.idx <= self.len);

        if !self.successful {
            self.have_output = false;
            self.out_len = 0;
            self.idx = 0;
            return false;
        }

        self.next_glyphs(self.len - self.idx);

        if self.have_separate_output {
            // Swap info and pos buffers.
            let info: Vec<GlyphPosition> = bytemuck::cast_vec(core::mem::take(&mut self.info));
            let pos: Vec<GlyphInfo> = bytemuck::cast_vec(core::mem::take(&mut self.pos));
            self.pos = info;
            self.info = pos;
            self.have_separate_output = false;
        }

        self.len = self.out_len;

        self.have_output = false;
        self.out_len = 0;
        self.idx = 0;
        true
    }

    pub(crate) fn clear_output(&mut self) {
        self.have_output = true;
        self.have_positions = false;

        self.idx = 0;
        self.out_len = 0;
        self.have_separate_output = false;
    }

    pub(crate) fn clear_positions(&mut self) {
        self.have_output = false;
        self.have_positions = true;

        self.out_len = 0;
        self.have_separate_output = false;

        for pos in &mut self.pos {
            *pos = GlyphPosition::default();
        }
    }

    pub(crate) fn replace_glyphs(&mut self, num_in: usize, num_out: usize, glyph_data: &[u32]) {
        if !self.make_room_for(num_in, num_out) {
            return;
        }

        debug_assert!(self.idx + num_in <= self.len);

        self.merge_clusters(self.idx, self.idx + num_in);

        let orig_info = self.info.get(self.idx).copied();
        let out_len = self.out_len;
        let out = self.out_info_mut().get_mut(out_len..out_len + num_out);
        match (orig_info, out, glyph_data.get(..num_out)) {
            (Some(orig_info), Some(out), Some(glyph_data)) => {
                for (out_info, &glyph_id) in out.iter_mut().zip(glyph_data) {
                    *out_info = orig_info;
                    out_info.glyph_id = glyph_id;
                }
            }
            _ => self.index_failed(),
        }

        self.idx += num_in;
        self.out_len += num_out;
    }

    pub(crate) fn replace_glyph(&mut self, glyph_index: u32) {
        if self.have_separate_output || self.out_len != self.idx {
            if !self.make_room_for(1, 1) {
                return;
            }

            self.copy_cur_to_out(self.out_len);
        }

        self.set_out_glyph(self.out_len, glyph_index);

        self.idx += 1;
        self.out_len += 1;
    }

    pub(crate) fn output_glyph(&mut self, glyph_index: u32) {
        if !self.make_room_for(0, 1) {
            return;
        }

        if self.idx == self.len && self.out_len == 0 {
            return;
        }

        let out_len = self.out_len;
        if self.idx < self.len {
            self.copy_cur_to_out(out_len);
        } else {
            match self.out_info().get(out_len - 1).copied() {
                Some(info) => self.set_out_info(out_len, info),
                None => self.index_failed(),
            }
        }

        self.set_out_glyph(out_len, glyph_index);

        self.out_len += 1;
    }

    pub(crate) fn output_info(&mut self, glyph_info: GlyphInfo) {
        if !self.make_room_for(0, 1) {
            return;
        }

        self.set_out_info(self.out_len, glyph_info);
        self.out_len += 1;
    }

    /// Copies glyph at idx to output but doesn't advance idx.
    pub(crate) fn copy_glyph(&mut self) {
        if !self.make_room_for(0, 1) {
            return;
        }

        self.copy_cur_to_out(self.out_len);
        self.out_len += 1;
    }

    /// Copies the current glyph to `out_info[out]`.
    #[inline(always)]
    fn copy_cur_to_out(&mut self, out: usize) {
        match self.info.get(self.idx).copied() {
            Some(info) => self.set_out_info(out, info),
            None => self.index_failed(),
        }
    }

    #[inline(always)]
    fn set_out_glyph(&mut self, out: usize, glyph_index: u32) {
        match self.out_info_mut().get_mut(out) {
            Some(info) => info.glyph_id = glyph_index,
            None => self.index_failed(),
        }
    }

    /// Copies glyph at idx to output and advance idx.
    ///
    /// If there's no output, just advance idx.
    #[inline(always)]
    pub(crate) fn next_glyph(&mut self) {
        if self.have_output {
            if self.have_separate_output || self.out_len != self.idx {
                if !self.ensure(self.out_len + 1) {
                    return;
                }

                self.copy_cur_to_out(self.out_len);
            }

            self.out_len += 1;
        }

        self.idx += 1;
    }

    /// Copies n glyphs at idx to output and advance idx.
    ///
    /// If there's no output, just advance idx.
    #[inline]
    pub(crate) fn next_glyphs(&mut self, n: usize) {
        if self.have_output {
            if self.have_separate_output || self.out_len != self.idx {
                if !self.ensure(self.out_len + n) {
                    return;
                }

                self.copy_infos_to_out(self.idx, self.out_len, n);
            }

            self.out_len += n;
        }

        self.idx += n;
    }

    /// Advance idx without copying to output.
    pub(crate) fn skip_glyph(&mut self) {
        self.idx += 1;
    }

    pub(crate) fn set_masks(
        &mut self,
        mut value: Mask,
        mask: Mask,
        cluster_start: u32,
        cluster_end: u32,
    ) {
        if mask == 0 {
            return;
        }

        let not_mask = !mask;
        value &= mask;

        self.max_ops -= self.len as i32;
        if self.max_ops < 0 {
            self.successful = false;
        }

        if cluster_start == 0 && cluster_end == u32::MAX {
            for info in self.info_slice_mut() {
                info.mask = (info.mask & not_mask) | value;
            }

            return;
        }

        for info in self.info_slice_mut() {
            if cluster_start <= info.cluster && info.cluster < cluster_end {
                info.mask = (info.mask & not_mask) | value;
            }
        }
    }

    #[inline(always)]
    pub(crate) fn merge_clusters(&mut self, start: usize, end: usize) {
        if end - start < 2 {
            return;
        }

        if !ClusterLevel::new(self.cluster_level).is_monotone() {
            self.unsafe_to_break(Some(start), Some(end));
            return;
        }

        self.merge_clusters_impl(start, end);
    }

    fn merge_clusters_impl(&mut self, start: usize, end: usize) {
        self.max_ops -= (end - start) as i32;
        if self.max_ops < 0 {
            self.successful = false;
        }

        if self.try_merge_clusters(start, end).is_none() {
            self.index_failed();
        }
    }

    fn try_merge_clusters(&mut self, mut start: usize, mut end: usize) -> Option<()> {
        let cluster_at = |infos: &[GlyphInfo], i: usize| infos.get(i).map(|info| info.cluster);
        let cluster = self
            .info
            .get(start..end)?
            .iter()
            .map(|info| info.cluster)
            .min()?;

        // Extend end
        if cluster != cluster_at(&self.info, end - 1)? {
            while end < self.len && cluster_at(&self.info, end - 1)? == cluster_at(&self.info, end)?
            {
                end += 1;
            }
        }

        // Extend start
        if cluster != cluster_at(&self.info, start)? {
            while self.idx < start
                && cluster_at(&self.info, start - 1)? == cluster_at(&self.info, start)?
            {
                start -= 1;
            }
        }

        // If we hit the start of buffer, continue in out-buffer.
        let start_cluster = cluster_at(&self.info, start)?;
        if self.idx == start && start_cluster != cluster {
            let mut i = self.out_len;
            while i != 0 {
                let info = self.out_info_mut().get_mut(i - 1)?;
                if info.cluster != start_cluster {
                    break;
                }
                Self::set_cluster(info, cluster, 0);
                i -= 1;
            }
        }

        for info in self.info.get_mut(start..end)? {
            Self::set_cluster(info, cluster, 0);
        }
        Some(())
    }

    pub(crate) fn merge_grapheme_clusters(&mut self, start: usize, end: usize) {
        if end - start < 2 {
            return;
        }

        if !ClusterLevel::new(self.cluster_level).is_graphemes() {
            self.unsafe_to_break(Some(start), Some(end));
            return;
        }

        self.merge_clusters_impl(start, end);
    }

    pub(crate) fn merge_out_clusters(&mut self, start: usize, end: usize) {
        if end - start < 2 {
            return;
        }

        if !ClusterLevel::new(self.cluster_level).is_monotone() {
            return;
        }

        self.merge_out_clusters_impl(start, end);
    }

    pub(crate) fn merge_out_grapheme_clusters(&mut self, start: usize, end: usize) {
        if end - start < 2 {
            return;
        }

        if !ClusterLevel::new(self.cluster_level).is_graphemes() {
            return;
        }

        self.merge_out_clusters_impl(start, end);
    }

    fn merge_out_clusters_impl(&mut self, start: usize, end: usize) {
        self.max_ops -= (end - start) as i32;
        if self.max_ops < 0 {
            self.successful = false;
        }

        if self.try_merge_out_clusters(start, end).is_none() {
            self.index_failed();
        }
    }

    fn try_merge_out_clusters(&mut self, mut start: usize, mut end: usize) -> Option<()> {
        let cluster_at = |infos: &[GlyphInfo], i: usize| infos.get(i).map(|info| info.cluster);
        let cluster = self
            .out_info()
            .get(start..end)?
            .iter()
            .map(|info| info.cluster)
            .min()?;

        // Extend start
        while start != 0
            && cluster_at(self.out_info(), start - 1)? == cluster_at(self.out_info(), start)?
        {
            start -= 1;
        }

        // Extend end
        while end < self.out_len
            && cluster_at(self.out_info(), end - 1)? == cluster_at(self.out_info(), end)?
        {
            end += 1;
        }

        // If we hit the start of buffer, continue in out-buffer.
        if end == self.out_len {
            let end_cluster = cluster_at(self.out_info(), end - 1)?;
            let mut i = self.idx;
            while i < self.len {
                let info = self.info.get_mut(i)?;
                if info.cluster != end_cluster {
                    break;
                }
                Self::set_cluster(info, cluster, 0);
                i += 1;
            }
        }

        for info in self.out_info_mut().get_mut(start..end)? {
            Self::set_cluster(info, cluster, 0);
        }
        Some(())
    }

    /// Merge clusters for deleting current glyph, and skip it.
    pub(crate) fn delete_glyph(&mut self) {
        if self.merge_deleted_cluster().is_none() {
            self.index_failed();
        }
        self.skip_glyph();
    }

    fn merge_deleted_cluster(&mut self) -> Option<()> {
        let GlyphInfo { cluster, mask, .. } = *self.info.get(self.idx)?;
        let next_cluster = self.info.get(self.idx + 1).map(|info| info.cluster);
        let prev_cluster = match self.out_len {
            0 => None,
            out_len => Some(self.out_info().get(out_len - 1)?.cluster),
        };

        if (self.idx + 1 < self.len && next_cluster == Some(cluster))
            || prev_cluster == Some(cluster)
        {
            // Cluster survives; do nothing.
            return Some(());
        }

        if let Some(old_cluster) = prev_cluster {
            // Merge cluster backward.
            if cluster < old_cluster {
                let mut i = self.out_len;
                while i != 0 {
                    let info = self.out_info_mut().get_mut(i - 1)?;
                    if info.cluster != old_cluster {
                        break;
                    }
                    Self::set_cluster(info, cluster, mask);
                    i -= 1;
                }
            }
            return Some(());
        }

        if self.idx + 1 < self.len {
            // Merge cluster forward.
            self.merge_clusters(self.idx, self.idx + 2);
        }
        Some(())
    }

    pub(crate) fn delete_glyphs_inplace(&mut self, filter: impl Fn(&GlyphInfo) -> bool) {
        // Merge clusters and delete filtered glyphs.
        // NOTE! We can't use out-buffer as we have positioning data.
        let mut j = 0;

        for i in 0..self.len {
            let Some(&info) = self.info.get(i) else {
                self.index_failed();
                break;
            };
            if filter(&info) {
                // Merge clusters.
                // Same logic as delete_glyph(), but for in-place removal

                let GlyphInfo { cluster, mask, .. } = info;
                if self
                    .info
                    .get(i + 1)
                    .is_some_and(|next| i + 1 < self.len && next.cluster == cluster)
                {
                    // Cluster survives; do nothing.
                    continue;
                }

                if j != 0 {
                    // Merge cluster backward.
                    let old_cluster = self.info.get(j - 1).map_or(cluster, |info| info.cluster);
                    if cluster < old_cluster {
                        for info in self.info.get_mut(..j).unwrap_or_default().iter_mut().rev() {
                            if info.cluster != old_cluster {
                                break;
                            }
                            Self::set_cluster(info, cluster, mask);
                        }
                    }
                    continue;
                }

                if i + 1 < self.len {
                    // Merge cluster forward.
                    self.merge_clusters(i, i + 2);
                }

                continue;
            }

            if j != i {
                if let Some(slot) = self.info.get_mut(j) {
                    *slot = info;
                }
                if let Some(&pos) = self.pos.get(i) {
                    if let Some(slot) = self.pos.get_mut(j) {
                        *slot = pos;
                    }
                }
            }

            j += 1;
        }

        self.len = j;
    }

    pub(crate) fn unsafe_to_break(&mut self, start: Option<usize>, end: Option<usize>) {
        self.set_glyph_flags(
            GlyphFlags::UNSAFE_TO_BREAK | GlyphFlags::UNSAFE_TO_CONCAT,
            start,
            end,
            Some(true),
            None,
        );
    }

    pub(crate) fn safe_to_insert_tatweel(&mut self, start: Option<usize>, end: Option<usize>) {
        if !self
            .flags
            .contains(BufferFlags::PRODUCE_SAFE_TO_INSERT_TATWEEL)
        {
            self.unsafe_to_break(start, end);
            return;
        }

        self.set_glyph_flags(
            GlyphFlags::SAFE_TO_INSERT_TATWEEL,
            start,
            end,
            Some(true),
            None,
        );
    }

    fn _set_glyph_flags_impl(
        &mut self,
        mask: Mask,
        start: usize,
        end: usize,
        interior: bool,
        from_out_buffer: bool,
    ) {
        if !from_out_buffer || !self.have_output {
            if !interior {
                for info in self.info.get_mut(start..end).unwrap_or_default() {
                    info.mask |= mask;
                }
            } else {
                let cluster = self._infos_find_min_cluster(&self.info, start, end, None);
                self._infos_set_glyph_flags(false, start, end, cluster, mask);
            }
        } else {
            debug_assert!(start <= self.out_len);
            debug_assert!(self.idx <= end);

            if !interior {
                let range_end = self.out_len;
                for info in self
                    .out_info_mut()
                    .get_mut(start..range_end)
                    .unwrap_or_default()
                {
                    info.mask |= mask;
                }

                for info in self.info.get_mut(self.idx..end).unwrap_or_default() {
                    info.mask |= mask;
                }
            } else {
                let mut cluster = self._infos_find_min_cluster(&self.info, self.idx, end, None);
                cluster = self._infos_find_min_cluster(
                    self.out_info(),
                    start,
                    self.out_len,
                    Some(cluster),
                );

                let out_len = self.out_len;
                self._infos_set_glyph_flags(true, start, out_len, cluster, mask);
                self._infos_set_glyph_flags(false, self.idx, end, cluster, mask);
            }
        }
    }

    /// Adds glyph flags in mask to infos with clusters between start and end.
    /// The start index will be from out-buffer if from_out_buffer is true.
    /// If interior is true, then the cluster having the minimum value is skipped. */
    fn set_glyph_flags(
        &mut self,
        flags: GlyphFlags,
        start: Option<usize>,
        end: Option<usize>,
        interior: Option<bool>,
        from_out_buffer: Option<bool>,
    ) {
        // If the range is not specified, ie. whole buffer, allow it.
        // But if range *is* specified, reject if range is too large.
        if let (Some(start), Some(end)) = (start, end) {
            if end.wrapping_sub(start) > 255 {
                return;
            }
        }

        let start = start.unwrap_or(0);
        let end = min(end.unwrap_or(self.len), self.len);
        let interior = interior.unwrap_or(false);
        let from_out_buffer = from_out_buffer.unwrap_or(false);

        if interior && !from_out_buffer && end - start < 2 {
            return;
        }

        self._set_glyph_flags_impl(flags.0, start, end, interior, from_out_buffer);
    }

    pub(crate) fn unsafe_to_concat(&mut self, start: Option<usize>, end: Option<usize>) {
        if !self.flags.contains(BufferFlags::PRODUCE_UNSAFE_TO_CONCAT) {
            return;
        }

        self.set_glyph_flags(GlyphFlags::UNSAFE_TO_CONCAT, start, end, Some(false), None);
    }

    pub(crate) fn unsafe_to_break_from_outbuffer(
        &mut self,
        start: Option<usize>,
        end: Option<usize>,
    ) {
        self.set_glyph_flags(
            GlyphFlags::UNSAFE_TO_BREAK | GlyphFlags::UNSAFE_TO_CONCAT,
            start,
            end,
            Some(true),
            Some(true),
        );
    }

    pub(crate) fn unsafe_to_concat_from_outbuffer(
        &mut self,
        start: Option<usize>,
        end: Option<usize>,
    ) {
        if !self.flags.contains(BufferFlags::PRODUCE_UNSAFE_TO_CONCAT) {
            return;
        }

        self.set_glyph_flags(
            GlyphFlags::UNSAFE_TO_CONCAT,
            start,
            end,
            Some(false),
            Some(true),
        );
    }

    pub(crate) fn move_to(&mut self, i: usize) -> bool {
        if !self.have_output {
            debug_assert!(i <= self.len);
            self.idx = i;
            return true;
        }

        if !self.successful {
            return false;
        }

        debug_assert!(i <= self.out_len + (self.len - self.idx));

        if self.out_len < i {
            let count = i - self.out_len;
            if !self.make_room_for(count, count) {
                return false;
            }

            self.max_ops -= count as i32;
            if self.max_ops < 0 {
                self.successful = false;
                return false;
            }

            self.copy_infos_to_out(self.idx, self.out_len, count);

            self.idx += count;
            self.out_len += count;
        } else if self.out_len > i {
            // Tricky part: rewinding...
            let count = self.out_len - i;

            // This will blow in our face if memory allocation fails later
            // in this same lookup...
            //
            // We used to shift with extra 32 items.
            // But that would leave empty slots in the buffer in case of allocation
            // failures.  See comments in shift_forward().  This can cause O(N^2)
            // behavior more severely than adding 32 empty slots can...
            if self.idx < count && !self.shift_forward(count - self.idx) {
                return false;
            }

            debug_assert!(self.idx >= count);

            self.max_ops -= count as i32;
            if self.max_ops < 0 {
                self.successful = false;
                return false;
            }

            self.idx -= count;
            self.out_len -= count;

            self.copy_out_to_infos(self.out_len, self.idx, count);
        }

        true
    }

    #[must_use]
    #[inline(always)]
    pub(crate) fn ensure(&mut self, size: usize) -> bool {
        if size <= self.info.len() {
            true
        } else {
            self.enlarge(size)
        }
    }

    #[must_use]
    fn enlarge(&mut self, size: usize) -> bool {
        if size > self.max_len {
            self.successful = false;
            return false;
        }

        self.info.resize(size, GlyphInfo::default());
        self.pos.resize(size, GlyphPosition::default());
        true
    }

    #[must_use]
    fn make_room_for(&mut self, num_in: usize, num_out: usize) -> bool {
        if !self.ensure(self.out_len + num_out) {
            return false;
        }

        if !self.have_separate_output && self.out_len + num_out > self.idx + num_in {
            debug_assert!(self.have_output);

            self.have_separate_output = true;
            self.copy_infos_to_out(0, 0, self.out_len);
        }

        true
    }

    fn shift_forward(&mut self, count: usize) -> bool {
        debug_assert!(self.have_output);
        if !self.ensure(self.len + count) {
            return false;
        }

        self.max_ops -= (self.len - self.idx) as i32;
        if self.max_ops < 0 {
            self.successful = false;
            return false;
        }

        // Destination starts past the source; the inline copy runs
        // descending to keep memmove semantics on overlap.
        let moved = self.len - self.idx;
        let Some(window) = self.info.get_mut(self.idx..self.len + count) else {
            self.index_failed();
            return false;
        };
        if moved < Self::SMALL_COPY {
            for i in (0..moved).rev() {
                if let Some(&info) = window.get(i) {
                    if let Some(slot) = window.get_mut(count + i) {
                        *slot = info;
                    }
                }
            }
        } else {
            window.copy_within(..moved, count);
        }

        if let Some(gap) = window.get_mut(moved..count) {
            gap.fill(GlyphInfo::default());
        }

        self.len += count;
        self.idx += count;

        true
    }

    fn clear_context(&mut self, side: usize) {
        self.context_len[side] = 0;
    }

    pub(crate) fn sort(
        &mut self,
        start: usize,
        end: usize,
        cmp: impl Fn(&GlyphInfo, &GlyphInfo) -> bool,
    ) {
        debug_assert!(!self.have_positions);

        for i in start + 1..end {
            let Some(&item) = self.info.get(i) else {
                self.index_failed();
                return;
            };
            let mut j = i;
            while j > start && self.info.get(j - 1).is_some_and(|prev| cmp(prev, &item)) {
                j -= 1;
            }

            if i == j {
                continue;
            }

            // Move item i to occupy place for item j, shift what's in between.
            self.merge_clusters(j, i + 1);
            if let Some(moved) = self.info.get_mut(j..=i) {
                moved.rotate_right(1);
            }
        }
    }

    pub(crate) fn set_cluster(info: &mut GlyphInfo, cluster: u32, mask: Mask) {
        if info.cluster != cluster {
            info.mask = (info.mask & !GlyphFlags::DEFINED_BITS) | (mask & GlyphFlags::DEFINED_BITS);
        }

        info.cluster = cluster;
    }

    // Called around shape()
    pub(crate) fn enter(&mut self) {
        self.serial = 0;
        self.scratch_flags = HB_BUFFER_SCRATCH_FLAG_DEFAULT;

        if let Some(len) = self.len.checked_mul(Buffer::MAX_LEN_FACTOR) {
            self.max_len = len.max(Buffer::MAX_LEN_MIN);
        }

        if let Ok(len) = i32::try_from(self.len) {
            if let Some(ops) = len.checked_mul(Buffer::MAX_OPS_FACTOR) {
                self.max_ops = ops.max(Buffer::MAX_OPS_MIN);
            }
        }
    }

    // Called around shape()
    pub(crate) fn leave(&mut self) {
        self.max_len = Buffer::MAX_LEN_DEFAULT;
        self.max_ops = Buffer::MAX_OPS_DEFAULT;
        self.serial = 0;
    }

    fn _infos_find_min_cluster(
        &self,
        info: &[GlyphInfo],
        start: usize,
        end: usize,
        cluster: Option<u32>,
    ) -> u32 {
        let mut cluster = cluster.unwrap_or(u32::MAX);

        if start == end {
            return cluster;
        }

        let Some(infos) = info.get(start..end) else {
            return cluster;
        };
        if self.cluster_level == HB_BUFFER_CLUSTER_LEVEL_CHARACTERS {
            for glyph_info in infos {
                cluster = min(cluster, glyph_info.cluster);
            }
        }

        let first = infos.first().map_or(cluster, |info| info.cluster);
        let last = infos.last().map_or(cluster, |info| info.cluster);
        cluster.min(first.min(last))
    }

    #[inline(always)]
    fn _infos_set_glyph_flags(
        &mut self,
        out_info: bool,
        start: usize,
        end: usize,
        cluster: u32,
        mask: Mask,
    ) {
        if start == end {
            return;
        }

        let cluster_level = self.cluster_level;

        let infos = if out_info {
            self.out_info_mut()
        } else {
            self.info.as_mut_slice()
        };
        if set_cluster_flags(infos, cluster_level, start, end, cluster, mask).is_none() {
            self.index_failed();
        }
    }

    pub(crate) fn next_syllable(&self, start: usize) -> usize {
        if start >= self.len {
            return start;
        }

        let end = start + min(self.len - start, MAX_SYLLABLE_LENGTH);
        let Some(infos) = self.info.get(start..end) else {
            return end;
        };
        let syllable = infos.first().map(GlyphInfo::syllable);
        start
            + infos
                .iter()
                .take_while(|info| Some(info.syllable()) == syllable)
                .count()
    }

    #[inline]
    pub(crate) fn allocate_lig_id(&mut self) -> u8 {
        let mut lig_id = self.next_serial() & 0x07;

        if lig_id == 0 {
            lig_id = self.allocate_lig_id();
        }

        lig_id
    }

    /// Sets the callback used to trace shaping operations, or removes it with `None`.
    ///
    /// The callback receives the current buffer, shaping font, and a HarfBuzz
    /// tracing message. Return `false` from a skippable `start` message to skip
    /// that operation. Per-glyph and recursion messages are informational and
    /// ignore the return value. Message text may change as shaping algorithms
    /// evolve.
    ///
    /// The callback can inspect [`Self::content_type`], [`Self::glyph_infos`],
    /// [`Self::glyph_positions`], or [`Self::serialize`]. Positions are empty
    /// until positioning begins. The callback is retained by [`Self::clear`]
    /// and [`Self::reset`]. To collect results, capture owned shared state such
    /// as an `Arc<Mutex<Vec<String>>>`.
    ///
    /// Requires the `tracing` feature, which also works without `std`.
    #[cfg(feature = "tracing")]
    pub fn set_message_function(&mut self, function: Option<Box<MessageFunction>>) {
        self.message_function = function;
    }

    #[cfg(feature = "tracing")]
    pub(crate) fn message(&mut self, font: &crate::ShaperFont<'_, '_>, msg: &str) -> bool {
        // HarfBuzz formats into a 100-byte C string, including its terminator.
        let mut end = msg.find('\0').unwrap_or(msg.len()).min(99);
        while !msg.is_char_boundary(end) {
            end -= 1;
        }
        let msg = &msg[..end];
        if let Some(mut function) = self.message_function.take() {
            // Substitution keeps separate input/output cursors. Present the
            // emitted prefix followed by the remaining input without syncing
            // the live buffer, which could change its allocation-limit behavior.
            let ret = if self.have_output && (self.have_separate_output || self.out_len != self.idx)
            {
                let mut info = Vec::with_capacity(self.out_len + self.len - self.idx);
                info.extend_from_slice(&self.out_info()[..self.out_len]);
                info.extend_from_slice(&self.info[self.idx..self.len]);
                let snapshot = Buffer {
                    flags: self.flags,
                    cluster_level: self.cluster_level,
                    invisible: self.invisible,
                    not_found_variation_selector: self.not_found_variation_selector,
                    content_type: self.content_type,
                    direction: self.direction,
                    script: self.script,
                    language: self.language.clone(),
                    successful: self.successful,
                    len: info.len(),
                    info,
                    context: self.context,
                    context_len: self.context_len,
                    ..Buffer::new()
                };
                function(&snapshot, font, msg)
            } else {
                function(self, font, msg)
            };
            self.message_function = Some(function);
            ret
        } else {
            true
        }
    }

    // The cursor position in the logical buffer shown to message callbacks.
    #[cfg(feature = "tracing")]
    pub(crate) fn message_idx(&self) -> usize {
        if self.have_output {
            self.out_len
        } else {
            self.idx
        }
    }

    #[cfg(feature = "tracing")]
    #[inline]
    pub(crate) fn messaging(&self) -> bool {
        self.message_function.is_some()
    }

    pub(crate) fn serialize_impl(
        &self,
        font: Option<&crate::ShaperFont<'_, '_>>,
        flags: SerializeFlags,
    ) -> Result<String, core::fmt::Error> {
        use core::fmt::Write;

        let mut s = String::with_capacity(64);

        let info = self.glyph_infos();
        if self.content_type == Some(ContentType::Unicode) {
            for info in info {
                s.push(if s.is_empty() { '<' } else { '|' });
                write!(&mut s, "U+{:04X}", info.glyph_id)?;
                if !flags.contains(SerializeFlags::NO_CLUSTERS) {
                    write!(&mut s, "={}", info.cluster)?;
                }
            }
            if !s.is_empty() {
                s.push('>');
            }
            return Ok(s);
        }
        let pos = self.glyph_positions();
        let default_pos = GlyphPosition::default();
        let mut x: i32 = 0;
        let mut y: i32 = 0;
        for (index, info) in info.iter().enumerate() {
            let position = pos.get(index).unwrap_or(&default_pos);
            s.push(if s.is_empty() { '[' } else { '|' });

            if !flags.contains(SerializeFlags::NO_GLYPH_NAMES) {
                match font.and_then(|font| font.glyph_name(info.as_glyph())) {
                    Some(name) => s.push_str(name.as_str()),
                    None => write!(&mut s, "gid{}", info.glyph_id)?,
                }
            } else {
                write!(&mut s, "{}", info.glyph_id)?;
            }

            if !flags.contains(SerializeFlags::NO_CLUSTERS) {
                write!(&mut s, "={}", info.cluster)?;
            }

            if !pos.is_empty() && !flags.contains(SerializeFlags::NO_POSITIONS) {
                let dx = x.saturating_add(position.x_offset);
                let dy = y.saturating_add(position.y_offset);
                if dx != 0 || dy != 0 {
                    write!(&mut s, "@{dx},{dy}")?;
                }

                if !flags.contains(SerializeFlags::NO_ADVANCES) {
                    write!(&mut s, "+{}", position.x_advance)?;
                    if position.y_advance != 0 {
                        write!(&mut s, ",{}", position.y_advance)?;
                    }
                }
            }

            if flags.contains(SerializeFlags::GLYPH_FLAGS) {
                if info.mask & GlyphFlags::DEFINED_BITS != 0 {
                    write!(&mut s, "#{:X}", info.mask & GlyphFlags::DEFINED_BITS)?;
                }
            }

            if flags.contains(SerializeFlags::GLYPH_EXTENTS) {
                if let Some(extents) = font.and_then(|font| font.glyph_extents(info.as_glyph())) {
                    write!(
                        &mut s,
                        "<{},{},{},{}>",
                        extents.x_bearing, extents.y_bearing, extents.width, extents.height
                    )?;
                }
            }

            if flags.contains(SerializeFlags::NO_ADVANCES) {
                x = x.saturating_add(position.x_advance);
                y = y.saturating_add(position.y_advance);
            }
        }

        if !s.is_empty() {
            s.push(']');
        }

        Ok(s)
    }
}

impl Default for Buffer {
    fn default() -> Self {
        Self::new()
    }
}

impl core::fmt::Debug for Buffer {
    fn fmt(&self, fmt: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        fmt.debug_struct("Buffer")
            .field("content_type", &self.content_type())
            .field("direction", &self.direction())
            .field("language", &self.language())
            .field("script", &self.script())
            .field("cluster_level", &self.cluster_level())
            .field("len", &self.len())
            .finish()
    }
}

// All arguments (including formatting and font references) disappear when
// tracing is disabled. Allocate message text only when a callback is installed.
macro_rules! buffer_message {
    ($buffer:expr, $font:expr, $($arg:tt)*) => {{
        #[cfg(feature = "tracing")]
        {
            if $buffer.messaging() {
                let msg = alloc::format!($($arg)*);
                $buffer.message($font, &msg)
            } else {
                true
            }
        }
        #[cfg(not(feature = "tracing"))]
        { true }
    }};
}

macro_rules! message {
    ($ctx:expr, $($arg:tt)*) => {
        buffer_message!($ctx.buffer, $ctx.font, $($arg)*)
    };
}

macro_rules! message_return {
    ($ctx:expr, $($arg:tt)*) => {
        if !message!($ctx, $($arg)*) { return; }
    };
}

macro_rules! message_continue {
    ($ctx:expr, $($arg:tt)*) => {
        if !message!($ctx, $($arg)*) { continue; }
    };
}

pub(crate) fn _cluster_group_func(a: &GlyphInfo, b: &GlyphInfo) -> bool {
    a.cluster == b.cluster
}

// TODO: to iter if possible

/// Sets `mask` on the glyphs in `start..end` outside `cluster`, as far as the
/// cluster level allows.
#[inline(always)]
fn set_cluster_flags(
    infos: &mut [GlyphInfo],
    cluster_level: ClusterLevelCode,
    start: usize,
    end: usize,
    cluster: u32,
    mask: Mask,
) -> Option<()> {
    let cluster_first = infos.get(start)?.cluster;
    let cluster_last = infos.get(end - 1)?.cluster;

    if cluster_level == HB_BUFFER_CLUSTER_LEVEL_CHARACTERS
        || (cluster != cluster_first && cluster != cluster_last)
    {
        for info in infos.get_mut(start..end)? {
            if info.cluster != cluster {
                info.mask |= mask;
            }
        }
        return Some(());
    }

    // Monotone clusters
    if cluster == cluster_first {
        let mut i = end;
        while start < i {
            let info = infos.get_mut(i - 1)?;
            if info.cluster == cluster_first {
                break;
            }
            if cluster != info.cluster {
                info.mask |= mask;
            }
            i -= 1;
        }
    } else {
        let mut i = start;
        while i < end {
            let info = infos.get_mut(i)?;
            if info.cluster == cluster_last {
                break;
            }
            if cluster != info.cluster {
                info.mask |= mask;
            }
            i += 1;
        }
    }
    Some(())
}

macro_rules! foreach_cluster {
    ($buffer:expr, $start:ident, $end:ident, $($body:tt)*) => {
        foreach_group!($buffer, $start, $end, $crate::buffer::_cluster_group_func, $($body)*)
    };
}

macro_rules! foreach_group {
    ($buffer:expr, $start:ident, $end:ident, $group_func:expr, $($body:tt)*) => {{
        let count = $buffer.len;
        let mut $start = 0;
        let mut $end = if count > 0 { $buffer.group_end(0, $group_func) } else { 0 };

        while $start < count {
            $($body)*;
            $start = $end;
            $end = $buffer.group_end($start, $group_func);
        }
    }};
}

macro_rules! foreach_syllable {
    ($buffer:expr, $start:ident, $end:ident, $($body:tt)*) => {{
        let mut $start = 0;
        let mut $end = $buffer.next_syllable(0);
        while $start < $buffer.len {
            $($body)*;
            $start = $end;
            $end = $buffer.next_syllable($start);
        }
    }};
}

macro_rules! foreach_grapheme {
    ($buffer:expr, $start:ident, $end:ident, $($body:tt)*) => {
        foreach_group!($buffer, $start, $end, $crate::ot::layout::grapheme_group, $($body)*)
    };
}

bitflags::bitflags! {
    #[derive(Default, Debug, Clone, Copy)]
    pub struct UnicodeProps: u16 {
        const GENERAL_CATEGORY  = 0x001F;
        const IGNORABLE         = 0x0020;
        // MONGOLIAN FREE VARIATION SELECTOR 1..4, or TAG characters, or CGJ sometimes
        const HIDDEN            = 0x0040;
        const CONTINUATION      = 0x0080;

        // If GEN_CAT=FORMAT, top byte masks:
        const CF_ZWJ            = 0x0100;
        const CF_ZWNJ           = 0x0200;
        const CF_VS             = 0x0400;
        const CF_AAT_DELETED    = 0x0800;
    }
}

bitflags::bitflags! {
    #[derive(Default, Debug, Clone, Copy)]
    pub struct GlyphPropsFlags: u16 {
        // The following three match LookupFlags::Ignore* numbers.
        const BASE_GLYPH    = 0x02;
        const LIGATURE      = 0x04;
        const MARK          = 0x08;
        const CLASS_MASK    = Self::BASE_GLYPH.bits() | Self::LIGATURE.bits() | Self::MARK.bits();

        // The following are used internally; not derived from GDEF.
        const SUBSTITUTED   = 0x10;
        const LIGATED       = 0x20;
        const MULTIPLIED    = 0x40;

        const PRESERVE      = Self::SUBSTITUTED.bits() | Self::LIGATED.bits() | Self::MULTIPLIED.bits();
    }
}

pub type ScratchFlags = u32;
pub const HB_BUFFER_SCRATCH_FLAG_DEFAULT: u32 = 0x0000_0000;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_FRACTION_SLASH: u32 = 0x0000_0001;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_DEFAULT_IGNORABLES: u32 = 0x0000_0002;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_SPACE_FALLBACK: u32 = 0x0000_0004;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_GPOS_ATTACHMENT: u32 = 0x0000_0008;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_CGJ: u32 = 0x0000_0010;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_BROKEN_SYLLABLE: u32 = 0x0000_0020;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_VARIATION_SELECTOR_FALLBACK: u32 = 0x0000_0040;
pub const HB_BUFFER_SCRATCH_FLAG_HAS_CONTINUATIONS: u32 = 0x0000_0080;

/* Reserved for shapers' internal use. */
pub const HB_BUFFER_SCRATCH_FLAG_SHAPER0: u32 = 0x0100_0000;
// pub const HB_BUFFER_SCRATCH_FLAG_SHAPER1: u32 = 0x02000000;
// pub const HB_BUFFER_SCRATCH_FLAG_SHAPER2: u32 = 0x04000000;
// pub const HB_BUFFER_SCRATCH_FLAG_SHAPER3: u32 = 0x08000000;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn next_syllable_is_bounded() {
        let mut buffer = Buffer::new();
        buffer
            .info
            .resize(MAX_SYLLABLE_LENGTH + 1, GlyphInfo::default());
        buffer.len = buffer.info.len();
        for info in &mut buffer.info {
            info.set_syllable(1);
        }

        assert_eq!(buffer.next_syllable(0), MAX_SYLLABLE_LENGTH);
        assert_eq!(buffer.next_syllable(MAX_SYLLABLE_LENGTH), buffer.len);
    }
}
