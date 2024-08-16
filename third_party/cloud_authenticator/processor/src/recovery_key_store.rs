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

//! recovery_key_store contains functions for working with Google's
//! recovery key store, also called Vault internally.

use crate::{
    debug, get_secret_from_request, pin, Authentication, DirtyFlag, ParsedState, Reauth,
    RequestError, COUNTER_ID_KEY, VAULT_HANDLE_WITHOUT_TYPE_KEY, WRAPPED_PIN_DATA_KEY,
};
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use bytes::Bytes;
use cbor::{cbor, MapKey, MapKeyRef, MapLookupKey, Value};

const SECONDS_IN_A_DAY: i64 = 86400;

/// The public keys of the recovery key store cohorts are published in
/// XML files. This module implements a basic XML parser.
mod xml {
    use alloc::boxed::Box;
    use alloc::collections::btree_map::Entry;
    use alloc::collections::BTreeMap;
    use alloc::string::String;
    use alloc::vec;
    use alloc::vec::Vec;
    use core::iter;

    /// XML values either contain string content or other tags. This parser
    /// doesn't support mixing them like HTML.
    #[derive(Debug, PartialEq, Eq)]
    pub enum Value {
        String(String),
        Object(BTreeMap<String, Element>),
    }

    impl Default for Value {
        fn default() -> Self {
            Value::String(String::new())
        }
    }

    // The maximum supported nesting of XML tags.
    const MAX_DEPTH: i32 = 16;

    /// Each XML element is either a single value or a list of them. E.g.
    /// in this XML:
    ///
    /// <pantry>
    ///   <food>carrot</food>
    /// </pantry>
    ///
    /// The <pantry> value contains a `food` element that would be `Single`.
    /// But if multiple foods are listed then `food` would be a `List`:
    ///
    /// <pantry>
    ///   <food>carrot</food>
    ///   <food>cheese</food>
    /// </pantry>
    #[derive(Debug, PartialEq, Eq)]
    pub enum Element {
        Single(Value),
        List(Vec<Value>),
    }

    impl Element {
        /// iter iterates over the values of the `Element`, whether there be
        /// one or several.
        pub fn iter(&self) -> Box<dyn iter::Iterator<Item = &Value> + '_> {
            match self {
                Element::Single(value) => Box::new(iter::once(value)),
                Element::List(values) => Box::new(values.iter()),
            }
        }
    }

    /// A ByteReader produces bytes one at a time and supports undoing a read.
    /// While an arbitrary number of bytes can be unread, the XML parser only
    /// requires a single byte lookahead.
    struct ByteReader<'a> {
        data: &'a [u8],
        offset: usize,
    }

    impl ByteReader<'_> {
        fn next(&mut self) -> Option<u8> {
            if self.offset >= self.data.len() {
                None
            } else {
                let ret = Some(self.data[self.offset]);
                self.offset += 1;
                ret
            }
        }

        fn unread(&mut self) {
            if self.offset > 0 {
                self.offset -= 1;
            }
        }
    }

    fn is_whitespace(c: u8) -> bool {
        // https://www.w3.org/TR/xml/#NT-S
        matches!(c, b' ' | b'\r' | b'\n' | b'\t')
    }

    fn is_name_start(c: u8) -> bool {
        // https://www.w3.org/TR/xml/#NT-NameStartChar
        matches!(c, b':' | b'_' | b'a'..=b'z' | b'A'..=b'Z')
    }

    fn is_name_char(c: u8) -> bool {
        // https://www.w3.org/TR/xml/#NT-NameChar
        if is_name_start(c) { true } else { matches!(c, b'-' | b'.' | b'0'..=b'9') }
    }

    struct Parser<'a> {
        reader: ByteReader<'a>,
    }

    impl Parser<'_> {
        /// Parse a complete XML document.
        fn top_level(&mut self) -> Option<(String, Value)> {
            self.skip_whitespace();
            self.match_header()?;
            self.match_char(b'<')?;
            self.parse_tag(/* depth= */ 1)
        }

        /// Discard bytes until `limit` returns true, then unread the final
        /// byte.
        fn skip_until<F>(&mut self, limit: F)
        where
            F: Fn(u8) -> bool,
        {
            loop {
                match self.reader.next() {
                    None => {
                        return;
                    }
                    Some(byte) => {
                        if limit(byte) {
                            self.reader.unread();
                            return;
                        }
                    }
                }
            }
        }

        fn skip_whitespace(&mut self) {
            self.skip_until(|c| !is_whitespace(c))
        }

        /// Skip an XML attribute within a tag.
        fn skip_attribute(&mut self) -> Option<()> {
            // https://www.w3.org/TR/xml/#NT-Attribute
            self.skip_until(|c| !is_name_char(c));
            self.skip_whitespace();
            self.match_char(b'=')?;
            self.skip_whitespace();
            let delim = self.reader.next()?;
            if delim != b'"' && delim != b'\'' {
                return None;
            }
            self.skip_until(|c| c == delim);
            self.match_char(delim)?;
            self.skip_whitespace();
            Some(())
        }

        /// Skip a series of XML attributes within a tag.
        fn skip_attributes(&mut self) -> Option<()> {
            loop {
                let Some(byte) = self.reader.next() else {
                    return Some(());
                };
                if !is_name_start(byte) {
                    self.reader.unread();
                    return Some(());
                }
                self.skip_attribute()?;
            }
        }

        /// Exactly match a single byte.
        fn match_char(&mut self, c: u8) -> Option<()> {
            match self.reader.next() {
                None => None,
                Some(byte) => {
                    if byte == c {
                        Some(())
                    } else {
                        None
                    }
                }
            }
        }

        /// Exactly match the given string.
        fn match_string(&mut self, s: &str) -> Option<()> {
            for c in s.bytes() {
                self.match_char(c)?;
            }
            Some(())
        }

        /// Extract a string made from all the bytes until either EOF or until
        /// `check` returns false. If `check` returns false, that byte is
        /// unread.
        fn string_while<F>(&mut self, check: F) -> Option<String>
        where
            F: Fn(u8) -> bool,
        {
            let mut vec = Vec::new();
            loop {
                let Some(byte) = self.reader.next() else {
                    break;
                };
                if !check(byte) {
                    self.reader.unread();
                    break;
                }
                vec.push(byte);
            }
            String::from_utf8(vec).ok()
        }

        /// Match an XML "header" (called a text declaration in the XML spec).
        fn match_header(&mut self) -> Option<()> {
            // https://www.w3.org/TR/xml/#NT-TextDecl
            self.match_string("<?xml")?;
            self.skip_whitespace();
            self.skip_attributes()?;
            self.match_string("?>")?;
            self.skip_whitespace();
            Some(())
        }

        /// Parse a tag. This expects to be positioned immediately after the
        /// opening '<'.
        fn parse_tag(&mut self, depth: i32) -> Option<(String, Value)> {
            if depth > MAX_DEPTH {
                return None;
            }

            self.skip_whitespace();
            let tag = self.string_while(is_name_char)?;
            self.skip_whitespace();
            self.skip_attributes();
            self.match_char(b'>')?;
            self.skip_whitespace();

            let mut dict = BTreeMap::new();
            loop {
                let byte = self.reader.next()?;
                if byte == b'<' {
                    let byte = self.reader.next()?;
                    if byte == b'/' {
                        // This is the closing tag.
                        self.skip_whitespace();
                        self.match_string(&tag)?;
                        self.skip_whitespace();
                        self.match_char(b'>')?;
                        self.skip_whitespace();
                        return Some((tag, Value::Object(dict)));
                    }

                    // This is a another tag within this tag.
                    self.reader.unread();
                    let (subtag, subvalue) = self.parse_tag(depth + 1)?;

                    match dict.entry(subtag) {
                        Entry::Vacant(entry) => {
                            entry.insert(Element::Single(subvalue));
                        }
                        Entry::Occupied(mut entry) => {
                            let existing = entry.get_mut();
                            let mut new = match existing {
                                Element::Single(prev_value) => {
                                    // This subtag has been seen once before. Now there are two
                                    // values.
                                    Element::List(vec![core::mem::take(prev_value), subvalue])
                                }
                                Element::List(prev_values) => {
                                    // This subtag has been seen more than once before. Append the
                                    // new value to the list.
                                    prev_values.push(subvalue);
                                    Element::List(core::mem::take(prev_values))
                                }
                            };
                            core::mem::swap(existing, &mut new);
                        }
                    }
                } else if dict.is_empty() {
                    // This tag contains string content.
                    self.reader.unread();
                    let contents = self.string_while(|c| c != b'<')?;
                    self.match_string("</")?;
                    self.skip_whitespace();
                    self.match_string(&tag)?;
                    self.skip_whitespace();
                    self.match_char(b'>')?;
                    self.skip_whitespace();
                    return Some((tag, Value::String(String::from(contents.trim()))));
                } else {
                    // This tag contains both tags and string content, which isn't supported.
                    return None;
                }
            }
        }
    }

    /// Parse an XML document.
    pub fn parse(xml: &[u8]) -> Option<(String, Value)> {
        Parser { reader: ByteReader { data: xml, offset: 0 } }.top_level()
    }

    #[cfg(test)]
    mod tests {
        use super::Value::Object;
        use super::*;
        use crate::recovery_key_store::xml::Element::{List, Single};

        #[test]
        fn test_parse() {
            let expected = (
                String::from("certificate"),
                Object(BTreeMap::from([
                    (
                        String::from("endpoints"),
                        Single(Object(BTreeMap::from([(
                            String::from("cert"),
                            List(vec![
                                Value::String(String::from(
                                    "MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABMCD3sSR26q9occ1Y/K2SQyIsSJkJtGALvd3t4l9E8ajmOV9fQHp7d4ExmRJIldlFL/Y5i5FBg3NvwK7TLvoAPmjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAD7HLz0sS04rV7BXzrd2KJdMk2fCbrjTPNNUUZu+UbPB0lDvWcP1+uroIOEZuPLUK0EBbQYzCjP/bp7tT4me4myivPbg2IBLvTaOVKbUzi6SqA4X+vyAe3c7Bp6A3hPzxNangk2jmpKdIvLXJ8DHyXVrCXk/dNObnWUDnvbmoXg5yWK/snB5OIysDPUlxUmRspxhRajVgRnDAMTnJ2YZhHC15Jm/neugxVKeSeBb4wamLRibkdWbc4KJTiSjh1CnH1OKsCI8N006Gk+YXHnrY3OmakVg/bSnfAoMWLMDvtXbDbMVYAl9uRLBDwoOS6MFMsrj+Iwniuv4E2Kb+UcWK36AR/KH1/ILFpRUTtfPwIQcvEc2tWkH+W2BJqKOvwGH3rOm2qF88g8/egrHua7jnv8aJlfQ3c3S7ytikxugCQhSAJhVO0kdWXGUut78UzBrhMEvBqHlQtZnyPSEWd6bJKdGqwmbQwdKoou5HCu0YQxanmzENR9PmDs6+AMN0xJDcb9TOBQsvQW+vY3D34U61izaU2xytglgRzjSlBwFYDP75VgsL9gcNlYSt9R1EroPPsaEV1xhW47WpWArLdprVhVX70kPf3fUkcpDXimapFpMWONWlSUCIKPy/q0d2DcamL9HN5sZLyOGPctMTEowPomW8TiISWJFdtSK2fJXkk8s",
                                )),
                                Value::String(String::from(
                                    "MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABOHSWq/RFpU1VnCCCmPcTDeJT3t3+27+BjFOdsC8/hcnbFUKwHt6Tt0uiHV3LP/aO0/DHYC8Kdb/KAMC+ai+aJ2jEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBALz6PK44f46capH7isFvHMdTosG3DIV4QP70zLtGtGBM+57RKU0UYLtgdtKfCCwQVIgru9PfMdNdbxKojI96cfB/QxsH5H/96iUET+EnvvQ63NMSnLtOp7H4UceBujpXeSLN0yRNr59JS+mLtyL5+5KjHgtOM7tpxJ3eP1tx8NnE30TE0BoeTQyoKu0wfHVsc5+Fs3EWJUpgV+Z0/KJFoy3M2Z0DHZxfn6fg+/xYxn8ttkMhlZXhJMjNqtcGmlwLYktmsG5LlsQNimXwGl9olVviEZwcHGUzHw8QWszoKzn+TgTgv76m2eZ5MwJeN1JnaLb+1gQtgKRpnG8TFxWGC/TIHUqLow/GruH2TSlLPr6l6ed+QjG01sAN5cdI7OR84D8W1F0vb8fVOr7kjf7N3qLDNQXDCRUUKHlRVanIt6h+kT1ctlM51+QmRhDsAkzY/3lFrXDySnQk18vlzTyA+QgqmvfNkPhgCp/fpgtWJFaPL9bJWaMaW/soXRUf26F6RMLK43EihdoVMtUAvmCIKUQyI88X6hJxEhWLyy/8Y45nAFk5CgXuzV2doOJTSITtJligTy1IuczH75bmp87c5ZPp51vUO4WYXuwffTCoQ8UYSYbNxxqKOfFkILnM1WoGAzCrVt5aKOyGPILzOsOS8X0EeQ9YF6Mvaf2iFljc2o30",
                                )),
                                Value::String(String::from(
                                    "MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABNeVqPpEctoVzN48WNefTpJEmRrrbpXoWRhHwH/AOYmQgXR6xX/AE1/qeen8fMj4Lnyb8KPveZjXvTlFq2mdBHGjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAEQIGwhKa7MDq+Wt5p7fvv1AXhX4HxpgkKv5xbuMWCcw6R8zTYQ4hF/XHegIEqjmwWFxEvD95Lu3oLz4gMEoZVywBt2QFb1wkWUjdeT9oy5YbrJiLm9evhMFWyjnu2h9OVqxCVvarVx35ZySThDr2n3CYntLSKyTSdVlzCsdcCOj1UFkqMe73gOUZFMkXETUoINlFYwX6NP5V1Moy8OjsSNa6/8zyYwivm3rQlj3GUEhSlX+0ib+IXYpcrDFF7/6+G8lWBAHmKGwGR6kpAQ7Zg7KEjY0gSYWOr86oJIMFzeXVjaqhwGXK2tO+JBTPZSf4zljke+QCDN1uZjscgpOOXcBvT3LqLDaz2TSen4EMXhD56lYrq/970a1ol7B26nNAjJr1Q2ZyH4kXgBnK/b7AjYzNhTx0k0o7zRdh4tMeNkxhHgpBQ7d8VM81lZJg95n5SuOvJkJlEsPus9nJ1QeKAAjLV+Hp4n+xEImnvwnPEeE9vo07KHeHsCaBFVVan+9VKMiFEnYO+JdA8DwVTwTHHRH2T2OcEF+oo6m9nZZgGZbcovftryoOetJRY8E2JG+j5ScVWwnh5QcWhP1oOqsZdFWbKmJyxbN0qhKRWB1l6xZipMTj4RYzrZtwXNWdJIudC1Lkr6GgMn2UybLPc4xDH5FLWDtLN7griLweFrniuAQ",
                                )),
                            ]),
                        )]))),
                    ),
                    (
                        String::from("intermediates"),
                        Single(Object(BTreeMap::from([(
                            String::from("cert"),
                            List(vec![
                                Value::String(String::from(
                                    "MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==",
                                )),
                                Value::String(String::from(
                                    "MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=",
                                )),
                            ]),
                        )]))),
                    ),
                    (
                        String::from("metadata"),
                        Single(Object(BTreeMap::from([
                            (
                                String::from("creation-time"),
                                Single(Value::String(String::from("1694037058"))),
                            ),
                            (
                                String::from("previous"),
                                Single(Object(BTreeMap::from([
                                    (
                                        String::from("hash"),
                                        Single(Value::String(String::from(
                                            "TQudrujnu1I9bdoDaYxGQYuRN/8SwTLjdk6vzYTOkIU=",
                                        ))),
                                    ),
                                    (
                                        String::from("serial"),
                                        Single(Value::String(String::from("10015"))),
                                    ),
                                ]))),
                            ),
                            (
                                String::from("refresh-interval"),
                                Single(Value::String(String::from("2592000"))),
                            ),
                            (String::from("serial"), Single(Value::String(String::from("10016")))),
                        ]))),
                    ),
                ])),
            );
            assert_eq!(expected, parse(crate::recovery_key_store::SAMPLE_CERTS_XML).unwrap());
        }

        #[test]
        fn test_parse_with_attributes() {
            // `SAMPLE_CERTS_XML` doesn't have any attributes in the tags so
            // this test checks that they are ignored.
            const XML: &[u8] = br#"<?xml  ?>< a  foo="true" bar='false' >b</ a >"#;
            assert_eq!((String::from("a"), Value::String(String::from("b"))), parse(XML).unwrap());
        }

        #[test]
        fn test_parse_with_mixed_tags_and_strings() {
            // This parser doesn't support mixing textual contents with string contents in
            // the same tag.
            const XML: &[u8] = br#"<?xml?><a>b<tag></tag></a>"#;
            assert!(parse(XML).is_none());
        }

        #[test]
        fn test_max_depth() {
            // The nests in this example exceeds `MAX_DEPTH`.
            const XML: &[u8] = br#"<?xml?>
                                   <a><a><a><a><a><a><a><a><a><a><a><a><a><a><a><a><a><a>
                                   </a></a></a></a></a></a></a></a></a></a></a></a></a></a></a></a></a></a>"#;
            assert!(parse(XML).is_none());
        }
    }
}

