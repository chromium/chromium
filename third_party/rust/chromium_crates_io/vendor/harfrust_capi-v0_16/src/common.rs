//! Scalar types, tags, directions, scripts, languages, features and
//! variations. Mirrors HarfBuzz's `hb-common.h`.
//!
//! Every enumerator and flag value here is numerically identical to its
//! HarfBuzz counterpart, so code can be ported by renaming `hb_` to `hr_`.

use core::ffi::{c_char, c_int, c_uint};
use core::str::FromStr;
use std::collections::BTreeMap;
use std::sync::{OnceLock, RwLock};

use harfrust::{font::Variation, Direction, Feature, Language, ParseSetting, Script, Tag};

/// HarfBuzz's OpenType rounding: halfway cases go towards positive infinity.
/// Promote f32 inputs before adding 0.5, matching `_hb_roundf`.
pub(crate) fn ot_round(value: impl Into<f64>) -> f64 {
    (value.into() + 0.5).floor()
}

/// A boolean, as C sees it: zero is false, non-zero is true.
pub type hr_bool_t = c_int;

/// A Unicode scalar value.
pub type hr_codepoint_t = u32;

/// A codepoint that is not one, which stands for the absence of a glyph or a
/// character wherever one is expected.
pub const HR_CODEPOINT_INVALID: hr_codepoint_t = u32::MAX;

/// A position, in whatever units the configured font scale implies.
pub type hr_position_t = i32;

/// A mask of feature bits applied to an item in a buffer.
pub type hr_mask_t = u32;

/// A four byte OpenType tag, packed big-endian into a 32 bit integer.
pub type hr_tag_t = u32;

/// The tag matching no script, language or feature.
pub const HR_TAG_NONE: hr_tag_t = 0u32;
/// The largest possible tag value.
pub const HR_TAG_MAX: hr_tag_t = 0xffff_ffffu32;
/// The largest possible tag value that is still signed-safe.
pub const HR_TAG_MAX_SIGNED: hr_tag_t = 0x7fff_ffffu32;

/// Value applied to a feature that covers the whole buffer, as its start.
pub const HR_FEATURE_GLOBAL_START: c_uint = 0u32;
/// Value applied to a feature that covers the whole buffer, as its end.
///
/// Spelled as a literal rather than `c_uint::MAX` so that it reaches the
/// generated header.
pub const HR_FEATURE_GLOBAL_END: c_uint = 0xFFFF_FFFFu32;

pub(crate) fn tag_to_rust(tag: hr_tag_t) -> Tag {
    Tag::from_be_bytes(tag.to_be_bytes())
}

pub(crate) fn tag_from_rust(tag: Tag) -> hr_tag_t {
    u32::from_be_bytes(tag.to_be_bytes())
}

/// Reads a C string of `len` bytes, or up to its NUL when `len` is negative.
///
/// # Safety
///
/// `ptr` must be `NULL`, or point to `len` readable bytes, or to a
/// NUL-terminated string when `len` is negative.
pub(crate) unsafe fn str_from_raw<'a>(ptr: *const c_char, len: c_int) -> Option<&'a str> {
    if ptr.is_null() {
        return None;
    }
    let bytes = if len < 0 {
        unsafe { core::ffi::CStr::from_ptr(ptr) }.to_bytes()
    } else {
        unsafe { core::slice::from_raw_parts(ptr.cast::<u8>(), len as usize) }
    };
    // Stop at an embedded NUL, as HarfBuzz does.
    let bytes = match bytes.iter().position(|&b| b == 0) {
        Some(nul) => &bytes[..nul],
        None => bytes,
    };
    core::str::from_utf8(bytes).ok()
}

/// Converts a string into a tag, padding with spaces and truncating past four
/// bytes.
///
/// Pass a negative `len` for a NUL-terminated string.
///
/// # Safety
///
/// See [`str_from_raw`].
#[no_mangle]
pub unsafe extern "C" fn hr_tag_from_string(str_: *const c_char, len: c_int) -> hr_tag_t {
    if str_.is_null() || len == 0 {
        return HR_TAG_NONE;
    }
    let len = if len < 0 { 4 } else { (len as usize).min(4) };
    let mut tag = [b' '; 4];
    for (index, slot) in tag[..len].iter_mut().enumerate() {
        // Tags are four raw bytes. Do not scan or validate the rest of a
        // C string, which neither this tag nor a script parsed from it uses.
        let byte = unsafe { str_.cast::<u8>().add(index).read() };
        if byte == 0 {
            if index == 0 {
                return HR_TAG_NONE;
            }
            break;
        }
        *slot = byte;
    }
    u32::from_be_bytes(tag)
}

/// Writes a tag's four bytes into `buf`. No terminating NUL is written.
///
/// # Safety
///
/// `buf` must point to at least four writable bytes.
#[no_mangle]
pub unsafe extern "C" fn hr_tag_to_string(tag: hr_tag_t, buf: *mut c_char) {
    if buf.is_null() {
        return;
    }
    let bytes = tag.to_be_bytes();
    unsafe { core::ptr::copy_nonoverlapping(bytes.as_ptr().cast::<c_char>(), buf, 4) };
}

/// The direction in which text is set.
///
/// This is an integer typedef rather than an enumeration because it appears in
/// [`hr_segment_properties_t`], which callers fill in themselves; a value
/// outside the set below is then merely unrecognised rather than undefined.
pub type hr_direction_t = u32;

/// Initial, unset direction.
pub const HR_DIRECTION_INVALID: hr_direction_t = 0u32;
/// Text is set horizontally from left to right.
pub const HR_DIRECTION_LTR: hr_direction_t = 4u32;
/// Text is set horizontally from right to left.
pub const HR_DIRECTION_RTL: hr_direction_t = 5u32;
/// Text is set vertically from top to bottom.
pub const HR_DIRECTION_TTB: hr_direction_t = 6u32;
/// Text is set vertically from bottom to top.
pub const HR_DIRECTION_BTT: hr_direction_t = 7u32;

pub(crate) fn direction_to_rust(direction: hr_direction_t) -> Direction {
    match direction {
        HR_DIRECTION_LTR => Direction::LeftToRight,
        HR_DIRECTION_RTL => Direction::RightToLeft,
        HR_DIRECTION_TTB => Direction::TopToBottom,
        HR_DIRECTION_BTT => Direction::BottomToTop,
        _ => Direction::Invalid,
    }
}

