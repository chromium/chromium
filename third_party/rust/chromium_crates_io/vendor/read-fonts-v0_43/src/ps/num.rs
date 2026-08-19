//! Various types of number parsers.

use super::error::Error;
use crate::Cursor;
use types::Fixed;

#[inline]
pub(crate) fn parse_int(cursor: &mut Cursor, b0: u8) -> Result<i32, Error> {
    // Size   b0 range     Value range              Value calculation
    //--------------------------------------------------------------------------------
    // 1      32 to 246    -107 to +107             b0 - 139
    // 2      247 to 250   +108 to +1131            (b0 - 247) * 256 + b1 + 108
    // 2      251 to 254   -1131 to -108            -(b0 - 251) * 256 - b1 - 108
    // 3      28           -32768 to +32767         b1 << 8 | b2
    // 5      29           -(2^31) to +(2^31 - 1)   b1 << 24 | b2 << 16 | b3 << 8 | b4
    // <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-3-operand-encoding>
    Ok(match b0 {
        32..=246 => b0 as i32 - 139,
        247..=250 => (b0 as i32 - 247) * 256 + cursor.read::<u8>()? as i32 + 108,
        251..=254 => -(b0 as i32 - 251) * 256 - cursor.read::<u8>()? as i32 - 108,
        28 => cursor.read::<i16>()? as i32,
        29 => cursor.read::<i32>()?,
        _ => {
            return Err(Error::InvalidNumber);
        }
    })
}

// Various unnamed constants inlined in FreeType's cff_parse_real function
// <<https://gitlab.freedesktop.org/freetype/freetype/-/blob/82090e67c24259c343c83fd9cefe6ff0be7a7eca/src/cff/cffparse.c#L183>>

// Value returned on overflow
const BCD_OVERFLOW: Fixed = Fixed::from_bits(0x7FFFFFFF);
// Value returned on underflow
const BCD_UNDERFLOW: Fixed = Fixed::ZERO;
// Limit at which we stop accumulating `number` and increase
// the exponent instead
const BCD_NUMBER_LIMIT: i32 = 0xCCCCCCC;
// Limit for the integral part of the result
const BCD_INTEGER_LIMIT: i32 = 0x7FFF;

// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/82090e67c24259c343c83fd9cefe6ff0be7a7eca/src/cff/cffparse.c#L150>
pub(crate) const BCD_POWER_TENS: [i32; 10] = [
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000,
];

/// Components for computing a fixed point value for a binary coded decimal
/// number.
#[derive(Copy, Clone, PartialEq, Eq, Default, Debug)]
pub struct BcdComponents {
    /// If overflow or underflow is detected early, then this
    /// contains the resulting value and we skip further
    /// processing.
    error: Option<Fixed>,
    number: i32,
    sign: i32,
    exponent: i32,
    exponent_add: i32,
    integer_len: i32,
    fraction_len: i32,
}

