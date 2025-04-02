use crate::{error::FendError, result::FResult};

/// This trait controls fend's interrupt functionality.
///
/// If the `should_interrupt` method returns `true`, fend will attempt to
/// interrupt the current calculation and return `Err(FendError::Interrupted)`.
///
/// This can be used to implement timeouts or user interrupts via e.g. Ctrl-C.
pub trait Interrupt {
	/// Returns `true` if the current calculation should be interrupted.
	fn should_interrupt(&self) -> bool;
}

pub(crate) fn test_int<I: crate::error::Interrupt>(int: &I) -> FResult<()> {
	if int.should_interrupt() {
		Err(FendError::Interrupted)
	} else {
		Ok(())
	}
}

#[derive(Default)]
pub(crate) struct Never;
impl Interrupt for Never {
	fn should_interrupt(&self) -> bool {
		false
	}
}
