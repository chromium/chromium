use alloc::boxed::Box;
use core::any::Any;
use smallvec::SmallVec;

use crate::aat::map::AatMap;
use crate::ot::shaper::OtShaper;

use crate::ot::map::*;
use crate::LayoutData;
use crate::{font::Font, Direction, Feature, Language, Mask, Script, ShapePlanner, ShaperFont};

/// A reusable plan for shaping with one set of segment properties.
///
/// A plan records the selected shaping features and lookups. Reuse it with a
/// compatible font, direction, script, language, and feature set.
pub struct ShapePlan {
    pub(crate) direction: Direction,
    pub(crate) script: Option<Script>,
    pub(crate) language: Option<Language>,
    pub(crate) shaper: &'static OtShaper,
    pub(crate) ot_map: OtMap,
    pub(crate) aat_map: AatMap,
    pub(crate) data: Option<Box<dyn Any + Send + Sync>>,

    pub(crate) frac_mask: Mask,
    pub(crate) numr_mask: Mask,
    pub(crate) dnom_mask: Mask,
    pub(crate) rtlm_mask: Mask,
    pub(crate) kern_mask: Mask,

    pub(crate) requested_kerning: bool,
    pub(crate) has_frac: bool,
    pub(crate) has_vert: bool,
    pub(crate) has_gpos_mark: bool,
    pub(crate) zero_marks: bool,
    pub(crate) fallback_glyph_classes: bool,
    pub(crate) fallback_mark_positioning: bool,
    pub(crate) adjust_mark_positioning_when_zeroing: bool,

    pub(crate) apply_gpos: bool,
    pub(crate) apply_fallback_kern: bool,
    pub(crate) apply_kern: bool,
    pub(crate) apply_kerx: bool,
    pub(crate) apply_morx: bool,
    pub(crate) apply_trak: bool,

    pub(crate) user_features: SmallVec<[Feature; 4]>,
}

impl ShapePlan {
    /// Builds a plan for a font and segment properties.
    ///
    /// # Panics
    ///
    /// Panics if `direction` is [`Direction::Invalid`].
    pub fn new(
        font: &Font,
        direction: Direction,
        script: Option<Script>,
        language: Option<&Language>,
        user_features: &[Feature],
    ) -> Self {
        let shaping_font = ShaperFont::new(font);
        Self::from_layout(
            shaping_font.layout(),
            direction,
            script,
            language,
            user_features,
        )
    }

    pub(crate) fn from_layout(
        layout: LayoutData<'_>,
        direction: Direction,
        script: Option<Script>,
        language: Option<&Language>,
        user_features: &[Feature],
    ) -> Self {
        assert_ne!(
            direction,
            Direction::Invalid,
            "Direction must not be Invalid"
        );
        let mut planner = ShapePlanner::new(layout, direction, script, language);
        planner.collect_features(user_features);
        planner.compile(user_features)
    }

    pub(crate) fn data<T: 'static>(&self) -> &T {
        self.data.as_ref().unwrap().downcast_ref().unwrap()
    }

    /// The direction of the text.
    pub fn direction(&self) -> Direction {
        self.direction
    }

    /// The script of the text.
    pub fn script(&self) -> Option<Script> {
        self.script
    }

    /// The language of the text.
    pub fn language(&self) -> Option<&Language> {
        self.language.as_ref()
    }
}

/// The properties used to match a reusable [`ShapePlan`].
pub struct ShapePlanKey<'a> {
    script: Option<Script>,
    direction: Direction,
    language: Option<&'a Language>,
    pub(crate) feature_variations: [Option<u32>; 2],
    features: &'a [Feature],
}

impl<'a> ShapePlanKey<'a> {
    /// Creates a key for a font, script, and direction.
    pub fn new(font: &Font, script: Option<Script>, direction: Direction) -> Self {
        let variations = font.feature_variations();
        Self {
            script,
            direction,
            language: None,
            feature_variations: [variations.gsub, variations.gpos],
            features: &[],
        }
    }

    /// Sets the language to use for this shape plan key.
    pub fn language(mut self, language: Option<&'a Language>) -> Self {
        self.language = language;
        self
    }

    /// Sets the features to use for this shape plan key.
    pub fn features(mut self, features: &'a [Feature]) -> Self {
        self.features = features;
        self
    }

    /// Returns true if this key is a match for the given shape plan.
    pub fn matches(&self, plan: &ShapePlan) -> bool {
        self.script == plan.script
            && self.direction == plan.direction
            && self.language == plan.language.as_ref()
            && self.feature_variations == *plan.ot_map.feature_variations()
            && features_equivalent(self.features, &plan.user_features)
    }
}

fn features_equivalent(features_a: &[Feature], features_b: &[Feature]) -> bool {
    if features_a.len() != features_b.len() {
        return false;
    }
    for (a, b) in features_a.iter().zip(features_b) {
        if a.tag != b.tag
            || a.value != b.value
            || (a.start == Feature::GLOBAL_START && a.end == Feature::GLOBAL_END)
                != (b.start == Feature::GLOBAL_START && b.end == Feature::GLOBAL_END)
        {
            return false;
        }
    }
    true
}

#[cfg(test)]
mod tests {
    use super::ShapePlan;

    #[test]
    fn test_shape_plan_is_send_and_sync() {
        fn ensure_send_and_sync<T: Send + Sync>() {}
        ensure_send_and_sync::<ShapePlan>();
    }
}