pub(crate) fn direction_from_rust(direction: Direction) -> hr_direction_t {
    match direction {
        Direction::Invalid => HR_DIRECTION_INVALID,
        Direction::LeftToRight => HR_DIRECTION_LTR,
        Direction::RightToLeft => HR_DIRECTION_RTL,
        Direction::TopToBottom => HR_DIRECTION_TTB,
        Direction::BottomToTop => HR_DIRECTION_BTT,
    }
}

/// Parses a direction from its name, matching on the first letter only.
///
/// # Safety
///
/// See [`str_from_raw`].
#[no_mangle]
pub unsafe extern "C" fn hr_direction_from_string(
    str_: *const c_char,
    len: c_int,
) -> hr_direction_t {
    let Some(s) = (unsafe { str_from_raw(str_, len) }) else {
        return HR_DIRECTION_INVALID;
    };
    Direction::from_str(s).map_or(HR_DIRECTION_INVALID, direction_from_rust)
}

/// Returns the name of a direction, as a NUL-terminated static string.
#[no_mangle]
pub extern "C" fn hr_direction_to_string(direction: hr_direction_t) -> *const c_char {
    let name: &[u8] = match direction {
        HR_DIRECTION_LTR => b"ltr\0",
        HR_DIRECTION_RTL => b"rtl\0",
        HR_DIRECTION_TTB => b"ttb\0",
        HR_DIRECTION_BTT => b"btt\0",
        _ => b"invalid\0",
    };
    name.as_ptr().cast::<c_char>()
}

/// Returns whether a direction is horizontal.
#[no_mangle]
pub extern "C" fn hr_direction_is_horizontal(direction: hr_direction_t) -> hr_bool_t {
    matches!(direction, HR_DIRECTION_LTR | HR_DIRECTION_RTL).into()
}

/// Returns whether a direction is vertical.
#[no_mangle]
pub extern "C" fn hr_direction_is_vertical(direction: hr_direction_t) -> hr_bool_t {
    matches!(direction, HR_DIRECTION_TTB | HR_DIRECTION_BTT).into()
}

/// Returns whether a direction runs forwards (left-to-right or top-to-bottom).
#[no_mangle]
pub extern "C" fn hr_direction_is_forward(direction: hr_direction_t) -> hr_bool_t {
    matches!(direction, HR_DIRECTION_LTR | HR_DIRECTION_TTB).into()
}

/// Returns whether a direction runs backwards.
#[no_mangle]
pub extern "C" fn hr_direction_is_backward(direction: hr_direction_t) -> hr_bool_t {
    matches!(direction, HR_DIRECTION_RTL | HR_DIRECTION_BTT).into()
}

/// Returns whether a direction is set at all.
#[no_mangle]
pub extern "C" fn hr_direction_is_valid(direction: hr_direction_t) -> hr_bool_t {
    matches!(
        direction,
        HR_DIRECTION_LTR | HR_DIRECTION_RTL | HR_DIRECTION_TTB | HR_DIRECTION_BTT
    )
    .into()
}

/// Returns the direction running opposite to the given one.
#[no_mangle]
pub extern "C" fn hr_direction_reverse(direction: hr_direction_t) -> hr_direction_t {
    // The low bit is the one that separates each pair, so flipping it is the
    // whole operation -- which is how HarfBuzz spells it, down to leaving a
    // direction that is not one as something else that is not one.
    direction ^ 1
}

/// An ISO 15924 script, held as its four byte tag.
///
/// HarfBuzz declares `hb_script_t` as an enum; here it is a tag typedef so
/// that arbitrary scripts round-trip without a cast. The `HR_SCRIPT_*`
/// constants below carry the same values as HarfBuzz's enumerators.
pub type hr_script_t = hr_tag_t;

/// The script matching no script at all.
pub const HR_SCRIPT_INVALID: hr_script_t = HR_TAG_NONE;

