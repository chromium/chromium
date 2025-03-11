// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_datetime::fieldsets::{self, enums::ZoneFieldSet};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TimeZoneTests(pub Vec<TimeZoneTest>);

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TimeZoneTest {
    pub locale: String,
    pub datetime: String,
    pub expectations: HashMap<String, String>,
}

pub fn pattern_to_semantic_skeleton(p: &str) -> Option<ZoneFieldSet> {
    Some(match p {
        "vvvv" => ZoneFieldSet::GenericLong(fieldsets::zone::GenericLong),
        "v" => ZoneFieldSet::GenericShort(fieldsets::zone::GenericShort),
        "VVVV" => ZoneFieldSet::Location(fieldsets::zone::Location),
        "zzzz" => ZoneFieldSet::SpecificLong(fieldsets::zone::SpecificLong),
        "z" => ZoneFieldSet::SpecificShort(fieldsets::zone::SpecificShort),
        "OOOO" => ZoneFieldSet::LocalizedOffsetLong(fieldsets::zone::LocalizedOffsetLong),
        "O" => ZoneFieldSet::LocalizedOffsetShort(fieldsets::zone::LocalizedOffsetShort),
        "VVV" => ZoneFieldSet::ExemplarCity(fieldsets::zone::ExemplarCity),
        // ISO currently untested
        "x" | "xx" | "xxx" | "xxxx" | "xxxxx" | "X" | "XX" | "XXX" | "XXXX" | "XXXXX" => {
            return None
        }
        _ => panic!("unhandled test {p}"),
    })
}
