//! Shared evaluation of layout and VARC conditions.

use super::{Condition, ConditionSet, LookupConditionRecord};
use crate::limits::MAX_RECURSION_DEPTH;
use crate::tables::variations::{DeltaSetIndex, ItemVariationStore, NO_VARIATION_INDEX};
use crate::types::F2Dot14;
use crate::{model::Error, FontData, FontRead, ReadError};

#[cfg(test)]
#[path = "condition_tests.rs"]
mod tests;

/// Returns a delta function for [`Condition::evaluate`] that reads `store`,
/// usually the GDEF variation store.
///
/// A missing store or delta set has a delta of zero.
pub fn store_deltas<'a>(
    store: Option<&'a ItemVariationStore<'a>>,
    coords: &'a [F2Dot14],
) -> impl Fn(u32) -> Result<f64, Error> + Copy + 'a {
    move |index| {
        Ok(store
            .and_then(|store| {
                store.compute_delta(
                    DeltaSetIndex {
                        outer: (index >> 16) as u16,
                        inner: index as u16,
                    },
                    coords,
                )
            })
            .unwrap_or_default()
            .to_f64())
    }
}

impl Condition<'_> {
    /// Evaluates this condition at the given normalized coordinates.
    ///
    /// Missing coordinates are zero. `delta` resolves format-2 variation
    /// indices using the caller's variation store; it is not called for
    /// [`NO_VARIATION_INDEX`]. The value is tested before rounding.
    ///
    /// A condition of an unknown format is false. Malformed children and
    /// evaluation limits return errors, including beneath a NOT condition.
    /// AND and OR conditions short-circuit.
    pub fn evaluate(
        &self,
        coords: &[F2Dot14],
        delta: impl FnMut(u32) -> Result<f64, Error>,
    ) -> Result<bool, Error> {
        Evaluator::new(coords, delta).evaluate(self, MAX_RECURSION_DEPTH)
    }
}

impl ConditionSet<'_> {
    /// Evaluates all conditions in this set, sharing one total-work budget.
    ///
    /// A null condition offset is unconditional. See [`Condition::evaluate`]
    /// for variation-delta resolution and error handling.
    pub fn evaluate(
        &self,
        coords: &[F2Dot14],
        delta: impl FnMut(u32) -> Result<f64, Error>,
    ) -> Result<bool, Error> {
        let offsets = self.condition_offsets();
        if offsets.len() != usize::from(self.condition_count()) {
            return Err(Error::Malformed);
        }
        let mut evaluator = Evaluator::new(coords, delta);
        for offset in offsets {
            if !evaluator.evaluate_offset(
                self.offset_data(),
                offset.get().to_u32(),
                MAX_RECURSION_DEPTH,
            )? {
                return Ok(false);
            }
        }
        Ok(true)
    }
}

impl LookupConditionRecord {
    /// Evaluates this record's condition, relative to its FeatureLookups table.
    ///
    /// A null offset is unconditional. See [`Condition::evaluate`] for
    /// variation-delta resolution and error handling.
    pub fn evaluate(
        &self,
        data: FontData<'_>,
        coords: &[F2Dot14],
        delta: impl FnMut(u32) -> Result<f64, Error>,
    ) -> Result<bool, Error> {
        Evaluator::new(coords, delta).evaluate_record(data, self)
    }
}

/// Evaluates conditions under one total-work budget.
pub(super) struct Evaluator<'a, F> {
    coords: &'a [F2Dot14],
    delta: F,
    remaining_ops: usize,
}

impl<'a, F: FnMut(u32) -> Result<f64, Error>> Evaluator<'a, F> {
    pub(super) fn new(coords: &'a [F2Dot14], delta: F) -> Self {
        Self {
            coords,
            delta,
            // Enough for a full flat ConditionSet, while bounding repeated
            // visits when several offsets refer to the same child.
            remaining_ops: usize::from(u16::MAX) + 1,
        }
    }

    /// Evaluates a lookup condition record, relative to its FeatureLookups
    /// table.
    pub(super) fn evaluate_record(
        &mut self,
        data: FontData<'_>,
        record: &LookupConditionRecord,
    ) -> Result<bool, Error> {
        self.evaluate_offset(
            data,
            record.condition_offset().offset().to_u32(),
            MAX_RECURSION_DEPTH,
        )
    }

    fn evaluate_offset(
        &mut self,
        data: FontData<'_>,
        offset: u32,
        depth: usize,
    ) -> Result<bool, Error> {
        if offset == 0 {
            return Ok(true);
        }
        let data = data.split_off(offset as usize).ok_or(Error::Malformed)?;
        let condition = match Condition::read(data) {
            Ok(condition) => condition,
            // The spec treats a condition of an unknown format as false.
            Err(ReadError::InvalidFormat(_)) => return Ok(false),
            Err(error) => return Err(error.into()),
        };
        self.evaluate(&condition, depth)
    }

    fn evaluate(&mut self, condition: &Condition<'_>, depth: usize) -> Result<bool, Error> {
        if depth == 0 || self.remaining_ops == 0 {
            return Err(Error::LimitExceeded);
        }
        self.remaining_ops -= 1;
        Ok(match condition {
            Condition::Format1AxisRange(condition) => {
                let coord = self
                    .coords
                    .get(usize::from(condition.axis_index()))
                    .copied()
                    .unwrap_or_default();
                coord >= condition.filter_range_min_value()
                    && coord <= condition.filter_range_max_value()
            }
            Condition::Format2VariableValue(condition) => {
                let index = condition.var_index();
                let delta = if index == NO_VARIATION_INDEX {
                    0.0
                } else {
                    (self.delta)(index)?
                };
                f64::from(condition.default_value()) + delta > 0.0
            }
            Condition::Format3And(condition) => {
                let offsets = condition.condition_offsets();
                if offsets.len() != usize::from(condition.condition_count()) {
                    return Err(Error::Malformed);
                }
                for offset in offsets {
                    if !self.evaluate_offset(
                        condition.offset_data(),
                        offset.get().to_u32(),
                        depth - 1,
                    )? {
                        return Ok(false);
                    }
                }
                true
            }
            Condition::Format4Or(condition) => {
                let offsets = condition.condition_offsets();
                if offsets.len() != usize::from(condition.condition_count()) {
                    return Err(Error::Malformed);
                }
                for offset in offsets {
                    if self.evaluate_offset(
                        condition.offset_data(),
                        offset.get().to_u32(),
                        depth - 1,
                    )? {
                        return Ok(true);
                    }
                }
                false
            }
            Condition::Format5Negate(condition) => !self.evaluate_offset(
                condition.offset_data(),
                condition.condition_offset().to_u32(),
                depth - 1,
            )?,
        })
    }
}