/// The Common script (`Zyyy`).
pub const HR_SCRIPT_COMMON: hr_script_t = 0x5A79_7979u32;
/// The Inherited script (`Zinh`).
pub const HR_SCRIPT_INHERITED: hr_script_t = 0x5A69_6E68u32;
/// The Arabic script (`Arab`).
pub const HR_SCRIPT_ARABIC: hr_script_t = 0x4172_6162u32;
/// The Armenian script (`Armn`).
pub const HR_SCRIPT_ARMENIAN: hr_script_t = 0x4172_6D6Eu32;
/// The Bengali script (`Beng`).
pub const HR_SCRIPT_BENGALI: hr_script_t = 0x4265_6E67u32;
/// The Cyrillic script (`Cyrl`).
pub const HR_SCRIPT_CYRILLIC: hr_script_t = 0x4379_726Cu32;
/// The Devanagari script (`Deva`).
pub const HR_SCRIPT_DEVANAGARI: hr_script_t = 0x4465_7661u32;
/// The Georgian script (`Geor`).
pub const HR_SCRIPT_GEORGIAN: hr_script_t = 0x4765_6F72u32;
/// The Greek script (`Grek`).
pub const HR_SCRIPT_GREEK: hr_script_t = 0x4772_656Bu32;
/// The Gujarati script (`Gujr`).
pub const HR_SCRIPT_GUJARATI: hr_script_t = 0x4775_6A72u32;
/// The Gurmukhi script (`Guru`).
pub const HR_SCRIPT_GURMUKHI: hr_script_t = 0x4775_7275u32;
/// The Hangul script (`Hang`).
pub const HR_SCRIPT_HANGUL: hr_script_t = 0x4861_6E67u32;
/// The Han script (`Hani`).
pub const HR_SCRIPT_HAN: hr_script_t = 0x4861_6E69u32;
/// The Hebrew script (`Hebr`).
pub const HR_SCRIPT_HEBREW: hr_script_t = 0x4865_6272u32;
/// The Hiragana script (`Hira`).
pub const HR_SCRIPT_HIRAGANA: hr_script_t = 0x4869_7261u32;
/// The Kannada script (`Knda`).
pub const HR_SCRIPT_KANNADA: hr_script_t = 0x4B6E_6461u32;
/// The Katakana script (`Kana`).
pub const HR_SCRIPT_KATAKANA: hr_script_t = 0x4B61_6E61u32;
/// The Lao script (`Laoo`).
pub const HR_SCRIPT_LAO: hr_script_t = 0x4C61_6F6Fu32;
/// The Latin script (`Latn`).
pub const HR_SCRIPT_LATIN: hr_script_t = 0x4C61_746Eu32;
/// The Malayalam script (`Mlym`).
pub const HR_SCRIPT_MALAYALAM: hr_script_t = 0x4D6C_796Du32;
/// The Oriya script (`Orya`).
pub const HR_SCRIPT_ORIYA: hr_script_t = 0x4F72_7961u32;
/// The Tamil script (`Taml`).
pub const HR_SCRIPT_TAMIL: hr_script_t = 0x5461_6D6Cu32;
/// The Telugu script (`Telu`).
pub const HR_SCRIPT_TELUGU: hr_script_t = 0x5465_6C75u32;
/// The Thai script (`Thai`).
pub const HR_SCRIPT_THAI: hr_script_t = 0x5468_6169u32;
/// The Tibetan script (`Tibt`).
pub const HR_SCRIPT_TIBETAN: hr_script_t = 0x5469_6274u32;
/// The Bopomofo script (`Bopo`).
pub const HR_SCRIPT_BOPOMOFO: hr_script_t = 0x426F_706Fu32;
/// The Braille script (`Brai`).
pub const HR_SCRIPT_BRAILLE: hr_script_t = 0x4272_6169u32;
/// The Canadian Syllabics script (`Cans`).
pub const HR_SCRIPT_CANADIAN_SYLLABICS: hr_script_t = 0x4361_6E73u32;
/// The Cherokee script (`Cher`).
pub const HR_SCRIPT_CHEROKEE: hr_script_t = 0x4368_6572u32;
/// The Ethiopic script (`Ethi`).
pub const HR_SCRIPT_ETHIOPIC: hr_script_t = 0x4574_6869u32;
/// The Khmer script (`Khmr`).
pub const HR_SCRIPT_KHMER: hr_script_t = 0x4B68_6D72u32;
/// The Mongolian script (`Mong`).
pub const HR_SCRIPT_MONGOLIAN: hr_script_t = 0x4D6F_6E67u32;
/// The Myanmar script (`Mymr`).
pub const HR_SCRIPT_MYANMAR: hr_script_t = 0x4D79_6D72u32;
/// The Ogham script (`Ogam`).
pub const HR_SCRIPT_OGHAM: hr_script_t = 0x4F67_616Du32;
/// The Runic script (`Runr`).
pub const HR_SCRIPT_RUNIC: hr_script_t = 0x5275_6E72u32;
/// The Sinhala script (`Sinh`).
pub const HR_SCRIPT_SINHALA: hr_script_t = 0x5369_6E68u32;
/// The Syriac script (`Syrc`).
pub const HR_SCRIPT_SYRIAC: hr_script_t = 0x5379_7263u32;
/// The Thaana script (`Thaa`).
pub const HR_SCRIPT_THAANA: hr_script_t = 0x5468_6161u32;
/// The Yi script (`Yiii`).
pub const HR_SCRIPT_YI: hr_script_t = 0x5969_6969u32;
/// The Deseret script (`Dsrt`).
pub const HR_SCRIPT_DESERET: hr_script_t = 0x4473_7274u32;
/// The Gothic script (`Goth`).
pub const HR_SCRIPT_GOTHIC: hr_script_t = 0x476F_7468u32;
/// The Old Italic script (`Ital`).
pub const HR_SCRIPT_OLD_ITALIC: hr_script_t = 0x4974_616Cu32;
/// The Buhid script (`Buhd`).
pub const HR_SCRIPT_BUHID: hr_script_t = 0x4275_6864u32;
/// The Hanunoo script (`Hano`).
pub const HR_SCRIPT_HANUNOO: hr_script_t = 0x4861_6E6Fu32;
/// The Tagalog script (`Tglg`).
pub const HR_SCRIPT_TAGALOG: hr_script_t = 0x5467_6C67u32;
/// The Tagbanwa script (`Tagb`).
pub const HR_SCRIPT_TAGBANWA: hr_script_t = 0x5461_6762u32;
/// The Cypriot script (`Cprt`).
pub const HR_SCRIPT_CYPRIOT: hr_script_t = 0x4370_7274u32;
/// The Limbu script (`Limb`).
pub const HR_SCRIPT_LIMBU: hr_script_t = 0x4C69_6D62u32;
/// The Linear B script (`Linb`).
pub const HR_SCRIPT_LINEAR_B: hr_script_t = 0x4C69_6E62u32;
/// The Osmanya script (`Osma`).
pub const HR_SCRIPT_OSMANYA: hr_script_t = 0x4F73_6D61u32;
/// The Shavian script (`Shaw`).
pub const HR_SCRIPT_SHAVIAN: hr_script_t = 0x5368_6177u32;
/// The Tai Le script (`Tale`).
pub const HR_SCRIPT_TAI_LE: hr_script_t = 0x5461_6C65u32;
/// The Ugaritic script (`Ugar`).
pub const HR_SCRIPT_UGARITIC: hr_script_t = 0x5567_6172u32;
/// The Buginese script (`Bugi`).
pub const HR_SCRIPT_BUGINESE: hr_script_t = 0x4275_6769u32;
/// The Coptic script (`Copt`).
pub const HR_SCRIPT_COPTIC: hr_script_t = 0x436F_7074u32;
/// The Glagolitic script (`Glag`).
pub const HR_SCRIPT_GLAGOLITIC: hr_script_t = 0x476C_6167u32;
/// The Kharoshthi script (`Khar`).
pub const HR_SCRIPT_KHAROSHTHI: hr_script_t = 0x4B68_6172u32;
/// The New Tai Lue script (`Talu`).
pub const HR_SCRIPT_NEW_TAI_LUE: hr_script_t = 0x5461_6C75u32;
/// The Old Persian script (`Xpeo`).
pub const HR_SCRIPT_OLD_PERSIAN: hr_script_t = 0x5870_656Fu32;
/// The Syloti Nagri script (`Sylo`).
pub const HR_SCRIPT_SYLOTI_NAGRI: hr_script_t = 0x5379_6C6Fu32;
/// The Tifinagh script (`Tfng`).
pub const HR_SCRIPT_TIFINAGH: hr_script_t = 0x5466_6E67u32;
/// The Unknown script (`Zzzz`).
pub const HR_SCRIPT_UNKNOWN: hr_script_t = 0x5A7A_7A7Au32;
/// The Balinese script (`Bali`).
pub const HR_SCRIPT_BALINESE: hr_script_t = 0x4261_6C69u32;
/// The Cuneiform script (`Xsux`).
pub const HR_SCRIPT_CUNEIFORM: hr_script_t = 0x5873_7578u32;
/// The Nko script (`Nkoo`).
pub const HR_SCRIPT_NKO: hr_script_t = 0x4E6B_6F6Fu32;
/// The Phags Pa script (`Phag`).
pub const HR_SCRIPT_PHAGS_PA: hr_script_t = 0x5068_6167u32;
/// The Phoenician script (`Phnx`).
pub const HR_SCRIPT_PHOENICIAN: hr_script_t = 0x5068_6E78u32;
/// The Carian script (`Cari`).
pub const HR_SCRIPT_CARIAN: hr_script_t = 0x4361_7269u32;
/// The Cham script (`Cham`).
pub const HR_SCRIPT_CHAM: hr_script_t = 0x4368_616Du32;
/// The Kayah Li script (`Kali`).
pub const HR_SCRIPT_KAYAH_LI: hr_script_t = 0x4B61_6C69u32;
/// The Lepcha script (`Lepc`).
pub const HR_SCRIPT_LEPCHA: hr_script_t = 0x4C65_7063u32;
/// The Lycian script (`Lyci`).
pub const HR_SCRIPT_LYCIAN: hr_script_t = 0x4C79_6369u32;
/// The Lydian script (`Lydi`).
pub const HR_SCRIPT_LYDIAN: hr_script_t = 0x4C79_6469u32;
/// The Ol Chiki script (`Olck`).
pub const HR_SCRIPT_OL_CHIKI: hr_script_t = 0x4F6C_636Bu32;
/// The Rejang script (`Rjng`).
pub const HR_SCRIPT_REJANG: hr_script_t = 0x526A_6E67u32;
/// The Saurashtra script (`Saur`).
pub const HR_SCRIPT_SAURASHTRA: hr_script_t = 0x5361_7572u32;
/// The Sundanese script (`Sund`).
pub const HR_SCRIPT_SUNDANESE: hr_script_t = 0x5375_6E64u32;
/// The Vai script (`Vaii`).
pub const HR_SCRIPT_VAI: hr_script_t = 0x5661_6969u32;
/// The Avestan script (`Avst`).
pub const HR_SCRIPT_AVESTAN: hr_script_t = 0x4176_7374u32;
/// The Bamum script (`Bamu`).
pub const HR_SCRIPT_BAMUM: hr_script_t = 0x4261_6D75u32;
/// The Egyptian Hieroglyphs script (`Egyp`).
pub const HR_SCRIPT_EGYPTIAN_HIEROGLYPHS: hr_script_t = 0x4567_7970u32;
/// The Imperial Aramaic script (`Armi`).
pub const HR_SCRIPT_IMPERIAL_ARAMAIC: hr_script_t = 0x4172_6D69u32;
/// The Inscriptional Pahlavi script (`Phli`).
pub const HR_SCRIPT_INSCRIPTIONAL_PAHLAVI: hr_script_t = 0x5068_6C69u32;
/// The Inscriptional Parthian script (`Prti`).
pub const HR_SCRIPT_INSCRIPTIONAL_PARTHIAN: hr_script_t = 0x5072_7469u32;
/// The Javanese script (`Java`).
pub const HR_SCRIPT_JAVANESE: hr_script_t = 0x4A61_7661u32;
/// The Kaithi script (`Kthi`).
pub const HR_SCRIPT_KAITHI: hr_script_t = 0x4B74_6869u32;
/// The Lisu script (`Lisu`).
pub const HR_SCRIPT_LISU: hr_script_t = 0x4C69_7375u32;
/// The Meetei Mayek script (`Mtei`).
pub const HR_SCRIPT_MEETEI_MAYEK: hr_script_t = 0x4D74_6569u32;
/// The Old South Arabian script (`Sarb`).
pub const HR_SCRIPT_OLD_SOUTH_ARABIAN: hr_script_t = 0x5361_7262u32;
/// The Old Turkic script (`Orkh`).
pub const HR_SCRIPT_OLD_TURKIC: hr_script_t = 0x4F72_6B68u32;
/// The Samaritan script (`Samr`).
pub const HR_SCRIPT_SAMARITAN: hr_script_t = 0x5361_6D72u32;
/// The Tai Tham script (`Lana`).
pub const HR_SCRIPT_TAI_THAM: hr_script_t = 0x4C61_6E61u32;
/// The Tai Viet script (`Tavt`).
pub const HR_SCRIPT_TAI_VIET: hr_script_t = 0x5461_7674u32;
/// The Batak script (`Batk`).
pub const HR_SCRIPT_BATAK: hr_script_t = 0x4261_746Bu32;
/// The Brahmi script (`Brah`).
pub const HR_SCRIPT_BRAHMI: hr_script_t = 0x4272_6168u32;
/// The Mandaic script (`Mand`).
pub const HR_SCRIPT_MANDAIC: hr_script_t = 0x4D61_6E64u32;
/// The Chakma script (`Cakm`).
pub const HR_SCRIPT_CHAKMA: hr_script_t = 0x4361_6B6Du32;
/// The Meroitic Cursive script (`Merc`).
pub const HR_SCRIPT_MEROITIC_CURSIVE: hr_script_t = 0x4D65_7263u32;
/// The Meroitic Hieroglyphs script (`Mero`).
pub const HR_SCRIPT_MEROITIC_HIEROGLYPHS: hr_script_t = 0x4D65_726Fu32;
/// The Miao script (`Plrd`).
pub const HR_SCRIPT_MIAO: hr_script_t = 0x506C_7264u32;
/// The Sharada script (`Shrd`).
pub const HR_SCRIPT_SHARADA: hr_script_t = 0x5368_7264u32;
/// The Sora Sompeng script (`Sora`).
pub const HR_SCRIPT_SORA_SOMPENG: hr_script_t = 0x536F_7261u32;
/// The Takri script (`Takr`).
pub const HR_SCRIPT_TAKRI: hr_script_t = 0x5461_6B72u32;
/// The Bassa Vah script (`Bass`).
pub const HR_SCRIPT_BASSA_VAH: hr_script_t = 0x4261_7373u32;
/// The Caucasian Albanian script (`Aghb`).
pub const HR_SCRIPT_CAUCASIAN_ALBANIAN: hr_script_t = 0x4167_6862u32;
/// The Duployan script (`Dupl`).
pub const HR_SCRIPT_DUPLOYAN: hr_script_t = 0x4475_706Cu32;
/// The Elbasan script (`Elba`).
pub const HR_SCRIPT_ELBASAN: hr_script_t = 0x456C_6261u32;
/// The Grantha script (`Gran`).
pub const HR_SCRIPT_GRANTHA: hr_script_t = 0x4772_616Eu32;
/// The Khojki script (`Khoj`).
pub const HR_SCRIPT_KHOJKI: hr_script_t = 0x4B68_6F6Au32;
/// The Khudawadi script (`Sind`).
pub const HR_SCRIPT_KHUDAWADI: hr_script_t = 0x5369_6E64u32;
/// The Linear A script (`Lina`).
pub const HR_SCRIPT_LINEAR_A: hr_script_t = 0x4C69_6E61u32;
/// The Mahajani script (`Mahj`).
pub const HR_SCRIPT_MAHAJANI: hr_script_t = 0x4D61_686Au32;
/// The Manichaean script (`Mani`).
pub const HR_SCRIPT_MANICHAEAN: hr_script_t = 0x4D61_6E69u32;
/// The Mende Kikakui script (`Mend`).
pub const HR_SCRIPT_MENDE_KIKAKUI: hr_script_t = 0x4D65_6E64u32;
/// The Modi script (`Modi`).
pub const HR_SCRIPT_MODI: hr_script_t = 0x4D6F_6469u32;
/// The Mro script (`Mroo`).
pub const HR_SCRIPT_MRO: hr_script_t = 0x4D72_6F6Fu32;
/// The Nabataean script (`Nbat`).
pub const HR_SCRIPT_NABATAEAN: hr_script_t = 0x4E62_6174u32;
/// The Old North Arabian script (`Narb`).
pub const HR_SCRIPT_OLD_NORTH_ARABIAN: hr_script_t = 0x4E61_7262u32;
/// The Old Permic script (`Perm`).
pub const HR_SCRIPT_OLD_PERMIC: hr_script_t = 0x5065_726Du32;
/// The Pahawh Hmong script (`Hmng`).
pub const HR_SCRIPT_PAHAWH_HMONG: hr_script_t = 0x486D_6E67u32;
/// The Palmyrene script (`Palm`).
pub const HR_SCRIPT_PALMYRENE: hr_script_t = 0x5061_6C6Du32;
/// The Pau Cin Hau script (`Pauc`).
pub const HR_SCRIPT_PAU_CIN_HAU: hr_script_t = 0x5061_7563u32;
/// The Psalter Pahlavi script (`Phlp`).
pub const HR_SCRIPT_PSALTER_PAHLAVI: hr_script_t = 0x5068_6C70u32;
/// The Siddham script (`Sidd`).
pub const HR_SCRIPT_SIDDHAM: hr_script_t = 0x5369_6464u32;
/// The Tirhuta script (`Tirh`).
pub const HR_SCRIPT_TIRHUTA: hr_script_t = 0x5469_7268u32;
/// The Warang Citi script (`Wara`).
pub const HR_SCRIPT_WARANG_CITI: hr_script_t = 0x5761_7261u32;
/// The Ahom script (`Ahom`).
pub const HR_SCRIPT_AHOM: hr_script_t = 0x4168_6F6Du32;
/// The Anatolian Hieroglyphs script (`Hluw`).
pub const HR_SCRIPT_ANATOLIAN_HIEROGLYPHS: hr_script_t = 0x486C_7577u32;
/// The Hatran script (`Hatr`).
pub const HR_SCRIPT_HATRAN: hr_script_t = 0x4861_7472u32;
/// The Multani script (`Mult`).
pub const HR_SCRIPT_MULTANI: hr_script_t = 0x4D75_6C74u32;
/// The Old Hungarian script (`Hung`).
pub const HR_SCRIPT_OLD_HUNGARIAN: hr_script_t = 0x4875_6E67u32;
/// The Signwriting script (`Sgnw`).
pub const HR_SCRIPT_SIGNWRITING: hr_script_t = 0x5367_6E77u32;
/// The Adlam script (`Adlm`).
pub const HR_SCRIPT_ADLAM: hr_script_t = 0x4164_6C6Du32;
/// The Bhaiksuki script (`Bhks`).
pub const HR_SCRIPT_BHAIKSUKI: hr_script_t = 0x4268_6B73u32;
/// The Marchen script (`Marc`).
pub const HR_SCRIPT_MARCHEN: hr_script_t = 0x4D61_7263u32;
/// The Osage script (`Osge`).
pub const HR_SCRIPT_OSAGE: hr_script_t = 0x4F73_6765u32;
/// The Tangut script (`Tang`).
pub const HR_SCRIPT_TANGUT: hr_script_t = 0x5461_6E67u32;
/// The Newa script (`Newa`).
pub const HR_SCRIPT_NEWA: hr_script_t = 0x4E65_7761u32;
/// The Masaram Gondi script (`Gonm`).
pub const HR_SCRIPT_MASARAM_GONDI: hr_script_t = 0x476F_6E6Du32;
/// The Nushu script (`Nshu`).
pub const HR_SCRIPT_NUSHU: hr_script_t = 0x4E73_6875u32;
/// The Soyombo script (`Soyo`).
pub const HR_SCRIPT_SOYOMBO: hr_script_t = 0x536F_796Fu32;
/// The Zanabazar Square script (`Zanb`).
pub const HR_SCRIPT_ZANABAZAR_SQUARE: hr_script_t = 0x5A61_6E62u32;
/// The Dogra script (`Dogr`).
pub const HR_SCRIPT_DOGRA: hr_script_t = 0x446F_6772u32;
/// The Gunjala Gondi script (`Gong`).
pub const HR_SCRIPT_GUNJALA_GONDI: hr_script_t = 0x476F_6E67u32;
/// The Hanifi Rohingya script (`Rohg`).
pub const HR_SCRIPT_HANIFI_ROHINGYA: hr_script_t = 0x526F_6867u32;
/// The Makasar script (`Maka`).
pub const HR_SCRIPT_MAKASAR: hr_script_t = 0x4D61_6B61u32;
/// The Medefaidrin script (`Medf`).
pub const HR_SCRIPT_MEDEFAIDRIN: hr_script_t = 0x4D65_6466u32;
/// The Old Sogdian script (`Sogo`).
pub const HR_SCRIPT_OLD_SOGDIAN: hr_script_t = 0x536F_676Fu32;
/// The Sogdian script (`Sogd`).
pub const HR_SCRIPT_SOGDIAN: hr_script_t = 0x536F_6764u32;
/// The Elymaic script (`Elym`).
pub const HR_SCRIPT_ELYMAIC: hr_script_t = 0x456C_796Du32;
/// The Nandinagari script (`Nand`).
pub const HR_SCRIPT_NANDINAGARI: hr_script_t = 0x4E61_6E64u32;
/// The Nyiakeng Puachue Hmong script (`Hmnp`).
pub const HR_SCRIPT_NYIAKENG_PUACHUE_HMONG: hr_script_t = 0x486D_6E70u32;
/// The Wancho script (`Wcho`).
pub const HR_SCRIPT_WANCHO: hr_script_t = 0x5763_686Fu32;
/// The Chorasmian script (`Chrs`).
pub const HR_SCRIPT_CHORASMIAN: hr_script_t = 0x4368_7273u32;
/// The Dives Akuru script (`Diak`).
pub const HR_SCRIPT_DIVES_AKURU: hr_script_t = 0x4469_616Bu32;
/// The Khitan Small Script script (`Kits`).
pub const HR_SCRIPT_KHITAN_SMALL_SCRIPT: hr_script_t = 0x4B69_7473u32;
/// The Yezidi script (`Yezi`).
pub const HR_SCRIPT_YEZIDI: hr_script_t = 0x5965_7A69u32;
/// The Cypro Minoan script (`Cpmn`).
pub const HR_SCRIPT_CYPRO_MINOAN: hr_script_t = 0x4370_6D6Eu32;
/// The Old Uyghur script (`Ougr`).
pub const HR_SCRIPT_OLD_UYGHUR: hr_script_t = 0x4F75_6772u32;
/// The Tangsa script (`Tnsa`).
pub const HR_SCRIPT_TANGSA: hr_script_t = 0x546E_7361u32;
/// The Toto script (`Toto`).
pub const HR_SCRIPT_TOTO: hr_script_t = 0x546F_746Fu32;
/// The Vithkuqi script (`Vith`).
pub const HR_SCRIPT_VITHKUQI: hr_script_t = 0x5669_7468u32;
/// The Kawi script (`Kawi`).
pub const HR_SCRIPT_KAWI: hr_script_t = 0x4B61_7769u32;
/// The Nag Mundari script (`Nagm`).
pub const HR_SCRIPT_NAG_MUNDARI: hr_script_t = 0x4E61_676Du32;
/// The Garay script (`Gara`).
pub const HR_SCRIPT_GARAY: hr_script_t = 0x4761_7261u32;
/// The Gurung Khema script (`Gukh`).
pub const HR_SCRIPT_GURUNG_KHEMA: hr_script_t = 0x4775_6B68u32;
/// The Kirat Rai script (`Krai`).
pub const HR_SCRIPT_KIRAT_RAI: hr_script_t = 0x4B72_6169u32;
/// The Ol Onal script (`Onao`).
pub const HR_SCRIPT_OL_ONAL: hr_script_t = 0x4F6E_616Fu32;
/// The Sunuwar script (`Sunu`).
pub const HR_SCRIPT_SUNUWAR: hr_script_t = 0x5375_6E75u32;
/// The Todhri script (`Todr`).
pub const HR_SCRIPT_TODHRI: hr_script_t = 0x546F_6472u32;
/// The Tulu Tigalari script (`Tutg`).
pub const HR_SCRIPT_TULU_TIGALARI: hr_script_t = 0x5475_7467u32;
/// The Beria Erfe script (`Berf`).
pub const HR_SCRIPT_BERIA_ERFE: hr_script_t = 0x4265_7266u32;
/// The Sidetic script (`Sidt`).
pub const HR_SCRIPT_SIDETIC: hr_script_t = 0x5369_6474u32;
/// The Tai Yo script (`Tayo`).
pub const HR_SCRIPT_TAI_YO: hr_script_t = 0x5461_796Fu32;
/// The Tolong Siki script (`Tols`).
pub const HR_SCRIPT_TOLONG_SIKI: hr_script_t = 0x546F_6C73u32;
/// The Jurchen script (`Jurc`).
pub const HR_SCRIPT_JURCHEN: hr_script_t = 0x4A75_7263u32;
/// The Proto-Cuneiform script (`Pcun`).
pub const HR_SCRIPT_PROTO_CUNEIFORM: hr_script_t = 0x5063_756Eu32;
/// The Seal script (`Seal`).
pub const HR_SCRIPT_SEAL: hr_script_t = 0x5365_616Cu32;
/// The Math script (`Zmth`).
pub const HR_SCRIPT_MATH: hr_script_t = 0x5A6D_7468u32;
/// The Myanmar Zawgyi script (`Qaag`).
pub const HR_SCRIPT_MYANMAR_ZAWGYI: hr_script_t = 0x5161_6167u32;

