// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#![no_std]

#[cfg_attr(test, macro_use)]
extern crate alloc;

#[cfg(test)]
#[macro_use]
extern crate lazy_static;

use alloc::string::String;
use alloc::vec::Vec;

#[rustfmt::skip]
const BASE64URL_DECODE: [u8; 256] = [
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     62/*-*/, 255,     255,
    52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
    60/*8*/, 61/*9*/, 255,     255,     255,     255,     255,     255,
    255,     0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
    7/*H*/,  8/*I*/,  9/*J*/,  10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
    15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
    23/*X*/, 24/*Y*/, 25/*Z*/, 255,     255,     255,     255,     63/*_*/,
    255,     26/*a*/, 27/*b*/, 28/*c*/, 29/*d*/, 30/*e*/, 31/*f*/, 32/*g*/,
    33/*h*/, 34/*i*/, 35/*j*/, 36/*k*/, 37/*l*/, 38/*m*/, 39/*n*/, 40/*o*/,
    41/*p*/, 42/*q*/, 43/*r*/, 44/*s*/, 45/*t*/, 46/*u*/, 47/*v*/, 48/*w*/,
    49/*x*/, 50/*y*/, 51/*z*/, 255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
    255,     255,     255,     255,     255,     255,     255,     255,
];

#[rustfmt::skip]
const BASE64URL_ENCODE: [char; 64] = [
    'A',     'B',     'C',     'D',     'E',     'F',     'G',     'H',
    'I',     'J',     'K',     'L',     'M',     'N',     'O',     'P',
    'Q',     'R',     'S',     'T',     'U',     'V',     'W',     'X',
    'Y',     'Z',     'a',     'b',     'c',     'd',     'e',     'f',
    'g',     'h',     'i',     'j',     'k',     'l',     'm',     'n',
    'o',     'p',     'q',     'r',     's',     't',     'u',     'v',
    'w',     'x',     'y',     'z',     '0',     '1',     '2',     '3',
    '4',     '5',     '6',     '7',     '8',     '9',     '-',     '_',
];

/// Decode a Base64Url-encoded string into a vector of bytes.
/// Returns None if a decoding error occurs.
pub fn base64url_decode(encoded_data: &str) -> Option<Vec<u8>> {
    let mut output = Vec::with_capacity(encoded_data.len() / 4 * 3 + 2);
    let mut index = 0usize;

    loop {
        let todo = core::cmp::min(encoded_data.len() - index, 4);
        let mut lookups = [0u8; 4];
        for i in 0..todo {
            let ch = BASE64URL_DECODE[encoded_data.as_bytes()[index + i] as usize];
            if ch == 255 {
                return None;
            }
            lookups[i] = ch;
        }
        match todo {
            0 => {
                break;
            }
            4 => {
                output.push(lookups[0] << 2 | lookups[1] >> 4);
                output.push(lookups[1] << 4 | lookups[2] >> 2);
                output.push(lookups[2] << 6 | lookups[3]);
                index += 4;
            }
            3 => {
                output.push(lookups[0] << 2 | lookups[1] >> 4);
                output.push(lookups[1] << 4 | lookups[2] >> 2);
                break;
            }
            2 => {
                output.push(lookups[0] << 2 | lookups[1] >> 4);
                break;
            }
            1 => {
                // 6 leftover bits is invalid.
                return None;
            }
            _ => {
                panic!("Invalid return value from get_next_encoded_bytes");
            }
        }
    }
    Some(output)
}

// Base64Url-encodes a slice of bytes into a string.
pub fn base64url_encode(data: &[u8]) -> String {
    if data.len() > (usize::MAX - 3) / 4 * 3 {
        return String::new();
    }
    let mut output = String::with_capacity(data.len() / 3 * 4 + 3);
    let mut index = 0usize;
    while data.len() - index >= 3 {
        output.push(BASE64URL_ENCODE[(data[index] >> 2) as usize] as char);
        output.push(
            BASE64URL_ENCODE[((data[index] & 3) << 4 | data[index + 1] >> 4) as usize] as char,
        );
        output.push(
            BASE64URL_ENCODE[((data[index + 1] & 15) << 2 | data[index + 2] >> 6) as usize] as char,
        );
        output.push(BASE64URL_ENCODE[(data[index + 2] & 63) as usize] as char);
        index += 3;
    }

    if data.len() - index == 2 {
        output.push(BASE64URL_ENCODE[(data[index] >> 2) as usize] as char);
        output.push(
            BASE64URL_ENCODE[((data[index] & 3) << 4 | data[index + 1] >> 4) as usize] as char,
        );
        output.push(BASE64URL_ENCODE[((data[index + 1] & 15) << 2) as usize] as char);
    } else if data.len() - index == 1 {
        output.push(BASE64URL_ENCODE[(data[index] >> 2) as usize] as char);
        output.push(BASE64URL_ENCODE[((data[index] & 3) << 4) as usize] as char);
    }

    output
}

#[cfg(test)]
mod tests {
    use super::*;
    use alloc::string::ToString;

    lazy_static! {
        static ref TEST_CASES: Vec<(&'static str, &'static str)> = vec![
            ("123456", "MTIzNDU2"),
            ("123", "MTIz"),
            ("1234", "MTIzNA"),
            ("12345", "MTIzNDU"),
            ("?????>", "Pz8_Pz8-"),
        ];
    }

    #[test]
    fn decoding_test() {
        for test_case in &*TEST_CASES {
            assert_eq!(test_case.0.as_bytes(), &base64url_decode(&test_case.1).unwrap());
        }

        // =, + and / are valid Base64 but not Base64Url.
        assert_eq!(None, base64url_decode(&"InvalidChar="));
        assert_eq!(None, base64url_decode(&"InvalidChar+"));
        assert_eq!(None, base64url_decode(&"Whitespace "));
        assert_eq!(None, base64url_decode(&"InvalidChar/"));

        // Encoding is invalid where length % 4 == 1.
        assert_eq!(None, base64url_decode(&"12345"));

        assert_eq!("".as_bytes(), &base64url_decode(&"").unwrap());
    }

    #[test]
    fn encoding_test() {
        for test_case in &*TEST_CASES {
            assert_eq!(test_case.1.to_string(), base64url_encode(&test_case.0.as_bytes()));
        }

        assert_eq!("".to_string(), base64url_encode(&"".as_bytes()));
    }
}
