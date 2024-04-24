//! Compensating for the engine characteristics (rounding).
//!
//! Implements 4 instructions.
//!
//! See <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructions#compensating-for-the-engine-characteristics>

use super::{Engine, OpResult};

impl<'a> Engine<'a> {
    /// Round value.
    ///
    /// ROUND\[ab\] (0x68 - 0x6B)
    ///
    /// Pops: n1
    /// Pushes: n2
    ///
    /// Rounds a value according to the state variable round_state while
    /// compensating for the engine. n1 is popped off the stack and,
    /// depending on the engine characteristics, is increased or decreased
    /// by a set amount. The number obtained is then rounded and pushed
    /// back onto the stack as n2.
    ///
    /// See <https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructions#round-value>
    /// and <https://gitlab.freedesktop.org/freetype/freetype/-/blob/57617782464411201ce7bbc93b086c1b4d7d84a5/src/truetype/ttinterp.c#L3143>
    pub(super) fn op_round(&mut self) -> OpResult {
        let n1 = self.value_stack.pop_f26dot6()?;
        let n2 = self.graphics.round(n1);
        self.value_stack.push(n2.to_bits())
    }
}