/// The x509 module provides basic X.509 support since the recovery key store
/// publishes its key in X.509 format.
mod x509 {
    use super::SECONDS_IN_A_DAY;
    use crate::der;
    use alloc::string::String;

    /// The parsed form of an X.509 certificate.
    #[derive(Debug, PartialEq)]
    pub struct Certificate<'a> {
        /// The full DER contents of the certificate.
        pub der: &'a [u8],
        /// The DER bytes, including the header, of the subject of the
        /// certificate.
        pub subject: &'a [u8],
        /// The DER bytes, including the header, of the issuer of the
        /// certificate.
        pub issuer: &'a [u8],
        /// The time before which the certificate is invalid. This is in seconds
        /// since the UNIX epoch, but only accurate to 24 hrs.
        pub not_before_approx_epoch_seconds: i64,
        /// The time after which the certificate is invalid. This is in seconds
        /// since the UNIX epoch, but only accurate to 24 hrs.
        pub not_after_approx_epoch_seconds: i64,
        /// The SubjectPublicKeyInfo from the certificate (including DER
        /// header).
        pub spki: &'a [u8],
        /// The signature by the issuer.
        pub signature: &'a [u8],
        /// The object identifier bytes (not including the header) of the
        /// signature algorithm used to sign this certificate.
        pub sig_algo_oid: &'a [u8],
        /// True if this is a CA certificate.
        pub is_ca: bool,
        /// The body of the certificate that is signed.
        pub to_be_signed: &'a [u8],
    }

    /// Parse a DER-encoded, X.509 certificate.
    pub fn parse(input: &[u8]) -> Option<Certificate> {
        // https://datatracker.ietf.org/doc/html/rfc5280#section-4.1
        let (top_level, empty) = der::next_tagged(input, der::SEQUENCE)?;
        if !empty.is_empty() {
            return None;
        }
        let (der::SEQUENCE, header_len, to_be_signed, top_level) = der::next_element(top_level)?
        else {
            return None;
        };
        let tbs = &to_be_signed[header_len..];
        let (_outer_sig_algo, top_level) = der::next_tagged(top_level, der::SEQUENCE)?;
        let (sig_bit_string, empty) = der::next_tagged(top_level, der::BIT_STRING)?;
        if !empty.is_empty() {
            return None;
        }
        // The first byte of a BIT STRING is the number of unused bits. This is
        // always zero for the key types that we deal with.
        let (unused_bits, signature) = der::u8_next(sig_bit_string)?;
        if unused_bits != 0 {
            return None;
        }

        let (_version, tbs) = der::next_optional(tbs, der::CONTEXT_SPECIFIC | der::CONSTRUCTED)?;
        let (_serial_number, tbs) = der::next_tagged(tbs, der::INTEGER)?;
        let (sig_algo, tbs) = der::next_tagged(tbs, der::SEQUENCE)?;
        let (sig_algo_oid, _) = der::next_tagged(sig_algo, der::OBJECT_IDENTIFIER)?;
        let (der::SEQUENCE, _, issuer, tbs) = der::next_element(tbs)? else {
            return None;
        };
        let (validity, tbs) = der::next_tagged(tbs, der::SEQUENCE)?;
        let (der::SEQUENCE, _, subject, tbs) = der::next_element(tbs)? else {
            return None;
        };
        let (der::SEQUENCE, _, spki, tbs) = der::next_element(tbs)? else {
            return None;
        };
        let (_issuer_unique_id, tbs) =
            der::next_optional(tbs, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1)?;
        let (_subject_unique_id, tbs) =
            der::next_optional(tbs, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 2)?;
        let (extensions, _tbs) =
            der::next_optional(tbs, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 3)?;
        // Future versions of X.509 may add further fields so `_tbs` might not be empty
        // at this point.

        let mut is_ca = false;
        if let Some(extensions) = extensions {
            let (mut extensions, empty) = der::next_tagged(extensions, der::SEQUENCE)?;
            if !empty.is_empty() {
                return None;
            }
            while !extensions.is_empty() {
                let (extension, rest) = der::next_tagged(extensions, der::SEQUENCE)?;
                extensions = rest;

                let (id, rest) = der::next_tagged(extension, der::OBJECT_IDENTIFIER)?;
                let (_critical, rest) = der::next_optional(rest, der::BOOLEAN)?;
                let (value, rest) = der::next_tagged(rest, der::OCTET_STRING)?;
                if !rest.is_empty() {
                    return None;
                }

                if id == b"\x55\x1d\x13" {
                    // https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1.9
                    let (basic_constraints, empty) = der::next_tagged(value, der::SEQUENCE)?;
                    if !empty.is_empty() {
                        return None;
                    }
                    let (is_ca_bool, _basic_constraints) =
                        der::next_optional(basic_constraints, der::BOOLEAN)?;
                    if let Some(b"\xff") = is_ca_bool {
                        is_ca = true;
                    }
                }
            }
        }

        let (not_before, rest) = parse_time(validity)?;
        let (not_after, empty) = parse_time(rest)?;
        if !empty.is_empty() {
            return None;
        }

        Some(Certificate {
            der: input,
            subject,
            issuer,
            not_before_approx_epoch_seconds: not_before,
            not_after_approx_epoch_seconds: not_after,
            spki,
            signature,
            sig_algo_oid,
            is_ca,
            to_be_signed,
        })
    }

    /// Parse a PKIX timestamp into approximate seconds since the epoch, also
    /// returning the remaining input. The parsed timestamp is only accurate to
    /// 24 hrs. Note that PKIX imposes additional limits on the format of
    /// timestamps on top of ASN.1 and this code assumes those limits.
    fn parse_time(der: &[u8]) -> Option<(i64, &[u8])> {
        // https://datatracker.ietf.org/doc/html/rfc5280#section-4.1.2.5
        let (tag, contents, rest) = der::next(der)?;
        if tag == der::UTC_TIME {
            // YYMMDDHHMMSSZ
            if contents.len() != 13 {
                return None;
            }
            let contents = String::from_utf8(contents.to_vec()).ok()?;
            let two_digit_year = contents[0..2].parse::<u8>().ok()? as u16;
            let year =
                if two_digit_year >= 50 { 1900 + two_digit_year } else { 2000 + two_digit_year };
            let month = contents[2..4].parse::<u8>().ok()?;
            let day = contents[4..6].parse::<u8>().ok()?;
            Some((epoch_seconds(year, month, day)?, rest))
        } else if tag == der::GENERALIZED_TIME {
            // YYYYMMDDHHMMSSZ
            if contents.len() != 15 {
                return None;
            }
            let contents = String::from_utf8(contents.to_vec()).ok()?;
            let year = contents[0..4].parse::<u16>().ok()?;
            let month = contents[4..6].parse::<u8>().ok()?;
            let day = contents[6..8].parse::<u8>().ok()?;
            Some((epoch_seconds(year, month, day)?, rest))
        } else {
            None
        }
    }

    /// Convert a year/month/day into seconds since the epoch.
    fn epoch_seconds(year: u16, month: u8, day: u8) -> Option<i64> {
        fn is_leap_year(year: u16) -> bool {
            year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)
        }
        fn days_in_year(year: u16) -> u16 {
            if is_leap_year(year) { 366 } else { 365 }
        }
        const DAYS_IN_MONTH: [u8; 12] = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        const DAYS_BEFORE_MONTH: [u16; 12] =
            [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];

        if !(1970..=9999).contains(&year) || !(1..=12).contains(&month) || !(1..=31).contains(&day)
        {
            return None;
        }

        let leap_year = is_leap_year(year);
        let month_idx = (month - 1) as usize;
        let days_in_month = if leap_year && month == 2 { 29 } else { DAYS_IN_MONTH[month_idx] };
        if day > days_in_month {
            return None;
        }

        let mut days: i64 = day as i64 - 1;
        days += DAYS_BEFORE_MONTH[month_idx] as i64;
        if month > 2 && leap_year {
            days += 1;
        }

        for y in 1970..year {
            days += days_in_year(y) as i64;
        }

        Some(days * SECONDS_IN_A_DAY)
    }

    #[cfg(test)]
    mod tests {
        use super::*;
        use crate::recovery_key_store::ROOT_CERTIFICATE;

        #[test]
        fn test_parse() {
            let result = parse(ROOT_CERTIFICATE).unwrap();
            let expected = Certificate {
                der: ROOT_CERTIFICATE,
                not_before_approx_epoch_seconds: 1525651200,
                not_after_approx_epoch_seconds: 2156889600,
                is_ca: true,
                // These offsets into `ROOT_CERTIFICATE` are derived from
                // the output of `openssl asn1parse -i`.
                issuer: &ROOT_CERTIFICATE[0x2e..0x2e + 51],
                subject: &ROOT_CERTIFICATE[0x81..0x81 + 51],
                // The self signature has been removed from the root certificate for size.
                signature: b"",
                spki: &ROOT_CERTIFICATE[0xb4..0xb4 + 550],
                sig_algo_oid: &ROOT_CERTIFICATE[0x23..0x23 + 9],
                to_be_signed: &ROOT_CERTIFICATE[0x04..0x04 + 763],
            };
            assert_eq!(result, expected);
        }

        #[test]
        fn test_epoch_seconds() {
            // These second values can be confirmed by evaluating, e.g.,
            // `time.gmtime(320630400)` in Python after `import time`.
            let expected = [
                (1970, 01, 01, 0),
                (1980, 02, 29, 320630400),
                (1983, 03, 05, 415670400),
                (1999, 12, 31, 946598400),
                (2000, 01, 01, 946684800),
                (2010, 06, 17, 1276732800),
            ];
            for (year, month, day, value) in expected {
                let Some(result) = epoch_seconds(year, month, day) else {
                    panic!("year: {}, month: {}, day: {} panicked", year, month, day);
                };
                if result != value {
                    panic!(
                        "year: {}, month: {}, day: {} -> {}, but expected {}",
                        year, month, day, result, value
                    );
                }
            }
        }

        #[test]
        fn test_invalid_dates() {
            let invalid = [
                (1969, 12, 31),
                (1970, 0, 1),
                (1970, 1, 0),
                (1970, 13, 1),
                (1970, 12, 32),
                // 1999 is not a leap year, so no 29th of Feb.
                (1999, 02, 29),
                // No April 31st in any year.
                (2000, 04, 31),
                (10000, 1, 1),
            ];
            for (year, month, day) in invalid {
                if let Some(_) = epoch_seconds(year, month, day) {
                    panic!(
                        "year: {}, month: {}, day: {} unexpectedly didn't panic",
                        year, month, day
                    );
                }
            }
        }
    }
}