pub(crate) fn script_to_rust(script: hr_script_t) -> Option<Script> {
    Script::from_iso15924_tag(tag_to_rust(script))
}

pub(crate) fn script_from_rust(script: Script) -> hr_script_t {
    tag_from_rust(script.tag())
}

/// Converts an ISO 15924 tag into a script.
#[no_mangle]
pub extern "C" fn hr_script_from_iso15924_tag(tag: hr_tag_t) -> hr_script_t {
    script_to_rust(tag).map_or(HR_SCRIPT_INVALID, script_from_rust)
}

/// Parses a script from an ISO 15924 tag written as a string.
///
/// # Safety
///
/// See [`str_from_raw`].
#[no_mangle]
pub unsafe extern "C" fn hr_script_from_string(str_: *const c_char, len: c_int) -> hr_script_t {
    // The name as a tag, and then the script that tag names: the two steps
    // HarfBuzz takes, so that a name which is no tag is no script either
    // rather than the `Zzzz` standing for a script known to be unknown.
    hr_script_from_iso15924_tag(unsafe { hr_tag_from_string(str_, len) })
}

/// Returns a script's ISO 15924 tag.
#[no_mangle]
pub extern "C" fn hr_script_to_iso15924_tag(script: hr_script_t) -> hr_tag_t {
    script
}

