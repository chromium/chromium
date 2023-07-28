use std::collections::hash_map;
use std::fmt::{self, Display, Write};
use std::hash::{Hash, Hasher};
use syn::Ident;

// 8-character symbol hash consisting of a-zA-Z0-9. We use 8 character because
// Mach-O section specifiers are restricted to at most 16 characters (see
// https://github.com/dtolnay/linkme/issues/35) and we leave room for a
// linkme-specific prefix.
pub(crate) struct Symbol(u64);

pub(crate) fn hash(ident: &Ident) -> Symbol {
    let mut hasher = hash_map::DefaultHasher::new();
    ident.hash(&mut hasher);
    Symbol(hasher.finish())
}

impl Display for Symbol {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        // log(62^8)/log(2) is 47.6 so we have enough bits in the 64-bit
        // standard library hash to produce a good distribution over 8 digits
        // from a 62-character alphabet.
        let mut remainder = self.0;
        for _ in 0..8 {
            let digit = (remainder % 62) as u8;
            remainder /= 62;
            formatter.write_char(match digit {
                0..=25 => b'a' + digit,
                26..=51 => b'A' + digit - 26,
                52..=61 => b'0' + digit - 52,
                _ => unreachable!(),
            } as char)?;
        }
        Ok(())
    }
}

#[test]
fn test_hash() {
    let ident = Ident::new("EXAMPLE", proc_macro2::Span::call_site());
    assert_eq!(hash(&ident).to_string(), "0GPSzIoo");
}