/// The key_distribution module is concerned with getting cohort public keys
/// from the distributed files that contain and validate them.
mod key_distribution {
    use super::{x509, xml, ROOT_CERTIFICATE, SECONDS_IN_A_DAY};
    use crate::spki;
    use alloc::collections::BTreeMap;
    use alloc::string::String;
    use alloc::vec;
    use alloc::vec::Vec;

    #[cfg(feature = "chromium_integration_test")]
    use super::TEST_ROOT_CERTIFICATE;

    /// A Cohort represents a specific recovery key store cohort. It includes:
    ///   * The public key of the cohort.
    ///   * Its certificate chain.
    ///   * The serial number of the public key update.
    type Cohort = ([u8; crypto::P256_X962_LENGTH], Vec<Vec<u8>>, i64);

    /// Return an ECDH public key, a certificate path, and the serial
    /// number, from an arbitrary recovery key store cohort, given the published
    /// XML files. These XML files are found at
    ///   * https://www.gstatic.com/cryptauthvault/v0/cert.xml
    ///   * https://www.gstatic.com/cryptauthvault/v0/cert.sig.xml
    ///
    /// The `current_time` is used for certificate validation. The
    /// `cohort_selector` is used to deterministically select one of the
    /// cohorts from the set.
    pub fn get_cohort_key(
        cert_xml: &[u8],
        sig_xml: &[u8],
        current_time_epoch_millis: i64,
        cohort_selector: u32,
    ) -> Result<Cohort, &'static str> {
        // The cert.sig.xml contains:
        //   a) a leaf X.509 certificate.
        //   b) a signature of the `cert.xml` file by that certificate.
        //   c) a number of intermediate certificates from which a chain can be
        //      built to `ROOT_CERTIFICATE`.
        let (sig_toplevel, sig_value) = xml::parse(sig_xml).ok_or("failed to parse sig XML")?;
        if sig_toplevel != "signature" {
            return Err("unexpected toplevel element in sig XML");
        }
        let xml::Value::Object(sig_value) = sig_value else {
            return Err("no child elements in sig XML");
        };

        #[cfg(not(feature = "chromium_integration_test"))]
        let root_cert = ROOT_CERTIFICATE;
        #[cfg(feature = "chromium_integration_test")]
        let root_cert = if sig_value.contains_key("chromium-test") {
            TEST_ROOT_CERTIFICATE
        } else {
            ROOT_CERTIFICATE
        };

        let Some(xml::Element::Single(xml::Value::String(leaf_cert_der_b64))) =
            sig_value.get("certificate")
        else {
            return Err("missing <certificate>");
        };
        let leaf_cert_der = base64::decode(leaf_cert_der_b64)
            .map_err(|_| "failed to base64-decode <certificate>")?;
        let leaf_cert = x509::parse(&leaf_cert_der).ok_or("failed to parse <certificate>")?;
        let (leaf_key_type, leaf_key) =
            spki::parse(leaf_cert.spki).ok_or("failed to parse leaf key")?;
        if leaf_key_type != spki::PublicKeyType::RSA {
            return Err("leaf key is not RSA, but should be");
        }

        let Some(xml::Element::Single(xml::Value::String(sig_b64))) = sig_value.get("value") else {
            return Err("missing <value>");
        };
        let sig = base64::decode(sig_b64).map_err(|_| "failed to base64-decode <value>")?;

        // The public key from <certificate> should only be trusted if it chains
        // up to `ROOT_CERTIFICATE`.
        build_path(&leaf_cert, sig_value, root_cert, current_time_epoch_millis)?;

        // The <value> is an RSA signature of the cert.xml file.
        if !crypto::rsa_verify(leaf_key, cert_xml, &sig) {
            return Err("signature validation failed");
        }