/// Returns the direction text in this script is usually set in.
#[no_mangle]
pub extern "C" fn hr_script_get_horizontal_direction(script: hr_script_t) -> hr_direction_t {
    // Text runs left to right unless its script is one of the ones that does
    // not, which is the answer HarfBuzz gives for a script it has never heard
    // of, and for one that is not a script at all.
    let Some(script) = script_to_rust(script) else {
        return HR_DIRECTION_LTR;
    };
    // `guess_segment_properties` derives the direction from the script, which
    // is exactly what this needs and keeps the two in step.
    let mut buffer = harfrust::Buffer::new();
    buffer.set_script(Some(script));
    buffer.guess_segment_properties();
    let direction = direction_from_rust(buffer.direction());
    if direction == HR_DIRECTION_INVALID {
        return HR_DIRECTION_LTR;
    }
    direction
}

/// An interned language tag.
///
/// Language values are interned for the lifetime of the process, so they may
/// be compared by pointer and never need to be freed.
pub struct hr_language_impl_t {
    lang: Language,
    /// NUL-terminated name, so it can be handed straight back to C.
    name: Box<[u8]>,
}

/// A language, as an interned pointer. `NULL` means "unset".
pub type hr_language_t = *const hr_language_impl_t;

/// Interned languages, which live for the life of the process, by their bytes.
///
/// Content decides how many there are, so finding one must not mean walking
/// them all. Looking one up is far more common than adding one, and adding is
/// what the write lock is for.
static LANGUAGES: RwLock<BTreeMap<&'static [u8], &'static hr_language_impl_t>> =
    RwLock::new(BTreeMap::new());

