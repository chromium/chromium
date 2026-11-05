//! ClearType policy and interpreter state.

use raw::{tables::gasp::GaspRangeBehavior, TableProvider};

/// The smoothing and grid fitting a font requests at a given size.
///
/// A rasterizer needs both properties. [`Self::symmetric_smoothing`] selects
/// antialiasing along both axes or only horizontally. [`Self::grid_fit`]
/// selects whether to hint at all. Windows reads the same two properties from
/// the same source, so honoring them makes text match Windows.
///
/// See <https://learn.microsoft.com/en-us/typography/opentype/spec/gasp>.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct RasterPolicy {
    /// True if the rasterizer should smooth along both axes.
    ///
    /// When false, the font expects antialiasing only in the horizontal
    /// direction. A rasterizer that smooths in both directions produces
    /// softer horizontal edges than the font expects.
    ///
    /// This also sets the symmetric rendering bit of the `GETINFO`
    /// instruction.
    pub symmetric_smoothing: bool,
    /// True if TrueType grid fitting should be applied.
    pub grid_fit: bool,
}

impl RasterPolicy {
    /// Returns the policy DirectWrite applies when no version 1 `gasp` range
    /// covers the size.
    ///
    /// This is asymmetric smoothing through 20 ppem for a font that has glyph
    /// instructions, and symmetric smoothing for everything else.
    pub(crate) fn fallback(has_glyph_instructions: bool, ppem: u16) -> Self {
        Self {
            symmetric_smoothing: !has_glyph_instructions || ppem > 20,
            grid_fit: has_glyph_instructions,
        }
    }

    /// Resolves the policy for the given font at the given nominal ppem.
    pub(crate) fn resolve<'a>(font: &impl TableProvider<'a>, ppem: u16) -> Self {
        let has_glyph_instructions = font
            .selected_maxp()
            .map(|maxp| maxp.max_size_of_instructions().unwrap_or_default() != 0)
            .unwrap_or_default();
        let mut policy = Self::fallback(has_glyph_instructions, ppem);
        if let Ok(gasp) = font.gasp() {
            if gasp.version() >= 1 {
                if let Some(range) = gasp
                    .gasp_ranges()
                    .iter()
                    .find(|range| ppem <= range.range_max_ppem())
                {
                    let behavior = range.range_gasp_behavior();
                    policy.symmetric_smoothing =
                        behavior.contains(GaspRangeBehavior::GASP_SYMMETRIC_SMOOTHING);
                    policy.grid_fit = behavior.contains(GaspRangeBehavior::GASP_SYMMETRIC_GRIDFIT);
                }
            }
        }
        policy
    }
}

impl Default for RasterPolicy {
    fn default() -> Self {
        Self {
            symmetric_smoothing: true,
            grid_fit: true,
        }
    }
}

/// The set of ClearType behaviors the TrueType interpreter applies.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum ClearType {
    /// Every glyph is grid fitted, the symmetric rendering bit of `GETINFO`
    /// follows the hinting options alone, and horizontal moves round to whole
    /// pixels.
    Classic,
    /// A version 1 `gasp` table selects symmetric smoothing and grid fitting
    /// per size, and glyphs that opt out of backward compatibility keep
    /// fractional horizontal positions.
    Enhanced,
}

/// ClearType configuration for one hinting instance.
#[derive(Copy, Clone, Debug)]
pub struct ClearTypeState {
    /// The set of ClearType behaviors the interpreter applies.
    pub mode: ClearType,
    /// True if the font produces broken contours without hinting.
    ///
    /// Their glyph programs run without backward compatibility, and the native
    /// ClearType adjustments are skipped.
    pub is_hint_reliant: bool,
    /// The policy resolved from the `gasp` table for this instance's size.
    pub policy: RasterPolicy,
}

impl ClearTypeState {
    /// Creates the state for `mode`.
    ///
    /// `is_hint_reliant` and `policy` describe the font at the size.
    /// [`ClearType::Classic`] ignores the policy and applies the defaults.
    pub fn new(mode: ClearType, is_hint_reliant: bool, policy: RasterPolicy) -> Self {
        match mode {
            ClearType::Classic => Self {
                is_hint_reliant,
                ..Self::default()
            },
            ClearType::Enhanced => Self {
                mode,
                is_hint_reliant,
                policy,
            },
        }
    }

