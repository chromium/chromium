pub(crate) mod arabic;
pub(crate) mod arabic_fallback;
pub(crate) mod arabic_pua;
#[rustfmt::skip]
pub(crate) mod arabic_table;
pub(crate) mod hangul;
pub(crate) mod hebrew;
pub(crate) mod indic;
pub(crate) mod indic_machine;
#[rustfmt::skip]
pub(crate) mod indic_table;
pub(crate) mod khmer;
pub(crate) mod khmer_machine;
mod machine_cursor;
pub(crate) mod myanmar;
pub(crate) mod myanmar_machine;
pub(crate) mod syllabic;
pub(crate) mod thai;
pub(crate) mod use_;
pub(crate) mod use_machine;
#[rustfmt::skip]
pub(crate) mod use_table;
#[allow(clippy::collapsible_match)]
pub(crate) mod vowel_constraints;

use crate::buffer::*;
use crate::normalize::*;
use crate::plan::ShapePlan;
use crate::tag::TagExt;
use crate::unicode::Codepoint;
use crate::ShaperFont;
use crate::*;
use crate::{Direction, Script, Tag};
use alloc::boxed::Box;
use core::any::Any;

impl GlyphInfo {
    declare_buffer_var!(
        u8,
        2,
        2,
        OT_SHAPER_VAR_U8_CATEGORY_VAR,
        ot_shaper_var_u8_category,
        set_ot_shaper_var_u8_category
    );
    declare_buffer_var!(
        u8,
        2,
        3,
        OT_SHAPER_VAR_U8_AUXILIARY_VAR,
        ot_shaper_var_u8_auxiliary,
        set_ot_shaper_var_u8_auxiliary
    );
}

pub const MAX_COMBINING_MARKS: usize = 32;

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum ZeroWidthMarks {
    None,
    ByGdefEarly,
    ByGdefLate,
}

pub type DecomposeFn = fn(&NormalizeContext, Codepoint) -> Option<(Codepoint, Codepoint)>;
pub type ComposeFn = fn(&NormalizeContext, Codepoint, Codepoint) -> Option<Codepoint>;

pub const DEFAULT_SHAPER: OtShaper = OtShaper {
    collect_features: None,
    override_features: None,
    create_data: None,
    preprocess_text: None,
    postprocess_glyphs: None,
    normalization_preference: NormalizationMode::Auto,
    decompose: None,
    compose: None,
    setup_masks: None,
    gpos_tag: None,
    reorder_marks: None,
    zero_width_marks: ZeroWidthMarks::ByGdefLate,
    fallback_position: true,
};

pub struct OtShaper {
    /// Called during `shape_plan()`.
    /// Shapers should use plan.map to add their features and callbacks.
    pub collect_features: Option<fn(&mut ShapePlanner)>,

    /// Called during `shape_plan()`.
    /// Shapers should use plan.map to override features and add callbacks after
    /// common features are added.
    pub override_features: Option<fn(&mut ShapePlanner)>,

    /// Called at the end of `shape_plan()`.
    /// Whatever shapers return will be accessible through `plan.data()` later.
    pub create_data: Option<fn(&ShapePlan) -> Box<dyn Any + Send + Sync>>,

    /// Called during `shape()`.
    /// Shapers can use to modify text before shaping starts.
    pub preprocess_text: Option<fn(&ShapePlan, &ShaperFont<'_, '_>, &mut Buffer)>,

    /// Called during `shape()`.
    /// Shapers can use to modify text before shaping starts.
    pub postprocess_glyphs: Option<fn(&ShapePlan, &ShaperFont<'_, '_>, &mut Buffer)>,

    /// How to normalize.
    pub normalization_preference: NormalizationMode,

    /// Called during `shape()`'s normalization.
    pub decompose: Option<DecomposeFn>,

    /// Called during `shape()`'s normalization.
    pub compose: Option<ComposeFn>,

    /// Called during `shape()`.
    /// Shapers should use map to get feature masks and set on buffer.
    /// Shapers may NOT modify characters.
    pub setup_masks: Option<fn(&ShapePlan, &ShaperFont<'_, '_>, &mut Buffer)>,

    /// If not `None`, then must match found GPOS script tag for
    /// GPOS to be applied.  Otherwise, fallback positioning will be used.
    pub gpos_tag: Option<Tag>,