fn intern_language(lang: &Language) -> hr_language_t {
    // The common path: already interned, so only a read lock is needed.
    if let Ok(languages) = LANGUAGES.read() {
        if let Some(found) = languages.get(lang.as_bytes()) {
            return *found as hr_language_t;
        }
    }

    let Ok(mut languages) = LANGUAGES.write() else {
        return core::ptr::null();
    };
    // Another thread may have interned it between the two locks.
    if let Some(found) = languages.get(lang.as_bytes()) {
        return *found as hr_language_t;
    }
    let mut name = lang.as_bytes().to_vec();
    name.push(0);
    let entry: &'static hr_language_impl_t = Box::leak(Box::new(hr_language_impl_t {
        lang: lang.clone(),
        name: name.into_boxed_slice(),
    }));
    // The key borrows from the leaked entry, which is never freed or moved.
    languages.insert(entry.lang.as_bytes(), entry);
    entry as hr_language_t
}

/// # Safety
///
/// `language` must be `NULL` or a value returned by `hr_language_from_string`.
pub(crate) unsafe fn language_to_rust(language: hr_language_t) -> Option<Language> {
    unsafe { language.as_ref() }.map(|entry| entry.lang.clone())
}

/// # Safety
/// `language` must be `NULL` or an interned language returned by this API.
pub(crate) unsafe fn language_ref(language: hr_language_t) -> Option<&'static Language> {
    unsafe { language.as_ref() }.map(|entry| &entry.lang)
}

