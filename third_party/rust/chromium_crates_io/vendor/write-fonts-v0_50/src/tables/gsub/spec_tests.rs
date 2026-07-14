use super::*;
use crate::assert_hex_eq;
use font_test_data::gsub as test_data;

#[test]
fn singlesubstformat1() {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/gsub#example-2-singlesubstformat1-subtable
    let table = SingleSubstFormat1::read(test_data::SINGLESUBSTFORMAT1_TABLE.into()).unwrap();
    let dumped = crate::write::dump_table(&table).unwrap();
    assert_hex_eq!(test_data::SINGLESUBSTFORMAT1_TABLE, &dumped);
}

#[test]
fn singlesubstformat2() {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/gsub#example-3-singlesubstformat2-subtable
    let table = SingleSubstFormat2::read(test_data::SINGLESUBSTFORMAT2_TABLE.into()).unwrap();
    let dumped = crate::write::dump_table(&table).unwrap();
    assert_hex_eq!(test_data::SINGLESUBSTFORMAT2_TABLE, &dumped);
}

#[test]
fn multiplesubstformat1() {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/gsub#example-4-multiplesubstformat1-subtable
    let table = MultipleSubstFormat1::read(test_data::MULTIPLESUBSTFORMAT1_TABLE.into()).unwrap();
    let dumped = crate::write::dump_table(&table).unwrap();
    assert_hex_eq!(test_data::MULTIPLESUBSTFORMAT1_TABLE, &dumped);
}

#[test]
fn alternatesubstformat1() {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/gsub#example-5-alternatesubstformat-1-subtable
    let table = AlternateSubstFormat1::read(test_data::ALTERNATESUBSTFORMAT1_TABLE.into()).unwrap();
    let dumped = crate::write::dump_table(&table).unwrap();
    assert_hex_eq!(test_data::ALTERNATESUBSTFORMAT1_TABLE, &dumped);
}

#[test]
fn ligaturesubstformat1() {
    // https://learn.microsoft.com/en-us/typography/opentype/spec/gsub#example-6-ligaturesubstformat1-subtable
    let table = LigatureSubstFormat1::read(test_data::LIGATURESUBSTFORMAT1_TABLE.into()).unwrap();
    let dumped = crate::write::dump_table(&table).unwrap();
    assert_hex_eq!(test_data::LIGATURESUBSTFORMAT1_TABLE, &dumped);
}
