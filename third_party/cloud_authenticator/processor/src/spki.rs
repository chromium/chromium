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

//! spki implements minimal parsing of SubjectPublicKeyInfo structures.

use crate::der;

#[derive(Debug, PartialEq)]
#[allow(clippy::upper_case_acronyms)]
pub enum PublicKeyType {
    P256,
    RSA,
}

/// Parses a SubjectPublicKeyInfo, returning the type of the key and the public
/// key itself in a type-specific format.
pub fn parse(input: &[u8]) -> Option<(PublicKeyType, &[u8])> {
    const EC_KEY: &[u8] = b"\x2A\x86\x48\xCE\x3D\x02\x01";
    const RSA: &[u8] = b"\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01";
    const P256: &[u8] = b"\x2A\x86\x48\xCE\x3D\x03\x01\x07";

    // See https://datatracker.ietf.org/doc/html/rfc5280#section-4.1
    //
    // SubjectPublicKeyInfo  ::=  SEQUENCE  {
    //    algorithm            AlgorithmIdentifier,
    //    subjectPublicKey     BIT STRING
    // }

    let (top_level, input) = der::next_tagged(input, der::SEQUENCE)?;
    if !input.is_empty() {
        return None;
    }

    let (algo_id, top_level) = der::next_tagged(top_level, der::SEQUENCE)?;

    // https://datatracker.ietf.org/doc/html/rfc5280#section-4.1.1.2
    //
    // AlgorithmIdentifier  ::=  SEQUENCE  {
    //    algorithm               OBJECT IDENTIFIER,
    //    parameters              ANY DEFINED BY algorithm OPTIONAL
    // }

    let (oid, algo_id) = der::next_tagged(algo_id, der::OBJECT_IDENTIFIER)?;
    let key_type = if oid == EC_KEY {
        let (curve, algo_id) = der::next_tagged(algo_id, der::OBJECT_IDENTIFIER)?;
        if curve != P256 || !algo_id.is_empty() {
            return None;
        }
        PublicKeyType::P256
    } else if oid == RSA {
        let (none_body, algo_id) = der::next_tagged(algo_id, der::NONE)?;
        if !none_body.is_empty() || !algo_id.is_empty() {
            return None;
        }
        PublicKeyType::RSA
    } else {
        return None;
    };

    let (pubkey, top_level) = der::next_tagged(top_level, der::BIT_STRING)?;
    if pubkey.is_empty() || !top_level.is_empty() {
        return None;
    }

    // The first byte of a BIT STRING is the number of unused bits. This is
    // always zero for the key types that we deal with.
    let (unused_bits, pubkey) = der::u8_next(pubkey)?;
    if unused_bits != 0 {
        return None;
    }

    Some((key_type, pubkey))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_p256() {
        const P256_SPKI : &[u8] = b"\x30\x59\x30\x13\x06\x07\x2A\x86\x48\xCE\x3D\x02\x01\x06\x08\x2A\x86\x48\xCE\x3D\x03\x01\x07\x03\x42\x00\x04\xC0\xA2\x04\xCF\xE4\x2C\x4B\xA8\x75\xC5\x2D\x8F\x99\x6B\xEB\xA3\x1B\x80\x6E\x4B\x39\xEE\xA2\x30\x37\x34\x4D\x08\xB2\xA8\xF9\xCC\xD8\xED\x65\xC4\xC0\x08\x42\x83\x2D\xE3\x18\x7A\x7C\x4C\xCE\xF2\xFA\xE1\xF6\x1C\x09\x6C\x50\xFE\xC9\xA9\x21\x64\x43\x96\x2B\x65";

        let Some((p256_type, p256_key)) = parse(P256_SPKI) else {
            panic!("failed to parse P-256 SPKI");
        };
        assert_eq!(p256_type, PublicKeyType::P256);
        assert_eq!(p256_key.len(), 65);
        assert_eq!(p256_key[0], 0x04);
    }

    #[test]
    fn test_parse_rsa() {
        const RSA_SPKI : &[u8] = b"\x30\x82\x01\x22\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01\x05\x00\x03\x82\x01\x0F\x00\x30\x82\x01\x0A\x02\x82\x01\x01\x00\xC8\xCE\xF7\xBC\x8E\x91\xDB\x52\xC0\x49\xD5\x87\xAA\xB2\x2E\x4E\x04\xAC\xA1\xB7\xA3\x88\x8D\x18\xC8\x5D\x89\x74\x3F\xA8\x7C\x8B\xD2\xBA\xB8\xC3\x84\x6A\x36\x52\xFC\x3A\x47\x8A\x01\xD5\x2E\xEE\xA1\x9D\xFB\x62\xCE\xEA\x97\x65\x9E\xF8\xAF\xED\x94\xF0\x56\x6E\xB6\x43\xBB\xFA\x3B\x05\xFF\x1D\xE7\x9E\x68\xAF\x90\x25\x7C\x18\x83\x1B\xE3\x5E\x7E\x48\x8A\x50\xF5\xAD\xC1\x8F\x2B\x46\xDE\x2E\xE9\x88\x3B\x4E\x1C\xC1\x8F\xF1\xB5\x3E\x34\x8F\xD0\xCA\x13\x01\x39\x34\xB8\x16\x4F\x68\xB9\xF4\xED\xBF\x6F\xE8\x3A\xFE\x26\xE8\x7F\xB8\x22\x90\xC6\xF1\x30\xF5\x4D\x6E\x73\xDA\xA3\x3D\x8C\xC6\xA3\x41\x58\xDB\xA4\x5C\x5D\xEE\x1C\x28\xC5\x63\xF5\x08\xC4\x58\x26\x49\xA8\x7D\x11\x98\x06\x71\xB2\x9C\x65\xB8\x13\x2A\x93\xE1\x1A\xB9\x03\x86\x7E\xE1\x9E\x9A\xF5\xC0\x15\xB1\x20\x73\x12\x0E\x85\x0A\x46\x43\x4F\x2B\x92\xBE\xEF\xF1\xB2\x49\xB1\xF6\xE4\xF5\x26\x9A\x48\x6E\x41\x9F\x84\xA3\xD9\x45\xD1\x4F\x28\xE2\xEC\xD2\xFC\x76\x06\x45\x18\xE8\xBB\xC0\x99\x3F\x75\x49\x61\x0F\x94\x46\xB6\x80\x36\x5C\x69\xD4\x82\xAA\x77\xB3\xB3\x00\xE4\x60\xAB\x47\x02\x03\x01\x00\x01";

        let Some((rsa_type, rsa_key)) = parse(RSA_SPKI) else {
            panic!("failed to parse P-256 SPKI");
        };
        assert_eq!(rsa_type, PublicKeyType::RSA);
        assert_eq!(rsa_key.len(), 270);
        assert_eq!(rsa_key[0], 0x30);
    }
}