    /// Returns true if the native ClearType adjustments to the interpreter
    /// are active.
    pub fn is_native_enhanced(&self) -> bool {
        self.mode == ClearType::Enhanced && !self.is_hint_reliant
    }

    /// Returns the resolved value for the symmetric rendering bit of
    /// `GETINFO`.
    pub fn symmetric_smoothing(&self) -> bool {
        self.mode == ClearType::Classic || self.policy.symmetric_smoothing
    }
}

impl Default for ClearTypeState {
    fn default() -> Self {
        Self {
            mode: ClearType::Classic,
            is_hint_reliant: false,
            policy: RasterPolicy::default(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use raw::FontRef;

    // Tinos carries a version 1 gasp with ranges (10, 0xE), (16, 0x7) and
    // (0xFFFF, 0xF), so symmetric smoothing is off from 11 through 16 ppem.
    #[test]
    fn policy_from_gasp_v1() {
        let font = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
        for (ppem, symmetric) in [(8, true), (10, true), (11, false), (16, false), (17, true)] {
            let policy = RasterPolicy::resolve(&font, ppem);
            assert_eq!(policy.symmetric_smoothing, symmetric, "at {ppem} ppem");
            assert!(policy.grid_fit, "at {ppem} ppem");
        }
    }

    // Every range of Cousine's gasp asks for both properties, so the policy
    // never differs from the default.
    #[test]
    fn policy_from_permissive_gasp() {
        let font = FontRef::new(font_test_data::COUSINE_HINT_SUBSET).unwrap();
        for ppem in [8, 12, 20, 40] {
            let policy = RasterPolicy::resolve(&font, ppem);
            assert!(policy.symmetric_smoothing);
            assert!(policy.grid_fit);
        }
    }

    // Without a version 1 gasp, DirectWrite smooths asymmetrically through
    // 20 ppem for a font that carries glyph instructions.
    #[test]
    fn fallback_policy() {
        for (ppem, symmetric) in [(8, false), (20, false), (21, true), (40, true)] {
            assert_eq!(
                RasterPolicy::fallback(true, ppem),
                RasterPolicy {
                    symmetric_smoothing: symmetric,
                    grid_fit: true,
                },
                "at {ppem} ppem"
            );
        }
        for ppem in [8, 20, 21, 40] {
            assert_eq!(
                RasterPolicy::fallback(false, ppem),
                RasterPolicy {
                    symmetric_smoothing: true,
                    grid_fit: false,
                },
                "at {ppem} ppem"
            );
        }
    }

    // A font with no glyph instructions is smoothed symmetrically at every
    // size and is not grid fit.
    #[test]
    fn policy_for_uninstructed_font() {
        let font = FontRef::new(font_test_data::AHEM).unwrap();
        for ppem in [8, 12, 30] {
            let policy = RasterPolicy::resolve(&font, ppem);
            assert!(policy.symmetric_smoothing, "at {ppem} ppem");
            assert!(!policy.grid_fit, "at {ppem} ppem");
        }
    }

    // The policy applies through a font's parsed tables as well as a
    // FontRef, which is how the scaler reads it.
    #[test]
    fn policy_through_the_font_model() {
        let font = crate::font::Font::new(font_test_data::TINOS_SUBSET.to_vec(), 0).unwrap();
        let policy = RasterPolicy::resolve(&font.tables(), 12);
        assert!(!policy.symmetric_smoothing);
        assert!(policy.grid_fit);
    }

    #[test]
    fn classic_state_ignores_the_font() {
        let font = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
        let policy = RasterPolicy::resolve(&font, 12);
        let state = ClearTypeState::new(ClearType::Classic, true, policy);
        assert!(state.symmetric_smoothing());
        assert!(!state.is_native_enhanced());
    }

    #[test]
    fn enhanced_state_follows_the_gasp() {
        let font = FontRef::new(font_test_data::TINOS_SUBSET).unwrap();
        let policy = RasterPolicy::resolve(&font, 12);
        let state = ClearTypeState::new(ClearType::Enhanced, false, policy);
        assert!(!state.symmetric_smoothing());
        assert!(state.is_native_enhanced());
        // A hint reliant font keeps the classic interpreter behavior.
        let state = ClearTypeState::new(ClearType::Enhanced, true, policy);
        assert!(!state.is_native_enhanced());
    }
}
