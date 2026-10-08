use crate::{Direction, Script};

/// The reason a call to [`shape`](crate::shape) could not produce glyphs.
///
/// Each of these is a misuse of the API. Running out of room is not among
/// them: pathological input can provoke it, so it is reported through
/// [`Buffer::allocation_successful`](crate::Buffer::allocation_successful)
/// rather than as a failure to shape.
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
#[non_exhaustive]
pub enum ShapeError {
    /// The buffer already holds the glyphs from an earlier call.
    ///
    /// To shape the same contents again, set the content type back to
    /// [`ContentType::Unicode`](crate::ContentType::Unicode) with
    /// [`set_content_type`](crate::Buffer::set_content_type); to shape something else,
    /// clear the buffer and fill it afresh.
    AlreadyShaped,

    /// The buffer has no direction, so no plan could be built for it.
    ///
    /// Call [`guess_segment_properties`](crate::Buffer::guess_segment_properties) to
    /// infer one from the contents, or set it with
    /// [`set_direction`](crate::Buffer::set_direction).
    DirectionUnset,

    /// The buffer's direction is not the one the supplied plan was built for.
    DirectionMismatch {
        /// The direction the plan was built for.
        plan: Direction,
        /// The direction the buffer carries.
        buffer: Direction,
    },

    /// The buffer's script is not the one the supplied plan was built for.
    ScriptMismatch {
        /// The script the plan was built for.
        plan: Script,
        /// The script the buffer carries.
        buffer: Script,
    },
}

impl core::fmt::Display for ShapeError {
    fn fmt(&self, fmt: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::AlreadyShaped => fmt.write_str("buffer already holds shaped glyphs"),
            Self::DirectionUnset => fmt.write_str("buffer has no direction set"),
            Self::DirectionMismatch { plan, buffer } => write!(
                fmt,
                "buffer direction does not match plan direction: {buffer:?} != {plan:?}"
            ),
            Self::ScriptMismatch { plan, buffer } => write!(
                fmt,
                "buffer script does not match plan script: {buffer:?} != {plan:?}"
            ),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for ShapeError {}
