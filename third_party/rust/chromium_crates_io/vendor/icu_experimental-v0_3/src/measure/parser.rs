// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use smallvec::SmallVec;

use crate::measure::measureunit::MeasureUnit;
use crate::measure::power::get_power;
use crate::measure::si_prefix::get_si_prefix;
use crate::units::InvalidUnitError;

use icu_provider::prelude::*;
use icu_provider::DataError;

use super::provider::si_prefix::{Base, SiPrefix};
use super::provider::single_unit::SingleUnit;

// TODO: add test cases for this parser after adding UnitsTest.txt to the test data.
/// A parser for the CLDR unit identifier (e.g. `meter-per-square-second`)
pub struct MeasureUnitParser {
    /// Contains the trie for the unit identifiers.
    payload: DataPayload<super::provider::trie::UnitsTrieV1>,
}

#[cfg(feature = "compiled_data")]
impl Default for MeasureUnitParser {
    /// Creates a new [`MeasureUnitParser`] from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    fn default() -> Self {
        Self {
            payload: DataPayload::from_static_ref(crate::provider::Baked::SINGLETON_UNITS_TRIE_V1),
        }
    }
}

impl MeasureUnitParser {
    /// Creates a new [`MeasureUnitParser`] from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            payload: DataPayload::from_static_ref(crate::provider::Baked::SINGLETON_UNITS_TRIE_V1),
        }
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<super::provider::trie::UnitsTrieV1>,
    {
        let payload = provider.load(DataRequest::default())?.payload;

        Ok(Self { payload })
    }

    icu_provider::gen_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    /// Get the unit id.
    /// NOTE:
    ///    if the unit id is found, the function will return (unit id, part without the unit id and without `-` at the beginning of the remaining part if it exists).
    ///    if the unit id is not found, the function will return an error.
    fn get_unit_id<'a>(&self, part: &'a [u8]) -> Result<(u16, &'a [u8]), InvalidUnitError> {
        let mut cursor = self.payload.get().trie.cursor();
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

        let mut constant_denominator = 0;
        let mut single_units = SmallVec::<[SingleUnit; 8]>::new();
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
                                    if let Some(remain) = code_units.strip_prefix(b"per-") {
                                        // First time locating `per-` keyword.
                                        sign = -1;
                                        code_units = remain;

                                        // Extract the constant denominator if present.
                                        let mut split = remain.splitn(2, |c| *c == b'-');
                                        if let Some(possible_constant_denominator) = split.next() {
                                            // Try to parse the possible constant denominator as a u64.
                                            if let Some(parsed_denominator) =
                                                core::str::from_utf8(possible_constant_denominator)
                                                    .ok()
                                                    .and_then(|s| s.parse::<f64>().ok())
                                                    .and_then(|num| {
                                                        if num > u64::MAX as f64 {
                                                            None
                                                        } else {
                                                            Some(num as u64)
                                                        }
                                                    })
                                            {
                                                constant_denominator = parsed_denominator;
                                                code_units = split.next().unwrap_or(&[]);
                                            }
                                        }

                                        continue;
                                    }

                                    return Err(InvalidUnitError);
                                }
                                Err(e) => return Err(e),
                            };
                        (si_prefix, unit_id, identifier_part_without_unit_id)
                    }
                };

            single_units.push(SingleUnit {
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

        // There is no unit without any valid single units.
        if single_units.is_empty() {
            return Err(InvalidUnitError);
        }

        Ok(MeasureUnit {
            single_units,
            constant_denominator,
        })
    }
}

#[cfg(test)]
mod tests {
    use crate::measure::parser::MeasureUnitParser;

    #[test]
    fn test_parser_cases() {
        let test_cases = vec![
            ("meter-per-square-second", 2, 0),
            ("portion-per-1e9", 1, 1_000_000_000),
            ("portion-per-1000000000", 1, 1_000_000_000),
            ("liter-per-100-kilometer", 2, 100),
        ];
        let parser = MeasureUnitParser::default();

        for (input, expected_len, expected_denominator) in test_cases {
            let measure_unit = parser.try_from_str(input).unwrap();
            assert_eq!(measure_unit.single_units.len(), expected_len);
            assert_eq!(measure_unit.constant_denominator, expected_denominator);
        }
    }

    #[test]
    fn test_invlalid_unit_ids() {
        let test_cases = vec![
            "kilo",
            "kilokilo",
            "onekilo",
            "meterkilo",
            "meter-kilo",
            "k",
            "meter-",
            "meter+",
            "-meter",
            "+meter",
            "-kilometer",
            "+kilometer",
            "-pow2-meter",
            "+pow2-meter",
            "p2-meter",
            "p4-meter",
            "+",
            "-",
            "-mile",
            "-and-mile",
            "-per-mile",
            "one",
            "one-one",
            "one-per-mile",
            "one-per-cubic-centimeter",
            "square--per-meter",
            "metersecond", // Must have a compound part between single units
            // Negative powers not supported in mixed units yet. TODO(CLDR-13701).
            "per-hour-and-hertz",
            "hertz-and-per-hour",
            // Compound units not supported in mixed units yet. TODO(CLDR-13701).
            "kilonewton-meter-and-newton-meter",
            // Invalid units due to invalid constant denominator
            "meter-per--20-second",
            "meter-per-1000-1e9-second",
            "meter-per-1e19-second",
            "per-1000",
            "meter-per-1000-1000",
            "meter-per-1000-second-1000-kilometer",
            "1000-meter",
            "meter-1000",
            "meter-per-1000-1000",
            "meter-per-1000-second-1000-kilometer",
            "per-1000-and-per-1000",
            "liter-per-kilometer-100",
        ];

        for input in test_cases {
            // TODO(Uicode-org/icu4x#6271):
            //      This is invalid, but because `100-kilometer` is a valid unit, it is not rejected.
            //      This should be fixed in CLDR.
            if input == "meter-per-100-100-kilometer" {
                continue;
            }

            let parser = MeasureUnitParser::default();
            let measure_unit = parser.try_from_str(input);
            if measure_unit.is_ok() {
                println!("OK:  {}", input);
                continue;
            }
            assert!(measure_unit.is_err());
        }
    }
}
