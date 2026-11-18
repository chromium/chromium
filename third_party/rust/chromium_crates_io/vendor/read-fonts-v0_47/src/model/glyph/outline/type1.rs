//! Outlines from Type 1 charstrings.
//!
//! The charstrings are parsed and evaluated by [`crate::ps::type1`].

use super::{DrawOptions, DrawnMetrics, PathSink};
use crate::model::Error;
use crate::{
    ps::type1::{Type1Font, Type1Instance},
    types::GlyphId,
};

/// A charstring from a Type 1 [`Font`](crate::model::Font).
#[derive(Clone, Copy)]
pub struct Type1Glyph<'a> {
    /// The font at the location the glyph is drawn at.
    pub(super) font: Type1Instance<'a>,
    pub(super) glyph: GlyphId,
}

impl<'a> Type1Glyph<'a> {
    /// Returns the glyph identifier.
    pub fn glyph(&self) -> GlyphId {
        self.glyph
    }

    /// Returns the Type 1 font that evaluates the charstring.
    pub fn type1_font(&self) -> &'a Type1Font {
        self.font.font()
    }

    pub(super) fn draw(
        &self,
        options: &DrawOptions,
        mut sink: &mut dyn PathSink,
    ) -> Result<DrawnMetrics, Error> {
        let h_advance = self.font.draw(self.glyph, options.ppem, &mut sink)?;
        Ok(DrawnMetrics {
            h_advance,
            ..Default::default()
        })
    }
}
