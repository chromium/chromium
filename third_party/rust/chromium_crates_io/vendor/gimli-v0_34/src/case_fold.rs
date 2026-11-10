include!("case_fold_data.rs");

fn case_fold_data(c: char) -> char {
    match CASE_FOLD_DATA.binary_search_by(|&(key, _)| key.cmp(&c)) {
        Ok(i) => CASE_FOLD_DATA[i].1,
        Err(_) => c,
    }
}

/// Perform case folding for the DWARF name index hashing.
///
/// This implements the case folding specified in DWARF 5 Section 6.1.1.4.5.
///
/// "The simple case folding algorithm is further described in the CaseFolding.txt file
/// distributed with the Unicode Character Database. That file defines four classes of
/// mappings: Common (C), Simple (S), Full (F), and Turkish (T). The hash
/// computation specified here uses the C + S mappings only, which do not affect the
/// total length of the string, with the addition that Turkish upper case dotted ’İ’ and
/// lower case dotless ’ı’ are folded to the Latin lower case ’i’."
pub fn case_fold(c: char) -> char {
    if c.is_ascii() {
        (c as u8).to_ascii_lowercase() as char
    } else {
        case_fold_data(c)
    }
}

/// Calculate a case folding DJB hash for the DWARF name index.
///
/// This uses the case folding specified in DWARF 5 Section 6.1.1.4.5
/// with the DJB hash specified in DWARF 5 Section 7.33.
pub fn case_folding_djb_hash(s: &str) -> u32 {
    let mut hash: u32 = 5381;
    for c in s.chars() {
        if c.is_ascii() {
            let byte = (c as u8).to_ascii_lowercase();
            hash = djb_hash_byte(hash, byte);
        } else {
            let c = case_fold_data(c);
            let mut bytes = [0; 4];
            for byte in c.encode_utf8(&mut bytes).as_bytes() {
                hash = djb_hash_byte(hash, *byte);
            }
        }
    }
    hash
}

#[inline]
fn djb_hash_byte(hash: u32, byte: u8) -> u32 {
    hash.wrapping_mul(33).wrapping_add(u32::from(byte))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_case_fold() {
        for (c, fold) in [
            ('A', 'a'), // ASCII
            ('1', '1'), // Identity
            ('Σ', 'σ'), // Greek
            ('K', 'k'), // Kelvin
            ('I', 'i'), // Latin capital letter I
            // DWARF extension to simple case folding: Turkish upper case dotted ’İ’
            // and lower case dotless ’ı’ are folded to the Latin lower case ’i’
            ('İ', 'i'),
            ('ı', 'i'),
        ] {
            assert_eq!(case_fold(c), fold);
            assert_eq!(case_fold(fold), fold);
        }
    }

    #[test]
    fn test_case_folding_djb_hash() {
        assert_eq!(case_folding_djb_hash(""), 5381);
        // Test case copied from LLVM
        let s = "İıÀàĀāĹĺЕеẦầKkⰝⱍＭｍ𐲒𐳒";
        assert_eq!(case_folding_djb_hash(s), 1145571043);
    }
}