impl BcdComponents {
    /// Parse a binary coded decimal number.
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/82090e67c24259c343c83fd9cefe6ff0be7a7eca/src/cff/cffparse.c#L183>
    pub(crate) fn parse(cursor: &mut Cursor) -> Result<Self, Error> {
        enum Phase {
            Integer,
            Fraction,
            Exponent,
        }
        let mut phase = Phase::Integer;
        let mut sign = 1i32;
        let mut exponent_sign = 1i32;
        let mut number = 0i32;
        let mut exponent = 0i32;
        let mut exponent_add = 0i32;
        let mut integer_len = 0;
        let mut fraction_len = 0;
        // Nibble value    Represents
        //----------------------------------
        // 0 to 9          0 to 9
        // a               . (decimal point)
        // b               E
        // c               E-
        // d               <reserved>
        // e               - (minus)
        // f               end of number
        // <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-5-nibble-definitions>
        'outer: loop {
            let b = cursor.read::<u8>()?;
            for nibble in [(b >> 4) & 0xF, b & 0xF] {
                match phase {
                    Phase::Integer => match nibble {
                        0x0..=0x9 => {
                            if number >= BCD_NUMBER_LIMIT {
                                exponent_add += 1;
                            } else if nibble != 0 || number != 0 {
                                number = number * 10 + nibble as i32;
                                integer_len += 1;
                            }
                        }
                        0xE => sign = -1,
                        0xA => {
                            phase = Phase::Fraction;
                        }
                        0xB => {
                            phase = Phase::Exponent;
                        }
                        0xC => {
                            phase = Phase::Exponent;
                            exponent_sign = -1;
                        }
                        _ => break 'outer,
                    },
                    Phase::Fraction => match nibble {
                        0x0..=0x9 => {
                            if nibble == 0 && number == 0 {
                                exponent_add -= 1;
                            } else if number < BCD_NUMBER_LIMIT && fraction_len < 9 {
                                number = number * 10 + nibble as i32;
                                fraction_len += 1;
                            }
                        }
                        0xB => {
                            phase = Phase::Exponent;
                        }
                        0xC => {
                            phase = Phase::Exponent;
                            exponent_sign = -1;
                        }
                        _ => break 'outer,
                    },
                    Phase::Exponent => {
                        match nibble {
                            0x0..=0x9 => {
                                // Arbitrarily limit exponent
                                if exponent > 1000 {
                                    return if exponent_sign == -1 {
                                        Ok(BCD_UNDERFLOW.into())
                                    } else {
                                        Ok(BCD_OVERFLOW.into())
                                    };
                                } else {
                                    exponent = exponent * 10 + nibble as i32;
                                }
                            }
                            _ => break 'outer,
                        }
                    }
                }
            }
        }
        exponent *= exponent_sign;
        Ok(Self {
            error: None,
            number,
            sign,
            exponent,
            exponent_add,
            integer_len,
            fraction_len,
        })
    }

    /// Returns the fixed point value for the precomputed components,
    /// optionally using an internal scale factor of 1000 to
    /// increase fractional precision.
    pub fn value(&self, scale_by_1000: bool) -> Fixed {
        if let Some(error) = self.error {
            return error;
        }
        let mut number = self.number;
        if number == 0 {
            return Fixed::ZERO;
        }
        let mut exponent = self.exponent;
        let mut integer_len = self.integer_len;
        let mut fraction_len = self.fraction_len;
        if scale_by_1000 {
            exponent += 3 + self.exponent_add;
        } else {
            exponent += self.exponent_add;
        }
        integer_len += exponent;
        fraction_len -= exponent;
        if integer_len > 5 {
            return BCD_OVERFLOW;
        }
        if integer_len < -5 {
            return BCD_UNDERFLOW;
        }
        // Remove non-significant digits
        if integer_len < 0 {
            number /= BCD_POWER_TENS[(-integer_len) as usize];
            fraction_len += integer_len;
        }
        // Can only happen if exponent was non-zero
        if fraction_len == 10 {
            number /= 10;
            fraction_len -= 1;
        }
        // Convert to fixed
        let mut result = if fraction_len > 0 {
            let b = BCD_POWER_TENS[fraction_len as usize];
            if number / b > BCD_INTEGER_LIMIT {
                0
            } else {
                (Fixed::from_bits(number) / Fixed::from_bits(b)).to_bits()
            }
        } else {
            number = number.wrapping_mul(BCD_POWER_TENS[-fraction_len as usize]);
            if number > BCD_INTEGER_LIMIT {
                return BCD_OVERFLOW;
            } else {
                number << 16
            }
        };
        if scale_by_1000 {
            // FreeType stores the scaled value and does a fixed division by
            // 1000 when the blue metrics are requested. We just do it here
            // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/f1cd6dbfa0c98f352b698448f40ac27e8fb3832e/src/psaux/psft.c#L554>
            result = (Fixed::from_bits(result) / Fixed::from_i32(1000)).to_bits();
        }
        Fixed::from_bits(result * self.sign)
    }

    /// Returns the fixed point value for the components along with a
    /// dynamically determined scale factor.
    ///
    /// Use for processing FontMatrix components.
    ///
    /// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/f1cd6dbfa0c98f352b698448f40ac27e8fb3832e/src/cff/cffparse.c#L332>
    pub(crate) fn dynamically_scaled_value(&self) -> (Fixed, i32) {
        if let Some(error) = self.error {
            return (error, 0);
        }
        let mut number = self.number;
        if number == 0 {
            return (Fixed::ZERO, 0);
        }
        let mut exponent = self.exponent;
        let integer_len = self.integer_len;
        let mut fraction_len = self.fraction_len;
        exponent += self.exponent_add;
        fraction_len += integer_len;
        exponent += integer_len;
        let result;
        let scaling;
        if fraction_len <= 5 {
            if number > BCD_INTEGER_LIMIT {
                result = Fixed::from_bits(number) / Fixed::from_bits(10);
                scaling = exponent - fraction_len + 1;
            } else {
                if exponent > 0 {
                    // Make scaling as small as possible
                    let new_fraction_len = exponent.min(5);
                    let shift = new_fraction_len - fraction_len;
                    if shift > 0 {
                        exponent -= new_fraction_len;
                        number *= BCD_POWER_TENS[shift as usize];
                        if number > BCD_INTEGER_LIMIT {
                            number /= 10;
                            exponent += 1;
                        }
                    } else {
                        exponent -= fraction_len;
                    }
                } else {
                    exponent -= fraction_len;
                }
                result = Fixed::from_bits(number << 16);
                scaling = exponent;
            }
        } else if (number / BCD_POWER_TENS[fraction_len as usize - 5]) > BCD_INTEGER_LIMIT {
            result = Fixed::from_bits(number)
                / Fixed::from_bits(BCD_POWER_TENS[fraction_len as usize - 4]);
            scaling = exponent - 4;
        } else {
            result = Fixed::from_bits(number)
                / Fixed::from_bits(BCD_POWER_TENS[fraction_len as usize - 5]);
            scaling = exponent - 5;
        }
        (Fixed::from_bits(result.to_bits() * self.sign), scaling)
    }
}

impl From<Fixed> for BcdComponents {
    fn from(value: Fixed) -> Self {
        Self {
            error: Some(value),
            ..Default::default()
        }
    }
}

