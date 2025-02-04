// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "serde")]

use icu_datetime::fields::components;
use icu_datetime::{fieldsets::serde::CompositeFieldSetSerde, options};
use icu_locale_core::preferences::extensions::unicode::keywords::HourCycle;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Fixture(pub Vec<Test>);

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Test {
    pub description: Option<String>,
    pub input: TestInput,
    pub output: TestOutput,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TestInput {
    pub value: String,
    pub options: TestOptions,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TestOptions {
    pub length: Option<TestOptionsLength>,
    pub components: Option<TestComponentsBag>,
    pub semantic: Option<CompositeFieldSetSerde>,
    #[serde(rename = "hourCycle")]
    pub hour_cycle: Option<TestHourCycle>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TestOptionsLength {
    pub date: Option<TestLength>,
    pub time: Option<TestLength>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum TestLength {
    #[serde(rename = "short")]
    Short,
    #[serde(rename = "medium")]
    Medium,
    #[serde(rename = "long")]
    Long,
    #[serde(rename = "full")]
    Full,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TestComponentsBag {
    pub era: Option<components::Text>,
    pub year: Option<components::Year>,
    pub month: Option<components::Month>,
    pub week: Option<components::Week>,
    pub day: Option<components::Day>,
    pub weekday: Option<components::Text>,

    pub hour: Option<components::Numeric>,
    pub minute: Option<components::Numeric>,
    pub second: Option<components::Numeric>,
    pub fractional_second: Option<options::FractionalSecondDigits>,

    pub time_zone_name: Option<components::TimeZoneName>,
}

#[derive(Debug, Copy, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum TestHourCycle {
    H11,
    H12,
    H23,
    H24,
}

impl From<TestHourCycle> for HourCycle {
    fn from(value: TestHourCycle) -> Self {
        match value {
            TestHourCycle::H11 => HourCycle::H11,
            TestHourCycle::H12 => HourCycle::H12,
            TestHourCycle::H23 => HourCycle::H23,
            TestHourCycle::H24 => HourCycle::H24,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TestOutput {
    // Key is locale, and value is expected test output.
    pub values: HashMap<String, TestOutputItem>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum TestOutputItem {
    ExpectedString(String),
    ExpectedStringAndPattern { formatted: String, pattern: String },
}

impl TestOutputItem {
    pub fn expectation(&self) -> &str {
        match self {
            Self::ExpectedString(s) => s,
            Self::ExpectedStringAndPattern { formatted, .. } => formatted,
        }
    }

    pub fn pattern(&self) -> Option<&str> {
        match self {
            Self::ExpectedString(_) => None,
            Self::ExpectedStringAndPattern { pattern, .. } => Some(pattern),
        }
    }
}
