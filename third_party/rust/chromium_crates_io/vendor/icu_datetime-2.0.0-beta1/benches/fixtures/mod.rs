// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_datetime::{fields::components, fieldsets::serde::CompositeFieldSetSerde, options};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Fixture(pub Vec<Test>);

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Test {
    pub setups: Vec<TestInput>,
    pub values: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TestInput {
    pub locale: String,
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

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum TestHourCycle {
    H11,
    H12,
    H23,
    H24,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct PatternsFixture(pub Vec<String>);
