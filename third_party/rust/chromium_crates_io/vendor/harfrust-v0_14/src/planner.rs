use core::ptr;

use crate::aat::map::*;
use crate::ot::layout::*;
use crate::ot::map::*;
use crate::ot::shaper::*;
use crate::shaper_font::LayoutData;
use crate::{Direction, Feature, Language, Script, Tag};

use super::plan::ShapePlan;

pub struct ShapePlanner<'a> {
    pub layout: LayoutData<'a>,
    pub direction: Direction,
    pub script: Option<Script>,
    pub language: Option<Language>,
    pub ot_map: OtMapBuilder<'a>,
    pub aat_map: AatMapBuilder,
    pub apply_morx: bool,
    pub script_zero_marks: bool,
    pub script_fallback_position: bool,
    pub shaper: &'static OtShaper,
}

impl<'a> ShapePlanner<'a> {
    pub fn new(
        layout: LayoutData<'a>,
        direction: Direction,
        script: Option<Script>,
        language: Option<&Language>,
    ) -> Self {
        let ot_map = OtMapBuilder::new(layout, script, language);
        let aat_map = AatMapBuilder::new(language);

        let mut shaper = match script {
            Some(script) => categorize(
                script,
                direction,
                ot_map.chosen_script(LayoutTableKind::Gsub),
            ),
            None => &DEFAULT_SHAPER,
        };

        let script_zero_marks = shaper.zero_width_marks != ZeroWidthMarks::None;
        let script_fallback_position = shaper.fallback_position;

        // https://github.com/harfbuzz/harfbuzz/issues/2124
        let apply_morx = (layout.aat.morx.is_some() || layout.aat.mort.is_some())
            && (direction.is_horizontal() || layout.ot.gsub.is_none());

        // https://github.com/harfbuzz/harfbuzz/issues/1528
        if apply_morx && !ptr::eq(ptr::from_ref(shaper), ptr::from_ref(&DEFAULT_SHAPER)) {
            shaper = &DUMBER_SHAPER;
        }

        ShapePlanner {
            layout,
            direction,
            script,
            language: language.cloned(),
            ot_map,
            aat_map,
            apply_morx,
            script_zero_marks,
            script_fallback_position,
            shaper,
        }
    }

    pub fn collect_features(&mut self, user_features: &[Feature]) {
        static COMMON_FEATURES: &[(Tag, MapFeatureFlags)] = &[
            (Tag::new(b"abvm"), F_GLOBAL),
            (Tag::new(b"blwm"), F_GLOBAL),
            (Tag::new(b"ccmp"), F_GLOBAL),
            (Tag::new(b"locl"), F_GLOBAL),
            (Tag::new(b"mark"), F_GLOBAL_MANUAL_JOINERS),
            (Tag::new(b"mkmk"), F_GLOBAL_MANUAL_JOINERS),
            (Tag::new(b"rlig"), F_GLOBAL),
        ];

        static HORIZONTAL_FEATURES: &[(Tag, MapFeatureFlags)] = &[
            (Tag::new(b"calt"), F_GLOBAL),
            (Tag::new(b"clig"), F_GLOBAL),
            (Tag::new(b"curs"), F_GLOBAL),
            (Tag::new(b"dist"), F_GLOBAL),
            (Tag::new(b"kern"), F_GLOBAL_HAS_FALLBACK),
            (Tag::new(b"liga"), F_GLOBAL),
            (Tag::new(b"rclt"), F_GLOBAL),
        ];

        let empty = F_NONE;
        self.ot_map.is_simple = true;

        self.ot_map.enable_feature(Tag::new(b"rvrn"), empty, 1);
        self.ot_map.add_gsub_pause(None);

        match self.direction {
            Direction::LeftToRight => {
                self.ot_map.enable_feature(Tag::new(b"ltra"), empty, 1);
                self.ot_map.enable_feature(Tag::new(b"ltrm"), empty, 1);
            }
            Direction::RightToLeft => {
                self.ot_map.enable_feature(Tag::new(b"rtla"), empty, 1);
                self.ot_map.add_feature(Tag::new(b"rtlm"), empty, 1);
            }
            _ => {}
        }

        // Automatic fractions.
        self.ot_map.add_feature(Tag::new(b"frac"), empty, 1);
        self.ot_map.add_feature(Tag::new(b"numr"), empty, 1);
        self.ot_map.add_feature(Tag::new(b"dnom"), empty, 1);

        // Random!
        self.ot_map
            .enable_feature(Tag::new(b"rand"), F_RANDOM, OtMap::MAX_VALUE);

        // Tracking.  We enable dummy feature here just to allow disabling
        // AAT 'trak' table using features.
        // https://github.com/harfbuzz/harfbuzz/issues/1303
        self.ot_map
            .enable_feature(Tag::new(b"trak"), F_HAS_FALLBACK, 1);

        self.ot_map.enable_feature(Tag::new(b"Harf"), empty, 1); // Considered required.
        self.ot_map.enable_feature(Tag::new(b"HARF"), empty, 1); // Considered discretionary.

        if let Some(func) = self.shaper.collect_features {
            self.ot_map.is_simple = false;
            func(self);
        }

        self.ot_map.enable_feature(Tag::new(b"Buzz"), empty, 1); // Considered required.
        self.ot_map.enable_feature(Tag::new(b"BUZZ"), empty, 1); // Considered discretionary.

        for &(tag, flags) in COMMON_FEATURES {
            self.ot_map.add_feature(tag, flags, 1);
        }

        if self.direction.is_horizontal() {
            for &(tag, flags) in HORIZONTAL_FEATURES {
                self.ot_map.add_feature(tag, flags, 1);
            }
        } else {
            // We only apply `vert` feature. See:
            // https://github.com/harfbuzz/harfbuzz/commit/d71c0df2d17f4590d5611239577a6cb532c26528
            // https://lists.freedesktop.org/archives/harfbuzz/2013-August/003490.html

            // We really want to find a 'vert' feature if there's any in the font, no
            // matter which script/langsys it is listed (or not) under.
            // See various bugs referenced from:
            // https://github.com/harfbuzz/harfbuzz/issues/63
            self.ot_map
                .enable_feature(Tag::new(b"vert"), F_GLOBAL_SEARCH, 1);
        }

        if !user_features.is_empty() {
            self.ot_map.is_simple = false;
        }

        for feature in user_features {
            let flags = if feature.is_global() { F_GLOBAL } else { empty };
            self.ot_map.add_feature(feature.tag, flags, feature.value);
        }

        if let Some(func) = self.shaper.override_features {
            func(self);
        }
    }