pub(crate) fn language_from_owned(language: Option<Language>) -> hr_language_t {
    language_from_rust(language.as_ref())
}

pub(crate) fn language_from_rust(language: Option<&Language>) -> hr_language_t {
    language.map_or(core::ptr::null(), intern_language)
}

/// Interns a language from a BCP 47 tag.
///
/// The returned value lives for the lifetime of the process and must not be
/// freed.
///
/// # Safety
///
/// See [`str_from_raw`].
#[no_mangle]
pub unsafe extern "C" fn hr_language_from_string(str_: *const c_char, len: c_int) -> hr_language_t {
    let Some(s) = (unsafe { str_from_raw(str_, len) }) else {
        return core::ptr::null();
    };
    Language::new(canonical_language(s))
        .as_ref()
        .map_or(core::ptr::null(), intern_language)
}

/// A language tag as HarfBuzz keeps it: lower-cased, with underscores read as
/// subtag separators, and ending at the first character that can be neither.
///
/// This is what drops the codeset and modifier a locale carries, so that
/// "en_US.utf8" and "en-us" are the same language.
fn canonical_language(tag: &str) -> String {
    let mut out = String::with_capacity(tag.len());
    for byte in tag.bytes() {
        match byte {
            b'a'..=b'z' | b'0'..=b'9' => out.push(byte as char),
            b'A'..=b'Z' => out.push(byte.to_ascii_lowercase() as char),
            b'-' | b'_' => out.push('-'),
            _ => break,
        }
    }
    out
}

/// Returns a language's tag as a NUL-terminated string, or `NULL` when the
/// language is unset.
/// # Safety
///
/// `language` must be `NULL` or a value returned by `hr_language_from_string`.
#[no_mangle]
pub unsafe extern "C" fn hr_language_to_string(language: hr_language_t) -> *const c_char {
    unsafe { language.as_ref() }.map_or(core::ptr::null(), |entry| {
        entry.name.as_ptr().cast::<c_char>()
    })
}

/// Returns the process's default language, taken from the environment.
#[no_mangle]
pub extern "C" fn hr_language_get_default() -> hr_language_t {
    intern_language(&default_language())
}

/// The language the process is running under, worked out once.
pub(crate) fn default_language() -> Language {
    // Worked out once and never changed, so this wants a `OnceLock` rather
    // than a lock that is taken on every call.
    static DEFAULT: OnceLock<Language> = OnceLock::new();
    let language = DEFAULT.get_or_init(|| {
        let from_env = ["LC_ALL", "LC_CTYPE", "LANG"]
            .into_iter()
            .find_map(|key| std::env::var(key).ok())
            .and_then(|value| {
                // Trim the codeset and modifier: "en_US.UTF-8" -> "en_US".
                let tag = value.split(['.', '@']).next().unwrap_or_default();
                Language::new(tag)
            });
        from_env
            .or_else(|| Language::new("x-hbot"))
            .unwrap_or_else(|| Language::new("und").expect("a valid language tag"))
    });
    language.clone()
}

/// Returns whether `language` is the same as, or a more specific form of,
/// `specific`.
/// # Safety
///
/// Both arguments must be `NULL` or values returned by
/// `hr_language_from_string`.
#[no_mangle]
pub unsafe extern "C" fn hr_language_matches(
    language: hr_language_t,
    specific: hr_language_t,
) -> hr_bool_t {
    if language == specific {
        return true.into();
    }
    let (Some(language), Some(specific)) =
        (unsafe { language.as_ref() }, unsafe { specific.as_ref() })
    else {
        return false.into();
    };
    let (lang, spec) = (language.lang.as_bytes(), specific.lang.as_bytes());
    if lang.is_empty() {
        return true.into();
    }
    // `specific` is the more specific of the two: it matches when it is
    // `language` with more subtags on the end, which is the direction
    // HarfBuzz reads these in.
    (spec.len() > lang.len() && spec.starts_with(lang) && spec[lang.len()] == b'-').into()
}

