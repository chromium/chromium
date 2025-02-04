// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::vec::Vec;
use zerotrie::ZeroTrieSimpleAscii;

use crate::measure::measureunit::MeasureUnit;
use crate::measure::power::get_power;
use crate::measure::si_prefix::get_si_prefix;
use crate::units::InvalidUnitError;

use super::provider::si_prefix::{Base, SiPrefix};
use super::provider::single_unit::SingleUnit;

// TODO: add test cases for this parser after adding UnitsTest.txt to the test data.
/// A parser for the CLDR unit identifier (e.g. `meter-per-square-second`)
pub struct MeasureUnitParser<'data> {
    /// Contains the trie for the unit identifiers.
    units_trie: &'data ZeroTrieSimpleAscii<[u8]>,
}

impl<'data> MeasureUnitParser<'data> {
    // TODO: revisit the public nature of the API. Maybe we should make it private and add a function to create it from a ConverterFactory.
    /// Creates a new MeasureUnitParser from a ZeroTrie payload.
    pub fn from_payload(payload: &'data ZeroTrieSimpleAscii<[u8]>) -> Self {
        Self {
            units_trie: payload,
        }
    }

    /// Get the unit id.
    /// NOTE:
    ///    if the unit id is found, the function will return (unit id, part without the unit id and without `-` at the beginning of the remaining part if it exists).
    ///    if the unit id is not found, the function will return an error.
    fn get_unit_id<'a>(&self, part: &'a [u8]) -> Result<(u16, &'a [u8]), InvalidUnitError> {
        let mut cursor = self.units_trie.cursor();
        let mut longest_match = Err(InvalidUnitError);

        for (i, byte) in part.iter().enumerate() {
            cursor.step(*byte);
            if cursor.is_empty() {
                break;
            }
            if let Some(value) = cursor.take_value() {
                longest_match = Ok((value as u16, &part[i + 1..]));
            }
        }
        longest_match
    }

    fn get_power<'a>(&self, part: &'a [u8]) -> Result<(u8, &'a [u8]), InvalidUnitError> {
        let (power, part_without_power) = get_power(part);

        // If the power is not found, return the part as it is.
        if part_without_power.len() == part.len() {
            return Ok((power, part));
        }

        // If the power is found, this means that the part must start with the `-` sign.
        match part_without_power.strip_prefix(b"-") {
            Some(part_without_power) => Ok((power, part_without_power)),
            None => Err(InvalidUnitError),
        }
    }

    /// Get the SI prefix.
    /// NOTE:
    ///    if the prefix is not found, the function will return (SiPrefix { power: 0, base: Base::Decimal }, part).
    fn get_si_prefix<'a>(&self, part: &'a [u8]) -> (SiPrefix, &'a [u8]) {
        let (si_prefix, part_without_si_prefix) = get_si_prefix(part);
        if part_without_si_prefix.len() == part.len() {
            return (si_prefix, part);
        }

        match part_without_si_prefix.strip_prefix(b"-") {
            Some(part_without_dash) => (si_prefix, part_without_dash),
            None => (si_prefix, part_without_si_prefix),
        }
    }

    // TODO: add test cases for this function.
    /// Parses a CLDR unit identifier and returns a MeasureUnit.
    /// Examples include: `meter`, `foot`, `meter-per-second`, `meter-per-square-second`, `meter-per-square-second-per-second`, etc.
    /// Returns:
    ///    - Ok(MeasureUnit) if the identifier is valid.
    ///    - Err(InvalidUnitError) if the identifier is invalid.
    #[inline]
    pub fn try_from_str(&self, s: &str) -> Result<MeasureUnit, InvalidUnitError> {
        self.try_from_utf8(s.as_bytes())
    }

    /// See [`Self::try_from_str`]
    pub fn try_from_utf8(&self, mut code_units: &[u8]) -> Result<MeasureUnit, InvalidUnitError> {
        if code_units.starts_with(b"-") || code_units.ends_with(b"-") {
            return Err(InvalidUnitError);
        }

        let mut measure_unit_items = Vec::<SingleUnit>::new();
        let mut sign = 1;
        while !code_units.is_empty() {
            // First: extract the power.
            let (power, identifier_part_without_power) = self.get_power(code_units)?;

            // Second: extract the si_prefix and the unit_id.
            let (si_prefix, unit_id, identifier_part_without_unit_id) =
                match self.get_unit_id(identifier_part_without_power) {
                    Ok((unit_id, identifier_part_without_unit_id)) => (
                        SiPrefix {
                            power: 0,
                            base: Base::Decimal,
                        },
                        unit_id,
                        identifier_part_without_unit_id,
                    ),
                    Err(_) => {
                        let (si_prefix, identifier_part_without_si_prefix) =
                            self.get_si_prefix(identifier_part_without_power);
                        let (unit_id, identifier_part_without_unit_id) =
                            match self.get_unit_id(identifier_part_without_si_prefix) {
                                Ok((unit_id, identifier_part_without_unit_id)) => {
                                    (unit_id, identifier_part_without_unit_id)
                                }
                                // If the sign is negative, this means that the identifier may contain more than one `per-` keyword.
                                Err(_) if sign == 1 => {
                                    if let Some(remainder) = code_units.strip_prefix(b"per-") {
                                        sign = -1;
                                        code_units = remainder;
                                        continue;
                                    }

                                    return Err(InvalidUnitError);
                                }
                                Err(e) => return Err(e),
                            };
                        (si_prefix, unit_id, identifier_part_without_unit_id)
                    }
                };

            measure_unit_items.push(SingleUnit {
                power: sign * power as i8,
                si_prefix,
                unit_id,
            });

            code_units = match identifier_part_without_unit_id.strip_prefix(b"-") {
                Some(remainder) => remainder,
                None if identifier_part_without_unit_id.is_empty() => {
                    identifier_part_without_unit_id
                }
                None => return Err(InvalidUnitError),
            };
        }

        Ok(MeasureUnit {
            contained_units: measure_unit_items.into(),
        })
    }
}