    pub fn compile(mut self, features: &[Feature]) -> ShapePlan {
        let ot_map = self.ot_map.compile();
        let mut aat_map = AatMap::default();
        if self.apply_morx {
            self.aat_map.compile(self.layout.aat, &mut aat_map);
        }

        let frac_mask = ot_map.get_1_mask(Tag::new(b"frac"));
        let numr_mask = ot_map.get_1_mask(Tag::new(b"numr"));
        let dnom_mask = ot_map.get_1_mask(Tag::new(b"dnom"));
        let has_frac = frac_mask != 0 || (numr_mask != 0 && dnom_mask != 0);

        let rtlm_mask = ot_map.get_1_mask(Tag::new(b"rtlm"));
        let has_vert = ot_map.get_1_mask(Tag::new(b"vert")) != 0;

        let horizontal = self.direction.is_horizontal();
        let kern_tag = if horizontal {
            Tag::new(b"kern")
        } else {
            Tag::new(b"vkrn")
        };
        let kern_mask = ot_map.get_mask(kern_tag).0;
        let requested_kerning = kern_mask != 0;

        let has_gpos_kern = ot_map
            .get_feature_index(LayoutTableKind::Gpos, kern_tag)
            .is_some();
        let disable_gpos = self.shaper.gpos_tag.is_some()
            && self.shaper.gpos_tag != ot_map.chosen_script(LayoutTableKind::Gpos);

        // Decide who provides glyph classes. GDEF or Unicode.
        let fallback_glyph_classes = !has_glyph_classes(self.layout.ot);

        // Decide who does substitutions. GSUB, morx, or fallback.
        let apply_morx = self.apply_morx;

        let mut apply_gpos = false;
        let mut apply_kerx = false;
        let mut apply_kern = false;

        // Decide who does positioning. GPOS, kerx, kern, or fallback.
        let has_kerx = self.layout.aat.kerx.is_some();
        let has_gsub = !apply_morx && self.layout.ot.gsub.is_some();
        let has_gpos = !disable_gpos && self.layout.ot.gpos.is_some();

        // Prefer GPOS over kerx if GSUB is present;
        // https://github.com/harfbuzz/harfbuzz/issues/3008
        if has_kerx && !(has_gsub && has_gpos) {
            apply_kerx = true;
        } else if has_gpos {
            apply_gpos = true;
        }

        if !apply_kerx && (!has_gpos_kern || !apply_gpos) {
            if has_kerx {
                apply_kerx = true;
            } else if has_kerning(self.layout.aat) {
                apply_kern = self.script_fallback_position;
            }
        }

        let apply_fallback_kern = !(apply_gpos || apply_kerx || apply_kern);
        let zero_marks = self.script_zero_marks
            && !apply_kerx
            && (!apply_kern || !has_machine_kerning(self.layout.aat));

        let has_gpos_mark = ot_map.get_1_mask(Tag::new(b"mark")) != 0;

        let mut adjust_mark_positioning_when_zeroing =
            !apply_gpos && !apply_kerx && (!apply_kern || !has_cross_kerning(self.layout.aat));

        let fallback_mark_positioning =
            adjust_mark_positioning_when_zeroing && self.script_fallback_position;

        // If we're using morx shaping, we cancel mark position adjustment because
        // Apple Color Emoji assumes this will NOT be done when forming emoji sequences;
        // https://github.com/harfbuzz/harfbuzz/issues/2967.
        if apply_morx {
            adjust_mark_positioning_when_zeroing = false;
        }

        // According to Ned, trak is applied by default for "modern fonts", as detected by presence of STAT table.
        // https://github.com/googlefonts/fontations/issues/1492
        let apply_trak = self.layout.apply_trak;

        let mut plan = ShapePlan {
            direction: self.direction,
            script: self.script,
            language: self.language,
            shaper: self.shaper,
            ot_map,
            aat_map,
            data: None,
            frac_mask,
            numr_mask,
            dnom_mask,
            rtlm_mask,
            kern_mask,
            requested_kerning,
            has_frac,
            has_vert,
            has_gpos_mark,
            zero_marks,
            fallback_glyph_classes,
            fallback_mark_positioning,
            adjust_mark_positioning_when_zeroing,
            apply_gpos,
            apply_kern,
            apply_fallback_kern,
            apply_kerx,
            apply_morx,
            apply_trak,
            user_features: features.into(),
        };

        if let Some(func) = self.shaper.create_data {
            plan.data = Some(func(&plan));
        }

        plan
    }
}