/// Parse a fixed point value with a dynamic scaling factor.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/f1cd6dbfa0c98f352b698448f40ac27e8fb3832e/src/cff/cffparse.c#L580>
pub(crate) fn parse_fixed_dynamic(cursor: &mut Cursor) -> Result<(Fixed, i32), Error> {
    let b0 = cursor.read::<u8>()?;
    match b0 {
        30 => Ok(BcdComponents::parse(cursor)?.dynamically_scaled_value()),
        28 | 29 | 32..=254 => {
            let num = parse_int(cursor, b0)?;
            let mut int_len = 10;
            if num > BCD_INTEGER_LIMIT {
                for (i, power_ten) in BCD_POWER_TENS.iter().enumerate().skip(5) {
                    if num < *power_ten {
                        int_len = i;
                        break;
                    }
                }
                let scaling = if (num - BCD_POWER_TENS[int_len - 5]) > BCD_INTEGER_LIMIT {
                    int_len - 4
                } else {
                    int_len - 5
                };
                Ok((
                    Fixed::from_bits(num) / Fixed::from_bits(BCD_POWER_TENS[scaling]),
                    scaling as i32,
                ))
            } else {
                Ok((Fixed::from_bits(num << 16), 0))
            }
        }
        _ => Err(Error::InvalidNumber),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::FontData;

    #[test]
    fn int_operands() {
        // Test the boundary conditions of the ranged int operators
        let empty = FontData::new(&[]);
        let min_byte = FontData::new(&[0]);
        let max_byte = FontData::new(&[255]);
        // 32..=246 => -107..=107
        assert_eq!(parse_int(&mut empty.cursor(), 32).unwrap(), -107);
        assert_eq!(parse_int(&mut empty.cursor(), 246).unwrap(), 107);
        // 247..=250 => +108 to +1131
        assert_eq!(parse_int(&mut min_byte.cursor(), 247).unwrap(), 108);
        assert_eq!(parse_int(&mut max_byte.cursor(), 250).unwrap(), 1131);
        // 251..=254 => -1131 to -108
        assert_eq!(parse_int(&mut min_byte.cursor(), 251).unwrap(), -108);
        assert_eq!(parse_int(&mut max_byte.cursor(), 254).unwrap(), -1131);
    }

    #[test]
    fn binary_coded_decimal_operands() {
        // From <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-5-nibble-definitions>:
        //
        // "A real number is terminated by one (or two) 0xf nibbles so that it is always padded
        // to a full byte. Thus, the value -2.25 is encoded by the byte sequence (1e e2 a2 5f)
        // and the value 0.140541E-3 by the sequence (1e 0a 14 05 41 c3 ff)."
        //
        // The initial 1e byte in the examples above is the dictionary operator to trigger
        // parsing of BCD so it is dropped in the tests here.
        let bytes = FontData::new(&[0xe2, 0xa2, 0x5f]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .value(false),
            Fixed::from_f64(-2.25)
        );
        let bytes = FontData::new(&[0x0a, 0x14, 0x05, 0x41, 0xc3, 0xff]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .value(false),
            Fixed::from_f64(0.140541E-3)
        );
        // Check that we match FreeType for 375e-4.
        // Note: we used to parse 0.0375... but the new FT matching code
        // has less precision
        let bytes = FontData::new(&[0x37, 0x5c, 0x4f]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .value(false),
            Fixed::from_f64(0.0370025634765625)
        );
    }

    #[test]
    fn scaled_binary_coded_decimal_operands() {
        // For blue scale, we compute values with an internal factor of 1000 to match
        // FreeType, which gives us more precision for fractional bits
        let bytes = FontData::new(&[0xA, 0x06, 0x25, 0xf]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .value(true),
            Fixed::from_f64(0.0625)
        );
        // Just an additional check to test increased precision. Compare to
        // the test above where this value generates 0.0370...
        let bytes = FontData::new(&[0x37, 0x5c, 0x4f]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .value(true),
            Fixed::from_f64(0.037506103515625)
        );
    }

    #[test]
    fn dynamically_scaled_binary_coded_decimal_operands() {
        // 0.0625
        let bytes = FontData::new(&[0xA, 0x06, 0x25, 0xf]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .dynamically_scaled_value(),
            (Fixed::from_f64(6250.0), -5)
        );
        // 0.0375
        let bytes = FontData::new(&[0x37, 0x5c, 0x4f]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .dynamically_scaled_value(),
            (Fixed::from_f64(375.0), -4)
        );
        // .001953125
        let bytes = FontData::new(&[0xa0, 0x1, 0x95, 0x31, 0x25, 0xff]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .dynamically_scaled_value(),
            (Fixed::from_bits(1280000000), -7)
        );
    }

    /// See <https://github.com/googlefonts/fontations/issues/1617>
    #[test]
    fn blue_scale_fraction_length_of_0() {
        // 0.0037
        let bytes = FontData::new(&[0x37, 0xC3, 0xFF]);
        assert_eq!(
            BcdComponents::parse(&mut bytes.cursor())
                .unwrap()
                .value(true),
            Fixed::from_f64(0.0370025634765625)
        );
    }
}
