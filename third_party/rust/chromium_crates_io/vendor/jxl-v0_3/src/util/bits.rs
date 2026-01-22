// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

pub fn value_of_lowest_1_bit(t: u32) -> u32 {
    t & t.wrapping_neg()
}
#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_value_of_lowest_1_bit() {
        assert_eq!(value_of_lowest_1_bit(0b0001), 1);
        assert_eq!(value_of_lowest_1_bit(0b1111), 1);
        assert_eq!(value_of_lowest_1_bit(0b0010), 2);
        assert_eq!(value_of_lowest_1_bit(0b0100), 4);
        assert_eq!(value_of_lowest_1_bit(0b1010), 2);
        assert_eq!(value_of_lowest_1_bit(0b1000_0000), 128);
        assert_eq!(value_of_lowest_1_bit(0), 0);
    }
}