/// A feature tag with the value to apply and the range to apply it over.
#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, Hash, Debug)]
pub struct hr_feature_t {
    /// The feature's OpenType tag.
    pub tag: hr_tag_t,
    /// The value to apply. Zero disables the feature; one enables it.
    pub value: u32,
    /// Index of the first item the feature applies to.
    pub start: c_uint,
    /// Index one past the last item the feature applies to.
    pub end: c_uint,
}

impl From<hr_feature_t> for Feature {
    fn from(value: hr_feature_t) -> Self {
        Feature {
            tag: tag_to_rust(value.tag),
            value: value.value,
            start: value.start,
            end: value.end,
        }
    }
}

impl From<Feature> for hr_feature_t {
    fn from(value: Feature) -> Self {
        hr_feature_t {
            tag: tag_from_rust(value.tag),
            value: value.value,
            start: value.start,
            end: value.end,
        }
    }
}

/// Parses a feature from its string form, such as `kern`, `-liga` or
/// `aalt[3:5]=2`.
///
/// Returns false, leaving `feature` untouched, if the string does not parse.
///
/// # Safety
///
/// See [`str_from_raw`]. `feature` must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_feature_from_string(
    str_: *const c_char,
    len: c_int,
    feature: *mut hr_feature_t,
) -> hr_bool_t {
    // Nothing parsed is nothing described: the destination says so rather
    // than keeping whatever the caller happened to leave in it.
    if let Some(out) = unsafe { feature.as_mut() } {
        *out = hr_feature_t::default();
    }
    let Some(s) = (unsafe { str_from_raw(str_, len) }) else {
        return false.into();
    };
    let Ok(parsed) = Feature::from_str(s) else {
        return false.into();
    };
    if let Some(out) = unsafe { feature.as_mut() } {
        *out = parsed.into();
    }
    true.into()
}

/// Writes a feature's string form into `buf`, always NUL-terminating it.
///
/// # Safety
///
/// `feature` must be `NULL` or readable, and `buf` must point to `size`
/// writable bytes.
#[no_mangle]
pub unsafe extern "C" fn hr_feature_to_string(
    feature: *const hr_feature_t,
    buf: *mut c_char,
    size: c_uint,
) {
    let Some(feature) = (unsafe { feature.as_ref() }) else {
        return;
    };
    let mut s = String::with_capacity(32);
    if feature.value == 0 {
        s.push('-');
    }
    s.push_str(tag_to_rust(feature.tag).to_string().trim_end());
    if feature.start != HR_FEATURE_GLOBAL_START || feature.end != HR_FEATURE_GLOBAL_END {
        s.push('[');
        if feature.start != HR_FEATURE_GLOBAL_START {
            s.push_str(&feature.start.to_string());
        }
        // A range covering one character is written as that one character,
        // which is also how it reads back.
        if feature.end != feature.start.saturating_add(1) {
            s.push(':');
            if feature.end != HR_FEATURE_GLOBAL_END {
                s.push_str(&feature.end.to_string());
            }
        }
        s.push(']');
    }
    if feature.value > 1 {
        s.push('=');
        s.push_str(&feature.value.to_string());
    }
    unsafe { write_c_string(&s, buf, size) };
}

/// A variation axis tag and the value to set it to, in user space.
#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Debug)]
pub struct hr_variation_t {
    /// The axis's OpenType tag.
    pub tag: hr_tag_t,
    /// The value to set the axis to, in user space.
    pub value: f32,
}

impl From<hr_variation_t> for Variation {
    fn from(value: hr_variation_t) -> Self {
        Variation {
            tag: tag_to_rust(value.tag),
            value: value.value,
        }
    }
}

impl From<Variation> for hr_variation_t {
    fn from(value: Variation) -> Self {
        hr_variation_t {
            tag: tag_from_rust(value.tag),
            value: value.value,
        }
    }
}

/// Parses a variation from its string form, such as `wght=700`.
///
/// Returns false, leaving `variation` untouched, if the string does not parse.
///
/// # Safety
///
/// See [`str_from_raw`]. `variation` must be `NULL` or writable.
#[no_mangle]
pub unsafe extern "C" fn hr_variation_from_string(
    str_: *const c_char,
    len: c_int,
    variation: *mut hr_variation_t,
) -> hr_bool_t {
    // Nothing parsed is nothing described: the destination says so rather
    // than keeping whatever the caller happened to leave in it.
    if let Some(out) = unsafe { variation.as_mut() } {
        *out = hr_variation_t::default();
    }
    let Some(s) = (unsafe { str_from_raw(str_, len) }) else {
        return false.into();
    };
    let Ok(parsed) = Variation::parse_setting(s) else {
        return false.into();
    };
    if let Some(out) = unsafe { variation.as_mut() } {
        *out = parsed.into();
    }
    true.into()
}

/// Writes a variation's string form into `buf`, always NUL-terminating it.
///
/// # Safety
///
/// `variation` must be `NULL` or readable, and `buf` must point to `size`
/// writable bytes.
#[no_mangle]
pub unsafe extern "C" fn hr_variation_to_string(
    variation: *const hr_variation_t,
    buf: *mut c_char,
    size: c_uint,
) {
    let Some(variation) = (unsafe { variation.as_ref() }) else {
        return;
    };
    let s = format!(
        "{}={}",
        tag_to_rust(variation.tag).to_string().trim_end(),
        variation.value
    );
    unsafe { write_c_string(&s, buf, size) };
}

/// The ink extents of a glyph, in the font's scaled units.
#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, Hash, Debug)]
pub struct hr_glyph_extents_t {
    /// Horizontal bearing from the glyph origin to the left of the ink box.
    pub x_bearing: hr_position_t,
    /// Vertical bearing from the glyph origin to the top of the ink box.
    pub y_bearing: hr_position_t,
    /// Width of the ink box.
    pub width: hr_position_t,
    /// Height of the ink box, measured downwards.
    pub height: hr_position_t,
}

/// Copies `s` into `buf`, truncating to `size` bytes and always writing a
/// terminating NUL.
///
/// # Safety
///
/// `buf` must point to `size` writable bytes.
pub(crate) unsafe fn write_c_string(s: &str, buf: *mut c_char, size: c_uint) {
    if buf.is_null() || size == 0 {
        return;
    }
    let capacity = size as usize - 1;
    // Truncate on a UTF-8 boundary so we never split a character.
    let mut len = s.len().min(capacity);
    while len > 0 && !s.is_char_boundary(len) {
        len -= 1;
    }
    unsafe {
        core::ptr::copy_nonoverlapping(s.as_ptr().cast::<c_char>(), buf, len);
        buf.add(len).write(0);
    }
}
