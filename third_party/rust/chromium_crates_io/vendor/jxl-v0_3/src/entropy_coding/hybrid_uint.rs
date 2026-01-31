// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::bit_reader::BitReader;
use crate::error::Error;

use crate::util::CeilLog2;

#[derive(Debug)]
pub struct HybridUint {
    split_token: u32,
    split_exponent: u32,
    msb_in_token: u32,
    lsb_in_token: u32,
}

impl HybridUint {
    pub(super) fn is_split_exponent_zero(&self) -> bool {
        self.split_exponent == 0
    }

    pub fn decode(log_alpha_size: usize, br: &mut BitReader) -> Result<HybridUint, Error> {
        let split_exponent = br.read((log_alpha_size + 1).ceil_log2())? as u32;
        let split_token = 1u32 << split_exponent;
        let msb_in_token;
        let lsb_in_token;
        if split_exponent != log_alpha_size as u32 {
            let nbits = (split_exponent + 1).ceil_log2() as usize;
            msb_in_token = br.read(nbits)? as u32;
            if msb_in_token > split_exponent {
                return Err(Error::InvalidUintConfig(split_exponent, msb_in_token, None));
            }
            let nbits = (split_exponent - msb_in_token + 1).ceil_log2() as usize;
            lsb_in_token = br.read(nbits)? as u32;
        } else {
            msb_in_token = 0;
            lsb_in_token = 0;
        }
        if lsb_in_token + msb_in_token > split_exponent {
            return Err(Error::InvalidUintConfig(
                split_exponent,
                msb_in_token,
                Some(lsb_in_token),
            ));
        }
        Ok(HybridUint {
            split_token,
            split_exponent,
            msb_in_token,
            lsb_in_token,
        })
    }

    #[inline]
    pub fn read(&self, symbol: u32, br: &mut BitReader) -> u32 {
        if symbol < self.split_token {
            return symbol;
        }
        let bits_in_token = self.lsb_in_token + self.msb_in_token;
        let nbits =
            self.split_exponent - bits_in_token + ((symbol - self.split_token) >> bits_in_token);
        // The bitstream is invalid if nbits >= 32. We do not report errors, and just pretend we
        // decoded a number <32.
        let nbits = nbits & 31;
        let low = symbol & ((1 << self.lsb_in_token) - 1);
        let symbol_nolow = symbol >> self.lsb_in_token;
        let bits = br.read_optimistic(nbits as usize) as u32;
        let hi = (symbol_nolow & ((1 << self.msb_in_token) - 1)) | (1 << self.msb_in_token);
        (((hi << nbits) | bits) << self.lsb_in_token) | low
    }
}

#[cfg(test)]
impl HybridUint {
    pub fn new(split_exponent: u32, msb_in_token: u32, lsb_in_token: u32) -> Self {
        Self {
            split_token: 1 << split_exponent,
            split_exponent,
            msb_in_token,
            lsb_in_token,
        }
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn test_hybrid_uint_decode_invalid() {
        use super::*;
        let mut br = BitReader::new(&[10, 75, 10, 75, 168, 139, 132, 255, 244]);
        br.skip_bits(1).unwrap();
        if let Ok(uint) = HybridUint::decode(15, &mut br) {
            uint.read(1022, &mut br);
        }
    }
}