        // Now that the `cert.xml` file has been validated, the cohort public keys
        // can be extracted from it.
        get_public_keys_from_certs(cert_xml, current_time_epoch_millis, root_cert, cohort_selector)
    }

    /// Return an ECDH public key, a certificate path, and the serial number
    /// from an arbitrary recovery key store cohort, given the `cert.xml` file,
    /// which must already have been validated.
    ///
    /// The `current_time` is used for certificate validation and also to
    /// "randomly" pick a cohort.
    fn get_public_keys_from_certs(
        cert_xml: &[u8],
        current_time_epoch_millis: i64,
        root_cert: &[u8],
        cohort_selector: u32,
    ) -> Result<Cohort, &'static str> {
        let (certs_toplevel, certs_value) =
            xml::parse(cert_xml).ok_or("failed to parse certs XML")?;
        if certs_toplevel != "certificate" {
            return Err("unexpected toplevel element in certs XML");
        }
        let xml::Value::Object(certs_value) = certs_value else {
            return Err("no child elements in certs XML");
        };

        let Some(xml::Element::Single(xml::Value::Object(metadata))) = certs_value.get("metadata")
        else {
            return Err("missing or incorrectly structured <metadata>");
        };

        let Some(xml::Element::Single(xml::Value::String(serial))) = metadata.get("serial") else {
            return Err("missing or incorrectly structured <serial>");
        };
        let serial = serial.parse::<i64>().map_err(|_| "cannot parse serial number")?;
        let Some(xml::Element::Single(xml::Value::Object(endpoints))) =
            certs_value.get("endpoints")
        else {
            return Err("missing or incorrectly structured <endpoints>");
        };
        let endpoint_ders = endpoints
            .get("cert")
            .ok_or("missing <cert>")?
            .iter()
            .map(|cert_value| {
                let xml::Value::String(cert_der_b64) = cert_value else {
                    return Err("unexpected object in <cert>");
                };
                base64::decode(cert_der_b64).map_err(|_| "failed to base64-decode <cert>")
            })
            .collect::<Result<Vec<Vec<u8>>, &'static str>>()?;

        if endpoint_ders.is_empty() {
            // This should be impossible, but is checked here because
            // establishing that fact involves aspects of the XML parser.
            return Err("no endpoint certificates");
        }
        // Pick one of the endpoints arbitrarily.
        let endpoint_der = &endpoint_ders[cohort_selector as usize % endpoints.len()];
        let endpoint_cert = x509::parse(endpoint_der).ok_or("failed to parse <cert>")?;

        let (cohort_public_key_type, cohort_public_key) =
            spki::parse(endpoint_cert.spki).ok_or("invalid SPKI in leaf certificate")?;
        if cohort_public_key_type != spki::PublicKeyType::P256 {
            return Err("leaf certificate public key is not P-256");
        }
        let cohort_public_key: [u8; crypto::P256_X962_LENGTH] =
            cohort_public_key.try_into().map_err(|_| "leaf certificate public key invalid")?;

        Ok((
            cohort_public_key,
            // Even though the `cert.xml` file is signed by `cert.sig.xml`,
            // the certificates within it also chain to `ROOT_CERTIFICATE` and
            // the certificate path is required in futher parts of the protocol.
            // Thus it might seem that checking `cert.sig.xml` is superfluous
            // but the Android implementation does it and so this code does too.
            build_path(&endpoint_cert, certs_value, root_cert, current_time_epoch_millis)?,
            serial,
        ))
    }

    /// Attempt to build a path from `leaf` to `root_der` using intermediates
    /// from `xml`. If successful, returns the DER encoded certificates on
    /// the path, not including the root, starting from the leaf.
    fn build_path(
        leaf: &x509::Certificate,
        xml: BTreeMap<String, xml::Element>,
        root_der: &[u8],
        current_time_epoch_millis: i64,
    ) -> Result<Vec<Vec<u8>>, &'static str> {
        let root = x509::parse(root_der).ok_or("failed to parse root")?;

        let Some(xml::Element::Single(xml::Value::Object(intermediates_value))) =
            xml.get("intermediates")
        else {
            return Err("missing <intermediates>");
        };
        let mut intermediates_der = intermediates_value
            .get("cert")
            .unwrap_or(&xml::Element::List(Vec::new()))
            .iter()
            .map(|cert_value| {
                let xml::Value::String(cert_der_b64) = cert_value else {
                    return Err("non-string in intermediates list");
                };
                base64::decode(cert_der_b64).map_err(|_| "failed to base64-decode <intermediate>")
            })
            .collect::<Result<Vec<Vec<u8>>, &'static str>>()?;

        let intermediates = intermediates_der
            .iter()
            .map(|der| x509::parse(der))
            .collect::<Option<Vec<x509::Certificate>>>()
            .ok_or("failed to parse <intermediate>")?;

        /// Verify an X.509 signature.
        fn verify(
            sig_algo_oid: &[u8],
            issuer_spki: &[u8],
            signed_message: &[u8],
            signature: &[u8],
        ) -> bool {
            // This is the DER-encoded OID for RSA signatures using SHA-256.
            const RSA_WITH_SHA256: &[u8] = b"\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b";
            if sig_algo_oid != RSA_WITH_SHA256 {
                return false;
            }
            let Some((pub_key_type, pub_key)) = spki::parse(issuer_spki) else {
                return false;
            };
            if pub_key_type != spki::PublicKeyType::RSA {
                return false;
            }
            crypto::rsa_verify(pub_key, signed_message, signature)
        }

        /// Check that the claimed chain of certificates is valid at the given
        /// time.
        fn check_path(certs: &[&x509::Certificate], current_time_epoch_secs: i64) -> bool {
            if certs.len() < 2 {
                return false;
            }
            certs.windows(2).all(|pair| {
                // The timestamps are only accurate to within a day, but that's fine for
                // expiration checking. The "not after" timestamp is advanced by a day so that
                // to err on the side of validity.
                pair[0].not_before_approx_epoch_seconds <= current_time_epoch_secs
                    && pair[0].not_after_approx_epoch_seconds + SECONDS_IN_A_DAY
                        >= current_time_epoch_secs
                    && pair[1].is_ca
                    && pair[0].issuer == pair[1].subject
                    && verify(
                        pair[0].sig_algo_oid,
                        pair[1].spki,
                        pair[0].to_be_signed,
                        pair[0].signature,
                    )
            })
        }

        /// Perform one step of path building. The path is built from the leaf
        /// upwards by adding a subset of the intermediate certificates. If
        /// successful it returns a vector of indexes of the intermediates used,
        /// in order from leaf to root.
        fn path_step(
            leaf: &x509::Certificate,
            intermediates: &[x509::Certificate],
            root: &x509::Certificate,
            current_time_epoch_millis: i64,
            // The number of steps performed so far.
            steps: &mut usize,
            // The indexes of the intermediates in the current path.
            path: Vec<usize>,
        ) -> Option<Vec<usize>> {
            // While cycles in the certificate graph are already blocked, this algorithm can
            // still be factorial in the number of intermediates. This limits the maximum
            // about of work that will be attempted. In practice, the total number of steps
            // needed at the time of writing is two.
            const MAX_STEPS: usize = 100;
            if *steps > MAX_STEPS {
                return None;
            }

            let head = path.last().map_or(leaf, |i| &intermediates[*i]);

            if head.issuer == root.subject {
                // We can reach the root in a single step from the current head. See whether
                // that's a valid chain.
                let mut certs = Vec::new();
                certs.push(leaf);
                for i in &path {
                    certs.push(&intermediates[*i]);
                }
                certs.push(root);

                return if check_path(&certs, current_time_epoch_millis / 1000) {
                    Some(path)
                } else {
                    None
                };
            }

            for (i, intermediate) in intermediates.iter().enumerate() {
                // An intermediate will not be used twice in any path.
                if path.contains(&i) {
                    continue;
                }
                if head.issuer == intermediate.subject {
                    // This intermediate fits on the end of the current path. Recurse to see whether
                    // a valid path can be found using it.
                    let mut path = path.clone();
                    path.push(i);
                    *steps += 1;
                    let path = path_step(
                        leaf,
                        intermediates,
                        root,
                        current_time_epoch_millis,
                        steps,
                        path,
                    );
                    if path.is_some() {
                        return path;
                    }
                }
            }

            // This path is a dead-end. Backtrack to try another.
            None
        }

        let mut steps = 0;
        let intermediate_indexes = path_step(
            leaf,
            &intermediates,
            &root,
            current_time_epoch_millis,
            &mut steps,
            Vec::new(),
        )
        .ok_or("no path found")?;

        // Build a vector of the certificates in the valid path by pulling
        // the values from `intermediates_der`.
        let mut ret = vec![leaf.der.to_vec()];
        for intermediate_index in intermediate_indexes {
            ret.push(core::mem::take(intermediates_der.get_mut(intermediate_index).unwrap()));
        }

        Ok(ret)
    }

    #[cfg(test)]
    mod tests {
        use super::super::{SAMPLE_CERTS_XML, SAMPLE_SIG_XML, SAMPLE_VALIDATION_EPOCH_MILLIS};
        use super::*;

        const SAMPLE_COHORT_SELECTOR: u32 = 0;

        #[test]
        fn test_get_public_keys() {
            let (_key, path, serial) = get_cohort_key(
                SAMPLE_CERTS_XML,
                SAMPLE_SIG_XML,
                SAMPLE_VALIDATION_EPOCH_MILLIS,
                SAMPLE_COHORT_SELECTOR,
            )
            .unwrap();
            assert_eq!(path.len(), 2);
            assert_eq!(serial, 10016);
        }

        #[test]
        fn test_get_public_keys_expired() {
            assert!(
                get_cohort_key(SAMPLE_CERTS_XML, SAMPLE_SIG_XML, 1, SAMPLE_COHORT_SELECTOR)
                    .is_err()
            )
        }

        #[test]
        fn test_looping() {
            // This is a self-signed certificate, thus its issuer and subject names are
            // equal. The this is substituted into the template below as the leaf, and as a
            // number of intermediates. This creates a factorial number of possible paths to
            // explore. This test checks that this fails quickly because of the step limit.
            const SELF_SIGNED: &str = "MIIC9TCCAd2gAwIBAgIRANpvKYYQJ4Qj7jmebDBBsnMwDQYJKoZIhvcNAQELBQAwEjEQMA4GA1UEChMHQWNtZSBDbzAeFw0yNDAyMTAxMzM4MDJaFw0yNTAyMDkxMzM4MDJaMBIxEDAOBgNVBAoTB0FjbWUgQ28wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC305e6s9JSPDozo3q0XqPeiv4LslYJx23UtJ8/f1EuoxjRCS4lXCAxaj0WBgyNLqShVa7ZJ3BbzfNIcqmDhzEWB8FeyucDutxjjUWmuE6jNd892Daid64tjPuBGVaqPw71zaletuq8CLprzZdTFuW9TNCuCaDaeeibqrfjH8SbUCV1lIMd0wIJ+6QcvoN6OBNSmR/zjjfoJq4U6gt1MpLpdy2kUhdr6gDU2A4JMOUztXk4MBvV2nfbtt8YldTJshukcfgC6Z3y5wXcGI1qsKQPbHYzIXIM2UOSSVIWLjuYWr68yIrQkP+ph1lyBFZo/j4yE5Bi4mleWxJn6K1azvIxAgMBAAGjRjBEMA4GA1UdDwEB/wQEAwIFoDATBgNVHSUEDDAKBggrBgEFBQcDATAMBgNVHRMBAf8EAjAAMA8GA1UdEQQIMAaCBHRlc3QwDQYJKoZIhvcNAQELBQADggEBAAK8zjiJ2HCPhdJB+T0526ao0pc08O8B8VZ4fD4Y+dO3REDwqQBvbPKtv9ysF3iSqd6wJwIHw4KpGi6Le557b786ev91mWoInRgEQb+aRKCeGJP2+g1T5JTy5KRfsheeq2ilGorGFTsqz89oDXbGsGilQZS69i/mMFaLketBDChG5XYom5YCtO2F6P++FVWyXCZtSl4roiO6Tol2si2GRmUZMi4YvgG98EbNaXQ0POO785cXSJVkB6Ag8uO7Vll01DcHYfJLkKXRCaetYIdUAlkdiRnJSaXNFKolqK3NO3RbUIBqmYjRU85Fbbun99Y2CbpA2xLRsfMjVabbQFkg/QA=";
            const TEMPLATE: &str = r#"
<?xml version="1.0" encoding="UTF-8"?>
<signature>
  <intermediates>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
    <cert>CERT</cert>
  </intermediates>
  <certificate>CERT</certificate>
  <value>9Et3PY1oHl8=</value>
</signature>
"#;
            let sig_xml = TEMPLATE.replace("CERT", SELF_SIGNED);
            assert_eq!(
                Err("no path found"),
                get_cohort_key(b"", &sig_xml.into_bytes(), 1, SAMPLE_COHORT_SELECTOR)
            );
        }
    }
}

/// The securebox module implements the "securebox" construction that is used
/// by the recovery key store.
///
/// Within Google, see go/securebox2
mod securebox {
    use alloc::vec::Vec;

    const EMPTY: &[u8] = b"";
    const VERSION: &[u8] = b"\x02\x00";
    const SALT: &[u8] = b"SECUREBOX\x02\x00";
    const SHARED_SECRET_MAGIC: &[u8] = b"SHARED HKDF-SHA-256 AES-128-GCM";
    const ECDH_MAGIC: &[u8] = b"P256 HKDF-SHA-256 AES-128-GCM";

