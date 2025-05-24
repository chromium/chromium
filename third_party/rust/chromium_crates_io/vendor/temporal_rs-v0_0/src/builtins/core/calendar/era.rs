//! Calendar Eras constants

// The general source for this implementation as of 2024-08-28 is the intl-era-monthcode proposal.
//
// As this source is currently a proposal, its content are subject to change, so full era support
// should be viewed as experimental.
//
// Source: https://tc39.es/proposal-intl-era-monthcode/

// TODO (0.1.0): Feature flag certain eras as experimental

use core::ops::RangeInclusive;

use tinystr::{tinystr, TinyAsciiStr};

/// Relevant Era info.
pub(crate) struct EraInfo {
    pub(crate) name: TinyAsciiStr<16>,
    pub(crate) range: RangeInclusive<i32>,
}

macro_rules! era_identifier {
    ($name:literal) => {
        tinystr!(19, $name)
    };
}

macro_rules! valid_era {
    ($name:literal, $range:expr ) => {
        EraInfo {
            name: tinystr!(16, $name),
            range: $range,
        }
    };
}

pub(crate) const BUDDHIST_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("buddhist"), era_identifier!("be")];

pub(crate) const ETHIOPIC_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("ethiopic"), era_identifier!("incar")];

pub(crate) const ETHIOPIC_ETHOPICAA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 3] = [
    era_identifier!("ethioaa"),
    era_identifier!("ethiopic-amete-alem"), // TODO: probably will break?
    era_identifier!("mundi"),
];

pub(crate) const ETHIOAA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 3] = [
    era_identifier!("ethioaa"),
    era_identifier!("ethiopic-amete-alem"), // TODO: probably will break?
    era_identifier!("mundi"),
];

pub(crate) const GREGORY_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 3] = [
    era_identifier!("gregory"),
    era_identifier!("ce"),
    era_identifier!("ad"),
];

pub(crate) const GREGORY_INVERSE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 3] = [
    era_identifier!("gregory-inverse"),
    era_identifier!("bc"),
    era_identifier!("bce"),
];

pub(crate) const HEBREW_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("hebrew"), era_identifier!("am")];

pub(crate) const INDIAN_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("indian"), era_identifier!("saka")];

pub(crate) const ISLAMIC_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("islamic"), era_identifier!("ah")];

pub(crate) const ISLAMIC_CIVIL_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 3] = [
    era_identifier!("islamic-civil"),
    era_identifier!("islamicc"),
    era_identifier!("ah"),
];

// TODO: Support islamic-rgsa
pub(crate) const _ISLAMIC_RGSA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("islamic-rgsa"), era_identifier!("ah")];

pub(crate) const ISLAMIC_TBLA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("islamic-tbla"), era_identifier!("ah")];

pub(crate) const ISLAMIC_UMALQURA_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("islamic-umalqura"), era_identifier!("ah")];

pub(crate) const JAPANESE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 4] = [
    era_identifier!("japanese"),
    era_identifier!("gregory"),
    era_identifier!("ce"),
    era_identifier!("ad"),
];

pub(crate) const JAPANESE_INVERSE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 4] = [
    era_identifier!("japanese-inverse"),
    era_identifier!("gregory-inverse"),
    era_identifier!("bc"),
    era_identifier!("bce"),
];

pub(crate) const PERSIAN_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("persian"), era_identifier!("ap")];

pub(crate) const ROC_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] =
    [era_identifier!("roc"), era_identifier!("minguo")];

pub(crate) const ROC_INVERSE_ERA_IDENTIFIERS: [TinyAsciiStr<19>; 2] = [
    era_identifier!("roc-inverse"),
    era_identifier!("before-roc"),
];

// NOTE: The below currently might not align 100% with ICU4X.
// TODO: Update to align with ICU4X depending on any Era updates.
pub(crate) const ISO_ERA: EraInfo = valid_era!("default", i32::MIN..=i32::MAX);
pub(crate) const BUDDHIST_ERA: EraInfo = valid_era!("buddhist", i32::MIN..=i32::MAX);
pub(crate) const CHINESE_ERA: EraInfo = valid_era!("chinese", i32::MIN..=i32::MAX);
pub(crate) const COPTIC_ERA: EraInfo = valid_era!("coptic", 1..=i32::MAX);
pub(crate) const COPTIC_INVERSE_ERA: EraInfo = valid_era!("coptic-inverse", 1..=i32::MAX);
pub(crate) const DANGI_ERA: EraInfo = valid_era!("dangi", i32::MIN..=i32::MAX);
pub(crate) const ETHIOPIC_ERA: EraInfo = valid_era!("ethiopic", 1..=i32::MAX);
pub(crate) const ETHIOPIC_ETHIOAA_ERA: EraInfo = valid_era!("ethioaa", i32::MIN..=5500);
pub(crate) const ETHIOAA_ERA: EraInfo = valid_era!("ethioaa", i32::MIN..=i32::MAX);
pub(crate) const GREGORY_ERA: EraInfo = valid_era!("gregory", 1..=i32::MAX);
pub(crate) const GREGORY_INVERSE_ERA: EraInfo = valid_era!("gregory-inverse", 1..=i32::MAX);
pub(crate) const HEBREW_ERA: EraInfo = valid_era!("hebrew", i32::MIN..=i32::MAX);
pub(crate) const INDIAN_ERA: EraInfo = valid_era!("indian", i32::MIN..=i32::MAX);
pub(crate) const ISLAMIC_ERA: EraInfo = valid_era!("islamic", i32::MIN..=i32::MAX);
pub(crate) const ISLAMIC_CIVIL_ERA: EraInfo = valid_era!("islamic-civil", i32::MIN..=i32::MAX);
// TODO: Support islamic-rgsa
pub(crate) const _ISLAMIC_RGSA_ERA: EraInfo = valid_era!("islamic-rgsa", i32::MIN..=i32::MAX);
pub(crate) const ISLAMIC_TBLA_ERA: EraInfo = valid_era!("islamic-tbla", i32::MIN..=i32::MAX);
pub(crate) const ISLAMIC_UMALQURA_ERA: EraInfo =
    valid_era!("islamic-umalqura", i32::MIN..=i32::MAX);
pub(crate) const HEISEI_ERA: EraInfo = valid_era!("heisei", 1..=31);
pub(crate) const JAPANESE_ERA: EraInfo = valid_era!("japanese", 1..=1868);
pub(crate) const JAPANESE_INVERSE_ERA: EraInfo = valid_era!("japanese-inverse", 1..=i32::MAX);
pub(crate) const MEJEI_ERA: EraInfo = valid_era!("mejei", 1..=45);
pub(crate) const REIWA_ERA: EraInfo = valid_era!("reiwa", 1..=i32::MAX);
pub(crate) const SHOWA_ERA: EraInfo = valid_era!("showa", 1..=64);
pub(crate) const TAISHO_ERA: EraInfo = valid_era!("showa", 1..=45);
pub(crate) const PERSIAN_ERA: EraInfo = valid_era!("persian", i32::MIN..=i32::MAX);
pub(crate) const ROC_ERA: EraInfo = valid_era!("roc", 1..=i32::MAX);
pub(crate) const ROC_INVERSE_ERA: EraInfo = valid_era!("roc-inverse", 1..=i32::MAX);
