use super::CompiledNormalizer;
use crate::provider::TimeZoneNormalizer;
use alloc::string::String;
use alloc::vec::Vec;
use serde::{self, Deserialize};

const CLDR_TIMEZONES: &str = include_str!("cldr-timezone.xml");

#[derive(Deserialize, Debug)]
struct Document {
    keyword: Keyword,
}

#[derive(Deserialize, Debug)]
struct Keyword {
    key: Key,
}

#[derive(Deserialize, Debug)]
struct Key {
    #[serde(rename = "type")]
    tzs: Vec<TimeZone>,
}

#[derive(Deserialize, Debug)]
struct TimeZone {
    #[serde(rename = "@alias")]
    aliases: Option<String>,
    #[serde(rename = "@iana")]
    iana: Option<String>,
}

/// This tests against CLDR's timezones.xml
/// which is known to be closer to the spec for
/// <https://tc39.es/ecma402/#sec-use-of-iana-time-zone-database>
#[test]
fn test_cldr_timezones() {
    let doc: Document = serde_xml_rs::from_str(CLDR_TIMEZONES).unwrap();

    for tz in doc.keyword.key.tzs {
        if let Some(aliases) = tz.aliases {
            let aliases: Vec<_> = aliases.split(" ").collect();

            // The primary string is either the first timezone, or the `iana` field if present
            let primary_str = if let Some(iana) = tz.iana.as_ref() {
                iana
            } else {
                aliases[0]
            };
            if primary_str.starts_with("Etc") {
                // These are handled elsewhere
                continue;
            }
            let primary = CompiledNormalizer
                .normalized(primary_str.as_bytes())
                .expect(primary_str);

            // We want to ensure they all canonicalize
            for alias in aliases {
                if alias == "Canada/East-Saskatchewan" || alias == "US/Pacific-New" {
                    // These are present in CLDR but not tzdb. Special case as known exceptions.
                    continue;
                }
                let normalized = CompiledNormalizer.normalized(alias.as_bytes()).unwrap();
                let canonicalized = CompiledNormalizer.canonicalized(normalized).unwrap();

                assert_eq!(
                    canonicalized, primary,
                    "{alias} should canonicalize to the same thing as {primary_str}"
                );
            }
        }
    }
}
