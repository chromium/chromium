//! T.81 H.2 lossless residual contexts over the shared QM probability engine.

use zune_core::bytestream::{ZByteReaderTrait, ZReader};

use crate::bitstream::BitStream;
use crate::bitstream_arith::{ArithDCTables, BitStreamArithmetic, StatisticsEntry};
use crate::errors::DecodeErrors;
use crate::marker::Marker;

#[derive(Clone, Copy, Default)]
struct Leading {
    nonzero:  StatisticsEntry,
    sign:     StatisticsEntry,
    positive: StatisticsEntry,
    negative: StatisticsEntry
}

#[derive(Clone, Copy, Default)]
struct Magnitude {
    size: [StatisticsEntry; 15],
    bits: [StatisticsEntry; 14]
}

#[derive(Clone, Copy, Default)]
struct Context {
    leading: [[Leading; 5]; 5],
    low:     Magnitude,
    high:    Magnitude
}

pub(crate) struct LosslessArithmetic {
    coder:    BitStreamArithmetic,
    contexts: [Context; 4]
}

fn classify(difference: i32, lower: u8, upper: u8) -> usize {
    let magnitude = difference.unsigned_abs();
    if magnitude <= (1u32 << lower) >> 1 {
        2
    } else if magnitude <= 1u32 << upper {
        if difference < 0 {
            1
        } else {
            3
        }
    } else if difference < 0 {
        0
    } else {
        4
    }
}

impl LosslessArithmetic {
    pub(crate) fn new() -> Self {
        Self {
            coder:    BitStreamArithmetic::new(),
            contexts: [Context::default(); 4]
        }
    }

    pub(crate) fn restart(&mut self) {
        self.coder = BitStreamArithmetic::new();
        self.contexts.fill(Context::default());
    }

    pub(crate) fn difference<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>, table: usize, conditioning: &ArithDCTables, left: i32,
        above: i32
    ) -> Result<i32, DecodeErrors> {
        let context = self
            .contexts
            .get_mut(table)
            .ok_or(DecodeErrors::FormatStatic(
                "Invalid lossless arithmetic conditioning selector"
            ))?;
        let a = classify(left, conditioning.l, conditioning.u);
        let b = classify(above, conditioning.l, conditioning.u);
        let leading = &mut context.leading[a][b];
        let value = if self.coder.decode_bit(&mut leading.nonzero, reader)? == 0 {
            0
        } else {
            let negative = self.coder.decode_bit(&mut leading.sign, reader)? != 0;
            let first = if negative { &mut leading.negative } else { &mut leading.positive };
            let magnitude = if self.coder.decode_bit(first, reader)? == 0 {
                1
            } else {
                let contexts = if above.unsigned_abs() > 1u32 << conditioning.u {
                    &mut context.high
                } else {
                    &mut context.low
                };
                let mut size = 0;
                while self.coder.decode_bit(&mut contexts.size[size], reader)? != 0 {
                    size += 1;
                    if size == contexts.size.len() {
                        return Err(DecodeErrors::FormatStatic(
                            "Lossless arithmetic residual overflow"
                        ));
                    }
                }
                let mut magnitude = 1i32 << size;
                if size != 0 {
                    for bit in (0..size).rev() {
                        magnitude |= i32::from(
                            self.coder
                                .decode_bit(&mut contexts.bits[size - 1], reader)?
                        ) << bit;
                    }
                }
                magnitude + 1
            };
            if negative {
                -magnitude
            } else {
                magnitude
            }
        };
        if matches!(self.coder.marker(), Some(marker) if !matches!(marker, Marker::RST(_) | Marker::EOI))
        {
            return Err(DecodeErrors::FormatStatic(
                "Unexpected marker in lossless arithmetic scan"
            ));
        }
        Ok(value)
    }

    pub(crate) fn marker<T: ZByteReaderTrait>(
        &mut self, reader: &mut ZReader<T>
    ) -> Result<Marker, DecodeErrors> {
        if let Some(marker) = self.coder.marker().take() {
            return Ok(marker);
        }
        // QM termination can leave a bounded suffix of unused entropy bytes.
        for _ in 0..4 {
            if reader.read_u8_err()? != 0xff {
                continue;
            }
            let mut byte = reader.read_u8_err()?;
            while byte == 0xff {
                byte = reader.read_u8_err()?;
            }
            if byte != 0 {
                return Marker::from_u8(byte)
                    .filter(|marker| !matches!(marker, Marker::UNKNOWN(_)))
                    .ok_or(DecodeErrors::FormatStatic(
                        "Invalid lossless arithmetic marker"
                    ));
            }
        }
        Err(DecodeErrors::FormatStatic(
            "Excess entropy after lossless arithmetic scan"
        ))
    }
}