    /// Called during `shape()`.
    /// Shapers can use to modify ordering of combining marks.
    pub reorder_marks: Option<fn(&ShapePlan, &mut Buffer, usize, usize)>,

    /// If and when to zero-width marks.
    pub zero_width_marks: ZeroWidthMarks,

    /// Whether to use fallback mark positioning.
    pub fallback_position: bool,
}

// Same as default but no mark advance zeroing / fallback positioning.
// Dumbest shaper ever, basically.
pub const DUMBER_SHAPER: OtShaper = OtShaper {
    collect_features: None,
    override_features: None,
    create_data: None,
    preprocess_text: None,
    postprocess_glyphs: None,
    normalization_preference: NormalizationMode::Auto,
    decompose: None,
    compose: None,
    setup_masks: None,
    gpos_tag: None,
    reorder_marks: None,
    zero_width_marks: ZeroWidthMarks::None,
    fallback_position: false,
};

pub fn categorize(
    script: Script,
    direction: Direction,
    gsub_script: Option<Tag>,
) -> &'static OtShaper {
    match script {
        // Unicode-1.1 additions
        Script::ARABIC

        // Unicode-3.0 additions
        | Script::SYRIAC => {
            // For Arabic script, use the Arabic shaper even if no OT script tag was found.
            // This is because we do fallback shaping for Arabic script (and not others).
            // But note that Arabic shaping is applicable only to horizontal layout; for
            // vertical text, just use the generic shaper instead.
            //
            // TODO: Does this still apply? Arabic fallback shaping was removed.
            if (gsub_script != Some(Tag::default_script()) || script == Script::ARABIC)
                && direction.is_horizontal()
            {
                &arabic::ARABIC_SHAPER
            } else {
                &DEFAULT_SHAPER
            }
        }

        // Unicode-1.1 additions
        Script::THAI
        | Script::LAO => &thai::THAI_SHAPER,

        // Unicode-1.1 additions
        Script::HANGUL => &hangul::HANGUL_SHAPER,

        // Unicode-1.1 additions
        Script::HEBREW => &hebrew::HEBREW_SHAPER,

        // Unicode-1.1 additions
        Script::BENGALI
        | Script::DEVANAGARI
        | Script::GUJARATI
        | Script::GURMUKHI
        | Script::KANNADA
        | Script::MALAYALAM
        | Script::ORIYA
        | Script::TAMIL
        | Script::TELUGU => {
            // If the designer designed the font for the 'DFLT' script,
            // (or we ended up arbitrarily pick 'latn'), use the default shaper.
            // Otherwise, use the specific shaper.
            //
            // If it's indy3 tag, send to USE.
            if gsub_script == Some(Tag::default_script()) ||
               gsub_script == Some(Tag::new(b"latn")) {
                &DEFAULT_SHAPER
            } else if gsub_script.is_some_and(|tag| tag.to_be_bytes()[3] == b'3') {
                &use_::UNIVERSAL_SHAPER
            } else {
                &indic::INDIC_SHAPER
            }
        }

        Script::KHMER => &khmer::KHMER_SHAPER,

        Script::MYANMAR => {
            // If the designer designed the font for the 'DFLT' script,
            // (or we ended up arbitrarily pick 'latn'), use the default shaper.
            // Otherwise, use the specific shaper.
            //
            // If designer designed for 'mymr' tag, also send to default
            // shaper.  That's tag used from before Myanmar shaping spec
            // was developed.  The shaping spec uses 'mym2' tag.
            if gsub_script == Some(Tag::default_script()) ||
               gsub_script == Some(Tag::new(b"latn")) ||
               gsub_script == Some(Tag::new(b"mymr"))
            {
                &DEFAULT_SHAPER
            } else {
                &myanmar::MYANMAR_SHAPER
            }
        }

        // https://github.com/harfbuzz/harfbuzz/issues/1162
        Script::MYANMAR_ZAWGYI => &myanmar::MYANMAR_ZAWGYI_SHAPER,

        // Unicode-2.0 additions
        Script::TIBETAN

        // Unicode-3.0 additions
        | Script::MONGOLIAN
        | Script::SINHALA

        // Unicode-3.2 additions
        | Script::BUHID
        | Script::HANUNOO
        | Script::TAGALOG
        | Script::TAGBANWA

        // Unicode-4.0 additions
        | Script::LIMBU
        | Script::TAI_LE

        // Unicode-4.1 additions
        | Script::BUGINESE
        | Script::KHAROSHTHI
        | Script::SYLOTI_NAGRI
        | Script::TIFINAGH

        // Unicode-5.0 additions
        | Script::BALINESE
        | Script::NKO
        | Script::PHAGS_PA

        // Unicode-5.1 additions
        | Script::CHAM
        | Script::KAYAH_LI
        | Script::LEPCHA
        | Script::REJANG
        | Script::SAURASHTRA
        | Script::SUNDANESE

        // Unicode-5.2 additions
        | Script::EGYPTIAN_HIEROGLYPHS
        | Script::JAVANESE
        | Script::KAITHI
        | Script::MEETEI_MAYEK
        | Script::TAI_THAM
        | Script::TAI_VIET

        // Unicode-6.0 additions
        | Script::BATAK
        | Script::BRAHMI
        | Script::MANDAIC

        // Unicode-6.1 additions
        | Script::CHAKMA
        | Script::MIAO
        | Script::SHARADA
        | Script::TAKRI

        // Unicode-7.0 additions
        | Script::DUPLOYAN
        | Script::GRANTHA
        | Script::KHOJKI
        | Script::KHUDAWADI
        | Script::MAHAJANI
        | Script::MANICHAEAN
        | Script::MODI
        | Script::PAHAWH_HMONG
        | Script::PSALTER_PAHLAVI
        | Script::SIDDHAM
        | Script::TIRHUTA

        // Unicode-8.0 additions
        | Script::AHOM
        | Script::MULTANI

        // Unicode-9.0 additions
        | Script::ADLAM
        | Script::BHAIKSUKI
        | Script::MARCHEN
        | Script::NEWA

        // Unicode-10.0 additions
        | Script::MASARAM_GONDI
        | Script::SOYOMBO
        | Script::ZANABAZAR_SQUARE

        // Unicode-11.0 additions
        | Script::DOGRA
        | Script::GUNJALA_GONDI
        | Script::HANIFI_ROHINGYA
        | Script::MAKASAR
        | Script::MEDEFAIDRIN
        | Script::OLD_SOGDIAN
        | Script::SOGDIAN

        // Unicode-12.0 additions
        | Script::ELYMAIC
        | Script::NANDINAGARI
        | Script::NYIAKENG_PUACHUE_HMONG
        | Script::WANCHO

        // Unicode-13.0 additions
        | Script::CHORASMIAN
        | Script::DIVES_AKURU
        | Script::KHITAN_SMALL_SCRIPT
        | Script::YEZIDI

        // Unicode-14.0 additions
        | Script::CYPRO_MINOAN
        | Script::OLD_UYGHUR
        | Script::TANGSA
        | Script::TOTO
        | Script::VITHKUQI

        // Unicode-15.0 additions
        | Script::KAWI
        | Script::NAG_MUNDARI

        // Unicode-16.0 additions
        | Script::GARAY
        | Script::GURUNG_KHEMA
        | Script::KIRAT_RAI
        | Script::OL_ONAL
        | Script::SUNUWAR
        | Script::TODHRI
        | Script::TULU_TIGALARI

        // Unicode-17.0 additions
        | Script::BERIA_ERFE
        | Script::SIDETIC
        | Script::TAI_YO
        | Script::TOLONG_SIKI

        // Unicode-18.0 additions
        | Script::JURCHEN
        | Script::PROTO_CUNEIFORM
        | Script::SEAL

        => {
            // If the designer designed the font for the 'DFLT' script,
            // (or we ended up arbitrarily pick 'latn'), use the default shaper.
            // Otherwise, use the specific shaper.
            // Note that for some simple scripts, there may not be *any*
            // GSUB/GPOS needed, so there may be no scripts found!
            if gsub_script == Some(Tag::default_script()) ||
               gsub_script == Some(Tag::new(b"latn")) {
                &DEFAULT_SHAPER
            } else {
                &use_::UNIVERSAL_SHAPER
            }
        }

        _ => &DEFAULT_SHAPER
    }
}