    /// Encrypt `payload`, and authenticate `header`, to a combination of
    /// `shared_secret` and a key shared with the optional public key
    /// `their_public`. Only fails if `their_public` is invalid.
    pub fn encrypt(
        their_public: Option<&[u8; crypto::P256_X962_LENGTH]>,
        shared_secret: &[u8],
        header: &[u8],
        payload: &[u8],
    ) -> Option<Vec<u8>> {
        let keying_material_storage;
        let my_public_storage;
        let (my_public, keying_material, info) = match their_public {
            None => (EMPTY, shared_secret, SHARED_SECRET_MAGIC),
            Some(their_public) => {
                let my_private = crypto::P256Scalar::generate();
                my_public_storage = my_private.compute_public_key();
                let ecdh_shared_secret =
                    crypto::p256_scalar_mult(&my_private, their_public).ok()?;
                keying_material_storage = [&ecdh_shared_secret, shared_secret].concat();
                (my_public_storage.as_ref(), keying_material_storage.as_ref(), ECDH_MAGIC)
            }
        };

        let mut secret_key = [0u8; 16];
        crypto::hkdf_sha256(keying_material, SALT, info, &mut secret_key).ok()?;

        let mut nonce = [0u8; crypto::NONCE_LEN];
        crypto::rand_bytes(&mut nonce);

        let mut ciphertext = payload.to_vec();
        crypto::aes_128_gcm_seal_in_place(&secret_key, &nonce, header, &mut ciphertext);

        Some([VERSION, my_public, nonce.as_ref(), ciphertext.as_ref()].concat())
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        fn decrypt(
            my_private: Option<&[u8; crypto::P256_SCALAR_LENGTH]>,
            shared_secret: &[u8],
            header: &[u8],
            encrypted_payload: &[u8],
        ) -> Option<Vec<u8>> {
            if encrypted_payload.len() < VERSION.len() {
                return None;
            }
            let (version, encrypted_payload) = encrypted_payload.split_at(VERSION.len());
            if version != VERSION {
                return None;
            }

            let keying_material_storage;
            let (keying_material, info, encrypted_payload) = match my_private {
                None => (shared_secret, SHARED_SECRET_MAGIC, encrypted_payload),
                Some(my_private) => {
                    let my_private = crypto::P256Scalar::try_from(my_private).ok()?;
                    if encrypted_payload.len() < crypto::P256_X962_LENGTH {
                        return None;
                    }
                    let (their_public, encrypted_payload) =
                        encrypted_payload.split_at(crypto::P256_X962_LENGTH);
                    let ecdh_shared_secret =
                        crypto::p256_scalar_mult(&my_private, their_public.try_into().unwrap())
                            .ok()?;
                    keying_material_storage = [&ecdh_shared_secret, shared_secret].concat();
                    (keying_material_storage.as_ref(), ECDH_MAGIC, encrypted_payload)
                }
            };

            if encrypted_payload.len() < crypto::NONCE_LEN {
                return None;
            }
            let (nonce, ciphertext) = encrypted_payload.split_at(crypto::NONCE_LEN);
            let ciphertext = ciphertext.to_vec();

            let mut secret_key = [0u8; 16];
            crypto::hkdf_sha256(keying_material, SALT, info, &mut secret_key).ok()?;

            crypto::aes_128_gcm_open_in_place(
                &secret_key,
                nonce.try_into().unwrap(),
                header,
                ciphertext,
            )
            .ok()
        }

        const TEST_SHARED_SECRET: &[u8] = b"sharedsecret";
        const TEST_HEADER: &[u8] = b"header";
        const TEST_PAYLOAD: &[u8] = b"payload";
        const TEST_PUBLIC_KEY : &[u8; crypto::P256_X962_LENGTH] = b"\x04\x8b\x76\xbe\x02\xa7\xf8\xa4\x03\x3c\x3f\x47\x37\xaa\xd7\x94\x23\x88\x34\xb1\xc4\x47\xab\xa7\xb8\xd8\x99\xd5\xc4\x3d\xc7\x20\x91\x1c\x40\x44\x6b\x4c\x0b\xfe\xe1\x72\x45\x72\x4d\x26\x8e\x9a\x55\x16\xa1\xf1\x98\x8c\x58\x97\xfd\xc0\x13\x22\xc7\x10\x17\x9a\xf1";
        const TEST_PRIVATE_KEY : &[u8; crypto::P256_SCALAR_LENGTH] = b"\x2a\x55\xbb\x5f\x96\x08\x60\x8c\xd8\x6b\x09\xbc\xe7\x30\xf7\xc4\xff\x0d\xd5\xe7\x64\xc7\x3c\xf2\x2c\xbb\x3c\x2a\x2b\xba\x83\x20";

        #[test]
        fn encrypt_symmetric() {
            let ciphertext = encrypt(None, TEST_SHARED_SECRET, TEST_HEADER, TEST_PAYLOAD).unwrap();
            let plaintext = decrypt(None, TEST_SHARED_SECRET, TEST_HEADER, &ciphertext).unwrap();
            assert_eq!(plaintext, TEST_PAYLOAD);
        }

        #[test]
        fn encrypt_asymmetric() {
            let ciphertext =
                encrypt(Some(TEST_PUBLIC_KEY), TEST_SHARED_SECRET, TEST_HEADER, TEST_PAYLOAD)
                    .unwrap();
            let plaintext =
                decrypt(Some(TEST_PRIVATE_KEY), TEST_SHARED_SECRET, TEST_HEADER, &ciphertext)
                    .unwrap();
            assert_eq!(plaintext, TEST_PAYLOAD);
        }

        #[test]
        fn decrypt_known_good() {
            // Test a known-good ciphertext generated by another implementation.
            const TEST_ENCRYPTED_PAYLOAD : &[u8] = b"\x02\x00\x04\xb5\xd6\xd6\x97\xe2\x8f\x35\x7b\xfe\x19\x22\xa3\xb5\x94\xaa\x32\xae\x82\x61\xfd\xef\xfa\xa8\x73\x91\xc2\x43\xd4\x0e\x53\x6a\xf0\xf1\x3b\xf7\xec\x4b\xdb\x34\x48\xeb\xf2\xa2\xf2\x81\x7e\x78\x5c\x08\x90\x45\x22\x2d\xed\xdf\x17\x12\xb4\xff\x0c\xb1\x27\x9d\x8a\x95\x1e\xc1\x46\xa8\x6b\x30\xc3\x57\xeb\x6d\xd4\xc3\xef\xb2\xc7\x02\x6d\x47\xef\x96\x75\xe4\xba\x5b\x3a\x1a\x9a\x8a\x3f\xcf\x02";
            const TEST_HEADER: &[u8] = b"\x01\x02\x03\x04";
            const TEST_PAYLOAD: &[u8] = b"\x04\x05\x06\x07";
            let plaintext =
                decrypt(Some(TEST_PRIVATE_KEY), b"", TEST_HEADER, TEST_ENCRYPTED_PAYLOAD).unwrap();
            assert_eq!(plaintext, TEST_PAYLOAD);
        }
    }
}

fn cohort_selector_from_handle(handle: &[u8; VAULT_HANDLE_LEN]) -> u32 {
    // unwrap: the slice will always be four bytes long, as required.
    u32::from_le_bytes(handle[VAULT_HANDLE_LEN - 4..VAULT_HANDLE_LEN].try_into().unwrap())
}

/// The maximum number of attempted guesses that the recovery key store will
/// allow for the secret.
const MAX_ATTEMPTS: u32 = 10;

/// The length, in bytes, of a Vault handle.
pub const VAULT_HANDLE_LEN: usize = 17;

/// This identifies a GPM PIN vault.
pub const VAULT_HANDLE_FIRST_BYTE: u8 = 3;

/// The length, in bytes, of a Vault handle.
pub const COUNTER_ID_LEN: usize = 8;

/// This structure holds the many values that result from wrapping a knowledge
/// factor. All these values need to be placed into a `Vault` protobuf for
/// submission to the service.
struct Wrapped {
    encrypted_recovery_key: Vec<u8>,
    vault_handle: [u8; VAULT_HANDLE_LEN],
    counter_id: [u8; COUNTER_ID_LEN],
    cohort_public_key: [u8; crypto::P256_X962_LENGTH],
    max_attempts: u32,
    certs_in_path: Vec<Vec<u8>>,
    app_public_key: [u8; crypto::P256_X962_LENGTH],
    wrapped_app_private_key: Vec<u8>,
    wrapped_wrapping_key: Vec<u8>,
    serial: i64,
}

impl From<Wrapped> for cbor::Value {
    fn from(value: Wrapped) -> Self {
        cbor!({
            "cohort_public_key": (&value.cohort_public_key),
            "encrypted_recovery_key": (value.encrypted_recovery_key),
            "vault_handle": (&value.vault_handle),
            "counter_id": (&value.counter_id),
            "max_attempts": (value.max_attempts as i64),
            "certs_in_path": (value.certs_in_path.into_iter().map(|cert| Value::Bytestring(Bytes::from(cert))).collect::<Vec<Value>>()),
            "app_public_key": (&value.app_public_key),
            "wrapped_app_private_key": (value.wrapped_app_private_key),
            "wrapped_wrapping_key": (value.wrapped_wrapping_key),
        })
    }
}

/// The Vault parameters. These must be constant when renewing a Vault so that
/// renewal doesn't reset the attempt counters.
struct Parameters {
    vault_handle: [u8; VAULT_HANDLE_LEN],
    counter_id: [u8; COUNTER_ID_LEN],
}

impl Parameters {
    /// Generate a random set of Parameters.
    fn random() -> Self {
        let mut params = Parameters { vault_handle: [0u8; VAULT_HANDLE_LEN], counter_id: [0u8; 8] };
        params.vault_handle[0] = VAULT_HANDLE_FIRST_BYTE;
        crypto::rand_bytes(&mut params.vault_handle[1..]);
        crypto::rand_bytes(&mut params.counter_id);
        params
    }

    fn from_pin_data(pin_data: &pin::Data) -> Self {
        let mut params =
            Parameters { counter_id: pin_data.counter_id, vault_handle: [0u8; VAULT_HANDLE_LEN] };
        params.vault_handle[0] = VAULT_HANDLE_FIRST_BYTE;
        params.vault_handle[1..].copy_from_slice(&pin_data.vault_handle_without_type);
        params
    }

    fn from_request(request: &BTreeMap<cbor::MapKey, cbor::Value>) -> Result<Self, RequestError> {
        let Some(Value::Bytestring(counter_id)) = request.get(COUNTER_ID_KEY) else {
            return debug("counter ID required");
        };
        if counter_id.len() != COUNTER_ID_LEN {
            return debug("bad counter ID length");
        }

        let Some(Value::Bytestring(vault_handle_without_type)) =
            request.get(VAULT_HANDLE_WITHOUT_TYPE_KEY)
        else {
            return debug("vault handle required");
        };
        if vault_handle_without_type.len() != VAULT_HANDLE_LEN - 1 {
            return debug("bad vault handle length");
        }

        let mut params = Parameters {
            counter_id: counter_id
                .as_ref()
                .try_into()
                .map_err(|_| RequestError::Debug("incorrect length counter ID"))?,
            vault_handle: [0u8; VAULT_HANDLE_LEN],
        };
        params.vault_handle[0] = VAULT_HANDLE_FIRST_BYTE;
        params.vault_handle[1..].copy_from_slice(vault_handle_without_type);
        Ok(params)
    }
}

