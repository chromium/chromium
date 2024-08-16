//! Autohinting specific metrics.

use crate::collections::SmallVec;

// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/afblue.h#L328>
pub(super) const MAX_BLUES: usize = 8;

// FreeType keeps a single array of blue values per metrics set
// and mutates when the scale factor changes. We'll separate them so
// that we can reuse unscaled metrics as immutable state without
// recomputing them (which is the expensive part).
// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/autofit/aflatin.h#L77>
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub(super) struct UnscaledBlue {
    pub position: i32,
    pub overshoot: i32,
    pub ascender: i32,
    pub descender: i32,
    pub flags: u32,
}

pub(super) type UnscaledBlues = SmallVec<UnscaledBlue, MAX_BLUES>;
