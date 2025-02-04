// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DayPeriodTests(pub Vec<DayPeriodTest>);

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DayPeriodTest {
    pub locale: String,
    pub test_cases: Vec<DayPeriodTestCase>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DayPeriodTestCase {
    pub datetimes: Vec<String>,
    pub expectations: Vec<DayPeriodExpectation>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DayPeriodExpectation {
    pub patterns: Vec<String>,
    pub expected: String,
}