fn wrap(
    pin_hash: &[u8],
    cert_xml: &[u8],
    sig_xml: &[u8],
    params: Parameters,
    current_time_epoch_millis: i64,
) -> Result<Wrapped, &'static str> {
    let (cohort_public_key, certs_in_path, serial) = key_distribution::get_cohort_key(
        cert_xml,
        sig_xml,
        current_time_epoch_millis,
        cohort_selector_from_handle(&params.vault_handle),
    )?;

    let max_attempts = MAX_ATTEMPTS.to_le_bytes();

    let vault_params = [
        b"V1 THM_encrypted_recovery_key".as_ref(),
        cohort_public_key.as_ref(),
        &params.counter_id,
        max_attempts.as_ref(),
        &params.vault_handle,
    ]
    .concat();

    let mut recovery_key = [0u8; 32];
    crypto::rand_bytes(&mut recovery_key);

    // Within Google, see go/recovery-key-store-creation
    let locally_encrypted_recovery_key = securebox::encrypt(
        /* their_public_key= */ None,
        /* shared_secret= */ pin_hash,
        /* header= */ b"V1 locally_encrypted_recovery_key",
        /* payload= */ recovery_key.as_ref(),
    )
    .ok_or("failed to encrypt PIN hash")?;
    let thm_kf_hash = crypto::sha256_two_part(b"THM_KF_hash", pin_hash);
    let encrypted_recovery_key = securebox::encrypt(
        Some(&cohort_public_key),
        /* shared_secret= */ &thm_kf_hash,
        /* header= */ &vault_params,
        /* payload= */ &locally_encrypted_recovery_key,
    )
    .ok_or("failed to encrypt encrypted PIN hash")?;

    let app_private_key = crypto::P256Scalar::generate();
    let app_public_key = app_private_key.compute_public_key();
    let mut app_private_key_to_be_wrapped =
        [app_private_key.bytes().as_ref(), app_public_key.as_ref()].concat();

    let mut wrapping_key = [0u8; 32];
    crypto::rand_bytes(&mut wrapping_key);

    let wrapped_wrapping_key = securebox::encrypt(
        /* their_public= */ None,
        &recovery_key,
        b"V1 encrypted_application_key",
        &wrapping_key,
    )
    .ok_or("failed to encrypt wrapping key")?;

    let mut nonce = [0u8; 12];
    crypto::rand_bytes(&mut nonce);

    crypto::aes_256_gcm_seal_in_place(
        &wrapping_key,
        &nonce,
        /* aad= */ b"",
        &mut app_private_key_to_be_wrapped,
    );
    let wrapped_app_private_key = [nonce.as_ref(), &app_private_key_to_be_wrapped].concat();

    Ok(Wrapped {
        cohort_public_key,
        encrypted_recovery_key,
        vault_handle: params.vault_handle,
        counter_id: params.counter_id,
        max_attempts: MAX_ATTEMPTS,
        certs_in_path,
        app_public_key,
        wrapped_app_private_key,
        wrapped_wrapping_key,
        serial,
    })
}

/// Return a CBOR response for `wrapped` which includes the fields needed to
/// insert the virtual member into a security domain.
fn include_security_domain_member_fields(
    wrapped: Wrapped,
    security_domain_secret: &[u8; 32],
) -> Result<cbor::Value, RequestError> {
    let header = b"V1 shared_key";
    let Some(wrapped_sds) =
        securebox::encrypt(Some(&wrapped.app_public_key), &[], header, security_domain_secret)
    else {
        return debug("generated public key was invalid");
    };
    let member_proof = crypto::hmac_sha256(security_domain_secret, &wrapped.app_public_key);
    let wrapped_cbor: cbor::Value = wrapped.into();

    Ok(cbor!({
        "wrapped_sds": wrapped_sds,
        "member_proof": (&member_proof),
        "wrapped": wrapped_cbor,
    }))
}

/// Check that the given serial number is at least equal to the greatest serial
/// number used by the given device before. This is done so that devices cannot
/// request rewrapping of PIN hashes under older recovery key store cohorts,
/// which may allow an attacker additional guesses against a PIN.
fn enforce_cert_highwater(
    state: &mut DirtyFlag<ParsedState>,
    device_id: &[u8],
    serial: i64,
) -> Result<(), RequestError> {
    let device = state.get_device(device_id).ok_or(RequestError::Debug("missing device"))?;
    let current_value: Option<i64> =
        device.get(RECOVERY_KEY_STORE_SERIAL_HIGHWATER_KEY).and_then(|v| match v {
            Value::Int(current) => Some(*current),
            _ => None,
        });

    // If the serial matches the current highwater exactly, we're done.
    match current_value {
        Some(current) if current == serial => return Ok(()),
        Some(current) if current > serial => return Err(RequestError::RecoveryKeyStoreDowngrade),
        _ => (),
    }

    // Need to update the highwater.
    let device =
        state.get_mut().get_mut_device(device_id).ok_or(RequestError::Debug("missing device"))?;
    device.insert(
        MapKey::String(String::from(RECOVERY_KEY_STORE_SERIAL_HIGHWATER)),
        Value::Int(serial),
    );
    Ok(())
}

map_keys! {
    PIN_HASH, PIN_HASH_KEY = "pin_hash",
    CERT_XML, CERT_XML_KEY = "cert_xml",
    SIG_XML, SIG_XML_KEY = "sig_xml",
    RECOVERY_KEY_STORE_SERIAL_HIGHWATER, RECOVERY_KEY_STORE_SERIAL_HIGHWATER_KEY = "recovery_key_store_serial_highwater",
}

/// Encrypts a PIN hash to a Vault public key. This is a purely public operation
/// that could be done by the client. It's exposed from the enclave to save
/// having to reimplement this logic in Chromium since it has to live in the
/// enclave in order to support `do_wrap_as_member`, below.
pub(crate) fn do_wrap(
    current_time_epoch_millis: i64,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let Some(Value::Bytestring(pin_hash)) = request.get(PIN_HASH_KEY) else {
        return debug("PIN hash required");
    };
    let Some(Value::Bytestring(cert_xml)) = request.get(CERT_XML_KEY) else {
        return debug("cert.xml required");
    };
    let Some(Value::Bytestring(sig_xml)) = request.get(SIG_XML_KEY) else {
        return debug("cert.sig.xml required");
    };
    let wrapped =
        wrap(pin_hash, cert_xml, sig_xml, Parameters::random(), current_time_epoch_millis)
            .map_err(RequestError::Debug)?;
    Ok(wrapped.into())
}

/// Encrypts a PIN hash to a Vault public key and then constructs a security
/// domain member for that PIN hash. This is a sensitive operation because it
/// allows a new PIN to be a member of the domain, thus the client must have
/// done user verification or else reauthenticated very recently.
pub(crate) fn do_wrap_as_member(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    current_time_epoch_millis: i64,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    // Reauth is required to perform this command. UV is not enough.
    let device_id = match auth {
        Authentication::Device(device_id, _, _, Reauth::Done) => device_id,
        _ => return debug("PIN change needs reauth via RAPT token"),
    };
    let Some(Value::Bytestring(pin_hash)) = request.get(PIN_HASH_KEY) else {
        return debug("PIN hash required");
    };
    let Some(Value::Bytestring(cert_xml)) = request.get(CERT_XML_KEY) else {
        return debug("cert.xml required");
    };
    let Some(Value::Bytestring(sig_xml)) = request.get(SIG_XML_KEY) else {
        return debug("cert.sig.xml required");
    };
    let (security_domain_secret, _) = get_secret_from_request(state, &request, device_id)?;
    let wrapped = wrap(
        pin_hash,
        cert_xml,
        sig_xml,
        Parameters::from_request(&request)?,
        current_time_epoch_millis,
    )
    .map_err(RequestError::Debug)?;

    // Reset the PIN count. This operation allows the client to change the PIN thus
    // they could request the security domain secret from Folsom. Getting another
    // batch of PIN attempts is less powerful than that and it allows the device to
    // use the new PIN immediately, without reregistering with the enclave.
    state.get_mut().set_pin_state(device_id, super::PINState { attempts: 0 })?;

    include_security_domain_member_fields(wrapped, &security_domain_secret)
}

pub(crate) fn do_rewrap(
    auth: &Authentication,
    state: &mut DirtyFlag<ParsedState>,
    current_time_epoch_millis: i64,
    request: BTreeMap<MapKey, Value>,
) -> Result<cbor::Value, RequestError> {
    let device_id = match auth {
        Authentication::Device(device_id, _, _, _) => device_id,
        _ => return debug("not authenticated"),
    };
    let Some(Value::Bytestring(cert_xml)) = request.get(CERT_XML_KEY) else {
        return debug("cert.xml required");
    };
    let Some(Value::Bytestring(sig_xml)) = request.get(SIG_XML_KEY) else {
        return debug("cert.sig.xml required");
    };
    let Some(Value::Bytestring(wrapped_pin_data)) = request.get(WRAPPED_PIN_DATA_KEY) else {
        return debug("wrapped PIN required");
    };
    let (security_domain_secret, _) = get_secret_from_request(state, &request, device_id)?;
    let pin_data = pin::Data::from_wrapped(wrapped_pin_data, &security_domain_secret)?;
    let wrapped = wrap(
        &pin_data.pin_hash,
        cert_xml,
        sig_xml,
        Parameters::from_pin_data(&pin_data),
        current_time_epoch_millis,
    )
    .map_err(RequestError::Debug)?;
    enforce_cert_highwater(state, device_id, wrapped.serial)?;
    include_security_domain_member_fields(wrapped, &security_domain_secret)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_wrap() {
        let pin_hash = [1u8; 32];
        assert!(
            wrap(
                &pin_hash,
                SAMPLE_CERTS_XML,
                SAMPLE_SIG_XML,
                Parameters::random(),
                SAMPLE_VALIDATION_EPOCH_MILLIS
            )
            .is_ok()
        );
    }
}

#[cfg(fuzzing)]
pub mod fuzzing {
    pub fn xml_parse(input: &[u8]) {
        super::xml::parse(input);
    }

    pub fn x509_parse(input: &[u8]) {
        super::x509::parse(input);
    }
}

// Certificate:
//     Data:
//         Version: 3 (0x2)
//         Serial Number:
//             6c:d7:6e:79:4d:a8:d2:f3:3d:80:6a:b8:37:a6:e1:8f
//     Signature Algorithm: sha256WithRSAEncryption
//         Issuer: CN=Google Cloud Key Vault Service Root CA
//         Validity
//             Not Before: May  7 18:24:02 2018 GMT
//             Not After : May  8 19:24:02 2038 GMT
//         Subject: CN=Google Cloud Key Vault Service Root CA
//         Subject Public Key Info:
//             Public Key Algorithm: rsaEncryption
//                 RSA Public-Key: (4096 bit)
//                 Modulus:
//                     00:ad:48:33:bb:ee:28:f7:29:76:d9:ea:a5:d4:18:
//                     ...
//                     e7:de:cd
//                 Exponent: 65537 (0x10001)
//         X509v3 extensions:
//             X509v3 Key Usage: critical
//                 Digital Signature, Certificate Sign, CRL Sign
//             X509v3 Basic Constraints: critical
//                 CA:TRUE
//     Signature Algorithm: sha256WithRSAEncryption
//       ...
//
// The self-signature has been removed from this certificate to make it a bit
// smaller.
const ROOT_CERTIFICATE : &[u8] = b"\x30\x82\x03\x0d\x30\x82\x02\xf7\xa0\x03\x02\x01\x02\x02\x10\x6c\xd7\x6e\x79\x4d\xa8\xd2\xf3\x3d\x80\x6a\xb8\x37\xa6\xe1\x8f\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x30\x31\x31\x2f\x30\x2d\x06\x03\x55\x04\x03\x13\x26\x47\x6f\x6f\x67\x6c\x65\x20\x43\x6c\x6f\x75\x64\x20\x4b\x65\x79\x20\x56\x61\x75\x6c\x74\x20\x53\x65\x72\x76\x69\x63\x65\x20\x52\x6f\x6f\x74\x20\x43\x41\x30\x1e\x17\x0d\x31\x38\x30\x35\x30\x37\x31\x38\x32\x34\x30\x32\x5a\x17\x0d\x33\x38\x30\x35\x30\x38\x31\x39\x32\x34\x30\x32\x5a\x30\x31\x31\x2f\x30\x2d\x06\x03\x55\x04\x03\x13\x26\x47\x6f\x6f\x67\x6c\x65\x20\x43\x6c\x6f\x75\x64\x20\x4b\x65\x79\x20\x56\x61\x75\x6c\x74\x20\x53\x65\x72\x76\x69\x63\x65\x20\x52\x6f\x6f\x74\x20\x43\x41\x30\x82\x02\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x02\x0f\x00\x30\x82\x02\x0a\x02\x82\x02\x01\x00\xad\x48\x33\xbb\xee\x28\xf7\x29\x76\xd9\xea\xa5\xd4\x18\x86\x06\xad\xe0\x59\x7a\x28\x87\x6a\xa5\xdc\x9f\xaf\x56\xec\xdf\xfd\x38\x63\xcd\xd2\x20\xd3\x19\x24\x93\x0f\xcd\x00\x5c\x58\x16\x2e\x3d\x12\x8d\x5f\x6b\xf8\x5f\xf3\x00\x88\xa0\x0a\x82\x12\xcd\x65\x0f\xab\x44\xdd\xc0\x83\xdd\x3d\xfe\x11\x03\xea\xba\x1e\x82\x07\x62\xa6\x64\x32\x7a\x98\xf9\xd7\xbd\x55\x25\x52\xe1\x6b\xd0\xed\x8c\xc1\x99\x30\xca\x5a\x81\x11\x3c\xca\xd3\x1e\x4d\x08\x78\x0b\xfe\x9e\x3b\xbe\x48\xe1\x1f\x1e\x8b\xa9\x56\xa8\x6e\x28\x0a\x94\x7c\x6c\xce\xf5\x62\xe1\xf9\x2d\xfe\xaa\xbb\x29\x6d\xd8\x4d\x5c\x61\xc1\xd2\xc6\x11\xa6\xfe\x3a\xa4\x9f\xc0\xcc\x5d\x04\xb8\x4c\x7c\x4d\x0a\xd1\xdb\xc5\xb7\xc6\xec\xf3\x22\x40\x17\x4e\x03\x26\xc3\x1b\x44\x28\x45\x14\x1a\x53\xd7\xb6\x74\xbb\x9d\xe2\x20\x00\x2e\xe6\xa5\x50\x84\xaa\xd0\x5e\x22\x00\xc2\x06\xe8\x66\xa7\x7e\x26\x41\xcb\x5d\x4d\x5f\x25\xe5\x53\xe5\x62\x4e\x26\x0a\x09\x15\x61\xe4\x75\x69\x07\xb5\xae\x26\x49\x89\x52\xef\x62\x75\x43\xdd\xbb\x24\x3d\x49\x74\x44\xf5\x90\x5b\x47\xf8\x40\xed\x60\x0b\x71\xef\x1e\xc5\xf7\x10\x00\x6d\xbd\xad\x30\x84\xf0\xb3\xfc\x30\x77\x6a\xc0\xcd\x94\xd7\xfe\x4c\x51\x6c\x39\x57\x54\xb4\xe8\x53\x4e\x4b\x15\x83\xeb\xf9\xd1\x55\xd7\x0b\xd7\x9a\x2d\x23\x96\x42\x31\x9e\x17\x5a\x54\x1a\x96\x0b\xdd\xe9\xe7\x6f\x14\x89\x47\x0b\xa6\x26\xfe\x1d\x5c\xcc\x58\x67\x58\x23\x71\xb5\x34\xe6\xbf\x95\x3a\x74\x73\xc2\xdc\x6c\x98\xda\xa6\x28\x95\x9d\xe4\x50\x27\x77\x08\xa8\x33\xce\x48\x49\xc4\xab\x8d\x21\xc9\x97\x75\x8f\x1d\xc9\x9c\xec\x49\x33\x01\xec\xf2\xfe\x2c\xf4\x62\x25\x6f\x70\x5c\x3a\x60\xef\x03\xf3\x2e\xd3\xdc\x44\x30\xac\x29\x1c\x19\xb8\x4c\x50\xca\x5d\xe1\x87\x39\x68\x5a\xed\xc7\x16\x10\x40\x9b\xc8\xee\x67\x72\xee\x97\xb8\xdd\xa2\xcb\x3f\x52\xf9\x3b\x8c\xca\x36\x41\x13\x9e\x76\xa7\xa3\xee\xe6\x01\x02\xdb\x19\x3e\xa9\xa6\xf4\x34\x60\xd3\x1d\xd2\xca\x2d\xbc\x96\x9f\x72\x31\x76\x60\x47\xc9\x3a\xfb\x88\xf0\xaa\x9a\x9c\x87\x9e\x09\x02\xfe\x96\xc6\x7e\xf1\xae\xb1\xce\x41\xa4\x1b\xa0\xb0\x1b\x65\xcf\xae\xe6\xe1\x15\x8b\x27\xbd\xb2\x01\xe5\x4f\x3b\xf9\x72\xff\xcc\x38\xa2\xb3\x6c\x19\x68\xe7\xde\xcd\x02\x03\x01\x00\x01\xa3\x23\x30\x21\x30\x0e\x06\x03\x55\x1d\x0f\x01\x01\xff\x04\x04\x03\x02\x01\x86\x30\x0f\x06\x03\x55\x1d\x13\x01\x01\xff\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x03\x01\x00";

// When building this code for integration testing in Chromium, a test root is
// used. The private key for this root, and code for generating it, is in
// `fake_recovery_key_store.cc` in Chromium.
#[cfg(feature = "chromium_integration_test")]
const TEST_ROOT_CERTIFICATE : &[u8] = b"\x30\x82\x02\xac\x30\x82\x01\x94\xa0\x03\x02\x01\x02\x02\x01\x01\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x30\x0f\x31\x0d\x30\x0b\x06\x03\x55\x04\x03\x0c\x04\x52\x6f\x6f\x74\x30\x1e\x17\x0d\x32\x34\x30\x31\x33\x31\x32\x32\x32\x30\x30\x32\x5a\x17\x0d\x32\x34\x30\x32\x31\x34\x32\x32\x32\x30\x30\x32\x5a\x30\x0f\x31\x0d\x30\x0b\x06\x03\x55\x04\x03\x0c\x04\x52\x6f\x6f\x74\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xc7\x95\x62\x74\xaf\x3f\x50\xb9\xa8\x4f\x55\x12\x0e\x5d\x1f\x89\x67\xdf\x55\x8c\x16\x75\x06\x1c\x33\x01\xa0\xdb\xa4\x88\x39\xdc\xca\x57\xf2\x6b\xd3\x0b\x22\x0d\x79\x2c\xc2\x4c\x1d\xf6\x0c\x32\xf6\x66\xca\x07\x89\x12\x17\x1e\x43\xbb\x16\xda\xe8\x2a\xec\x89\x25\x84\x88\xa5\x5d\xc1\x80\xb1\x9c\xab\xed\x00\x7d\xec\x9d\x4a\x49\xa4\x61\x48\x9e\xab\xb3\x78\x35\xce\x72\x80\x40\x20\x58\xca\x90\xbc\x24\x41\x01\x48\xaf\xc7\x69\x9d\x04\xa0\xa5\x20\xe8\xca\x16\xe5\x96\x6a\x36\x82\x2f\xc6\xe5\x0b\x53\x72\x3d\x89\x78\xc8\x27\xf7\xda\xe1\x6e\x07\xd6\xc1\xad\xaf\xa7\x27\xbb\x4b\x1f\x09\xdc\x41\x2d\x68\x21\xda\x85\x0b\xe1\x6f\x7b\x34\xa3\x45\x80\x43\x4e\x2a\xe7\x17\xd1\xde\xd7\xc0\xde\xf6\x0c\xa2\x50\x41\xd2\xf7\x7b\xb6\x81\x29\xb3\x80\xcc\xbf\x1e\x5a\xeb\xf3\x3c\xd2\xe6\x8d\xfc\x9a\x41\x23\xdc\xcb\x68\xff\x9e\x34\xe9\x25\xd7\xa7\x27\xc4\x68\x72\x38\x0e\xcc\x5b\x48\x40\xbb\x90\xa0\xfd\xd4\xc4\xea\xe9\x11\xd5\x6e\xda\x34\xea\x40\x2b\xcd\x21\x9b\x77\xdc\x05\xc2\xcf\x00\x05\xbc\x28\x5c\x59\xcf\xd9\x67\xcb\x30\x12\x77\x5e\xb6\xe9\x02\x03\x01\x00\x01\xa3\x13\x30\x11\x30\x0f\x06\x03\x55\x1d\x13\x01\x01\xff\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b\x05\x00\x03\x82\x01\x01\x00\xb1\xaa\xae\x2a\x28\x83\xdb\x56\x02\xdb\x0f\x15\x32\xce\xa6\x9f\xac\xe4\x98\xa7\x08\x99\x24\xae\xe4\x7c\x8d\x2a\x98\x03\x02\xdc\x9d\xca\x58\x8b\xda\x75\xce\xcc\xef\x51\x26\xab\xd7\x14\xcc\x15\x4d\xa4\x66\x46\x03\x81\xf9\x2b\x09\x0b\x01\xf9\x9a\x1a\x39\x79\xec\x99\xee\x59\xbd\x82\x0a\x44\xe0\xc2\x9f\xe4\x0e\x82\x8e\x10\x0e\x5a\xc3\xd5\x7c\x3c\xe7\x21\x78\xd0\xfa\xea\x72\x7a\xf9\x55\x46\x3b\x89\x94\xec\x8e\xb1\x75\xe2\x14\xf8\xf4\x7d\x63\x71\xea\x60\xad\x03\xdc\x7f\xf4\xe6\x98\x0f\xd0\x9f\x1e\x97\x95\xb4\x7d\xea\x89\xb0\xae\x8a\x52\xb7\xe8\x94\x96\x04\x4a\xce\x82\x8f\x9b\xb9\x1b\x5f\xf2\xe3\xc0\x05\x02\x4b\x7b\xfc\x94\x47\x21\x2b\x24\x61\xd1\x81\xea\x93\xf4\x04\x1e\x6c\x2c\xca\xfe\x7b\x05\xf1\x50\x25\x4f\x4c\x61\xf2\x7c\x0d\xef\xaa\x3c\xe9\x0f\xa6\xdc\x78\x78\x72\x78\x7c\x5b\x7c\x63\xc7\x95\x09\x62\xfc\x05\xc6\xde\xdd\x1f\x16\x61\xa8\xb1\x37\x40\xce\xf8\xfd\xae\x9f\x35\xc0\x1d\xb0\x1f\x45\xec\x4f\xb5\x95\x00\xe2\xb0\x24\x75\xe6\x2b\xb8\xe1\xbc\x84\x3f\xf9\xd1\x0a\x18\x72\xa5\xb8\xa1\x28\x73\x69\x4f\x78\x5d\x83";

#[cfg(test)]
pub(crate)
const SAMPLE_CERTS_XML : &[u8] = br#"<?xml version="1.0" encoding="UTF-8"?>
<certificate>
  <metadata>
    <serial>10016</serial>
    <creation-time>1694037058</creation-time>
    <refresh-interval>2592000</refresh-interval>
    <previous>
      <serial>10015</serial>
      <hash>TQudrujnu1I9bdoDaYxGQYuRN/8SwTLjdk6vzYTOkIU=</hash>
    </previous>
  </metadata>
  <intermediates>
    <cert>MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==</cert>
    <cert>MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=</cert>
  </intermediates>
  <endpoints>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABMCD3sSR26q9occ1Y/K2SQyIsSJkJtGALvd3t4l9E8ajmOV9fQHp7d4ExmRJIldlFL/Y5i5FBg3NvwK7TLvoAPmjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAD7HLz0sS04rV7BXzrd2KJdMk2fCbrjTPNNUUZu+UbPB0lDvWcP1+uroIOEZuPLUK0EBbQYzCjP/bp7tT4me4myivPbg2IBLvTaOVKbUzi6SqA4X+vyAe3c7Bp6A3hPzxNangk2jmpKdIvLXJ8DHyXVrCXk/dNObnWUDnvbmoXg5yWK/snB5OIysDPUlxUmRspxhRajVgRnDAMTnJ2YZhHC15Jm/neugxVKeSeBb4wamLRibkdWbc4KJTiSjh1CnH1OKsCI8N006Gk+YXHnrY3OmakVg/bSnfAoMWLMDvtXbDbMVYAl9uRLBDwoOS6MFMsrj+Iwniuv4E2Kb+UcWK36AR/KH1/ILFpRUTtfPwIQcvEc2tWkH+W2BJqKOvwGH3rOm2qF88g8/egrHua7jnv8aJlfQ3c3S7ytikxugCQhSAJhVO0kdWXGUut78UzBrhMEvBqHlQtZnyPSEWd6bJKdGqwmbQwdKoou5HCu0YQxanmzENR9PmDs6+AMN0xJDcb9TOBQsvQW+vY3D34U61izaU2xytglgRzjSlBwFYDP75VgsL9gcNlYSt9R1EroPPsaEV1xhW47WpWArLdprVhVX70kPf3fUkcpDXimapFpMWONWlSUCIKPy/q0d2DcamL9HN5sZLyOGPctMTEowPomW8TiISWJFdtSK2fJXkk8s</cert>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABOHSWq/RFpU1VnCCCmPcTDeJT3t3+27+BjFOdsC8/hcnbFUKwHt6Tt0uiHV3LP/aO0/DHYC8Kdb/KAMC+ai+aJ2jEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBALz6PK44f46capH7isFvHMdTosG3DIV4QP70zLtGtGBM+57RKU0UYLtgdtKfCCwQVIgru9PfMdNdbxKojI96cfB/QxsH5H/96iUET+EnvvQ63NMSnLtOp7H4UceBujpXeSLN0yRNr59JS+mLtyL5+5KjHgtOM7tpxJ3eP1tx8NnE30TE0BoeTQyoKu0wfHVsc5+Fs3EWJUpgV+Z0/KJFoy3M2Z0DHZxfn6fg+/xYxn8ttkMhlZXhJMjNqtcGmlwLYktmsG5LlsQNimXwGl9olVviEZwcHGUzHw8QWszoKzn+TgTgv76m2eZ5MwJeN1JnaLb+1gQtgKRpnG8TFxWGC/TIHUqLow/GruH2TSlLPr6l6ed+QjG01sAN5cdI7OR84D8W1F0vb8fVOr7kjf7N3qLDNQXDCRUUKHlRVanIt6h+kT1ctlM51+QmRhDsAkzY/3lFrXDySnQk18vlzTyA+QgqmvfNkPhgCp/fpgtWJFaPL9bJWaMaW/soXRUf26F6RMLK43EihdoVMtUAvmCIKUQyI88X6hJxEhWLyy/8Y45nAFk5CgXuzV2doOJTSITtJligTy1IuczH75bmp87c5ZPp51vUO4WYXuwffTCoQ8UYSYbNxxqKOfFkILnM1WoGAzCrVt5aKOyGPILzOsOS8X0EeQ9YF6Mvaf2iFljc2o30</cert>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABNeVqPpEctoVzN48WNefTpJEmRrrbpXoWRhHwH/AOYmQgXR6xX/AE1/qeen8fMj4Lnyb8KPveZjXvTlFq2mdBHGjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAEQIGwhKa7MDq+Wt5p7fvv1AXhX4HxpgkKv5xbuMWCcw6R8zTYQ4hF/XHegIEqjmwWFxEvD95Lu3oLz4gMEoZVywBt2QFb1wkWUjdeT9oy5YbrJiLm9evhMFWyjnu2h9OVqxCVvarVx35ZySThDr2n3CYntLSKyTSdVlzCsdcCOj1UFkqMe73gOUZFMkXETUoINlFYwX6NP5V1Moy8OjsSNa6/8zyYwivm3rQlj3GUEhSlX+0ib+IXYpcrDFF7/6+G8lWBAHmKGwGR6kpAQ7Zg7KEjY0gSYWOr86oJIMFzeXVjaqhwGXK2tO+JBTPZSf4zljke+QCDN1uZjscgpOOXcBvT3LqLDaz2TSen4EMXhD56lYrq/970a1ol7B26nNAjJr1Q2ZyH4kXgBnK/b7AjYzNhTx0k0o7zRdh4tMeNkxhHgpBQ7d8VM81lZJg95n5SuOvJkJlEsPus9nJ1QeKAAjLV+Hp4n+xEImnvwnPEeE9vo07KHeHsCaBFVVan+9VKMiFEnYO+JdA8DwVTwTHHRH2T2OcEF+oo6m9nZZgGZbcovftryoOetJRY8E2JG+j5ScVWwnh5QcWhP1oOqsZdFWbKmJyxbN0qhKRWB1l6xZipMTj4RYzrZtwXNWdJIudC1Lkr6GgMn2UybLPc4xDH5FLWDtLN7griLweFrniuAQ</cert>
  </endpoints>
</certificate>
"#;

#[cfg(test)]
pub(crate)
const SAMPLE_SIG_XML : &[u8] = br#"<?xml version="1.0" encoding="UTF-8"?>
<signature>
  <intermediates>
    <cert>MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=</cert>
    <cert>MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==</cert>
  </intermediates>
  <certificate>MIIFGTCCAwGgAwIBAgIRAOUOMMnP/H98t0zAwO3YjxIwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUxMDBaFw0yODA5MDYyMTUxMDBaMDUxMzAxBgNVBAMTKkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBTaWduaW5nIEtleTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANqoaDjGHUrdnO6raw9omQ+xnhSxqwTSY2dlC83an+F9JNlL/CHjvn+kyKP7rP57k4y9+9REqjvk+zaR6rQjzP6m2FbYf/kXsmS8ohtTXsmI9NTvobGCGZOYwFbB28yxoOiXA2A91cG+Rt/KmetMcGphFE0/9PGZg9JSmWiGLDJEvgG4ckz6fmL/orhbC/V1K3ArNZ2eJ8Sw29eMo62XpJqvmi+6BrFS3edcJNC1dUpC/ixP73G1J5XDVb60no4JolG1N7Utug/WlPr88eI7LdV05sMfRfX+ta4TrIK7yJ1urGuOVsIDBGFjsfgpRTlwiG829D9uGhRSAE8GzVCFiVF8AfQwlEtgahwg23QzWRaKYo6qeRMCw1hNURF31hQ5bgQeKcaS98x6MkzszBOT2aFiK0EWBzwsJLI3KadRYUMcKa3AFXSv7QLGkAU+Ivas/m3Mt0s7KQnIzjsYbOqiC895WsylxaQyMy5xvVKp0gYjmK2YtgfXo59hznqns1FzeR4fBsbKsh+NnWXzcJ8cEg8jbk0nxAz0reMj1IN25Wb1WDfUCiTy+9V6dfFLQFQ6KYDb/bbIRyPk4g176gWK9agVrHrhiQsDVstSN/cAgLBVUFi1oeLzZ0SwB4wCXuP8SmEVrGl3zxxv3szgUxwfm+elaZ0BrA5deSenJdhV1QQ3AgMBAAGjIDAeMA4GA1UdDwEB/wQEAwIHgDAMBgNVHRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4ICAQDuLSK5nov/grmYNc8CTnrKNZ1w8p5Wbi9QThzJXoSV1BuFklXNX4GlgjZ04eS5ns/lUCdqByx0K2ZGX24wzZX0sSUQ+74Fq5uDINm6ESPV46y6hXvqIotLYIrgpl7Z2Ej7D6JT5fPYzAncUQd8Z9LuNMMt/rG8IlfSN6yOuZnAxI8wKtCrp23QugtqYKHyfxCN/HzCMEs1XP7qhgolnmLoTqU9j2HlPPESmH4+St4w7QPVQWARQ2S0hdtT4dhjmkqeDBojBjkGn9fS+vsOKsH3CDTt3A0pFI66xQ9TwT5mHCIIkAxGzc/DzPtpTUz6XBhtWNyI59adbCHfOtWWNjpriYvTbOm1ZZL6DXsaFJIbYX0Cmh6unonuvZ2c1Pu6nnVxR1HamIdtDZjvgbyFRJ4wCWpMhAU9WVJSotz57OXf/CvbBI0gfhl/EmWtKsGiDryPjphILWrnO55V6G6HJgk6xpzcjZzSnWpf5UF9RGjUaZNwOtxma/57pM8o5vTCeaOrq/3dKUWO2JBgxkOG+/ZCOe0E0Q2CwCCWTtf4ReaUIbeYQTj4cfR4eaj6Z8euytwEM2UQCep+HXJdOxv6/eHRXPK21Alt0crWmhZ8J7hZyeZ/24a3in8hqg9X9wxZXPghXo4W3My3Tn+dP2m36RiBQOCHSoYWMRINZccj9284GQ==</certificate>
  <value>n6kI2dGZKz5CGbXnbz79m51QTDt+WszzNOvcqXsGm6g3ObmpjkghTU3wPmrJ0c5zUD1l4QQEmTKRBIACgK7Sp64JdC4IGP5y+z8HhXPslP3Dc5aySOk4b++m7AIbkAuw63SbPD8L2nQ20CMNiaVVBqZJ0uWUV04qN8IOll1L8NbeZLhjFUcx9riYBrzWOr9uis5IANkfPTFgFyPFjqFk9XrbVpPcNCRtz7Pew+L7OW5z7sh5rW8iZmjhhV/e4VDTgYBFq/Js5W4yalRI9uuEXLJqG1/US4L5cMnJoZOxPmz48an0ug/Pi8yV9cIq+xvER/XaeeUG53Fqy9cn2qG6ROwxH109toaLx3TZaLjdVh7wcJCLtOY6WngHksQbIyU1mDYzz7uWItCss2Nb0NbZ+QMn3k1GxDGIwlY/HXdt7OihPQWLRM2H/QRqlI9p8i1L+DaPrhyGrGHzYKN8z9qGZYx1AsQUWQCR0YeXvlxjtSvBEPtWkfEE0RrZPJtFh+bvrD55Id7XapnGKKXYMmYf9KbDJ3GMD1aT6xgMhlAhtltN5vNg08LSH5Ma4TXhmNpKny5JQqlAUTby1wIhgdElQSdU0jYpmle8N0wsuLoX+e3bHFKxWVkrwvXDC0v2wqH5mzm8FLhxXZDA2ApnGT+eOC1gjd8qTuouzm5GuMhjvig=</value>
</signature>
"#;

/// This is a timestamp at which the sample XML files are valid.
#[cfg(test)]
pub(crate) const SAMPLE_VALIDATION_EPOCH_MILLIS: i64 = 1707344402000;
