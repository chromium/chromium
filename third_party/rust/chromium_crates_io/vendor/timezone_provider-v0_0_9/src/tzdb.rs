//! `timezone_provider` is the core data provider implementations for `temporal_rs`

// What are we even doing here? Why are providers needed?
//
// Two core data sources need to be accounted for:
//
//   - IANA identifier normalization (hopefully, semi easy)
//   - IANA TZif data (much harder)
//

use std::borrow::Cow;
#[cfg(feature = "datagen")]
use std::{
    collections::{BTreeMap, BTreeSet},
    fs, io,
    path::Path,
};

#[cfg(feature = "datagen")]
use parse_zoneinfo::{
    line::{Line, LineParser},
    table::{Table, TableBuilder},
};

use zerotrie::ZeroAsciiIgnoreCaseTrie;
use zerovec::{VarZeroVec, ZeroVec};

/// A data struct for IANA identifier normalization
#[derive(PartialEq, Debug, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, yoke::Yokeable, serde::Deserialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider))]
pub struct IanaIdentifierNormalizer<'data> {
    /// TZDB version
    pub version: Cow<'data, str>,
    /// An index to the location of the normal identifier.
    #[cfg_attr(feature = "datagen", serde(borrow))]
    pub available_id_index: ZeroAsciiIgnoreCaseTrie<ZeroVec<'data, u8>>,

    /// The normalized IANA identifier
    #[cfg_attr(feature = "datagen", serde(borrow))]
    pub normalized_identifiers: VarZeroVec<'data, str>,
}

// ==== End Data marker implementation ====
#[cfg(feature = "datagen")]
const ZONE_INFO_FILES: [&str; 9] = [
    "africa",
    "antarctica",
    "asia",
    "australasia",
    "backward",
    "etcetera",
    "europe",
    "northamerica",
    "southamerica",
];

#[cfg(feature = "datagen")]
pub struct TzdbDataProvider {
    version: String,
    data: Table,
}

#[cfg(feature = "datagen")]
impl TzdbDataProvider {
    pub fn new(tzdata: &Path) -> Result<Self, io::Error> {
        let parser = LineParser::default();
        let mut builder = TableBuilder::default();

        let version_file = tzdata.join("version");
        let version = fs::read_to_string(version_file)?.trim().into();

        for filename in ZONE_INFO_FILES {
            let file_path = tzdata.join(filename);
            let file = fs::read_to_string(file_path)?;

            for line in file.lines() {
                match parser.parse_str(line) {
                    Ok(Line::Zone(zone)) => builder.add_zone_line(zone).unwrap(),
                    Ok(Line::Continuation(cont)) => builder.add_continuation_line(cont).unwrap(),
                    Ok(Line::Rule(rule)) => builder.add_rule_line(rule).unwrap(),
                    Ok(Line::Link(link)) => builder.add_link_line(link).unwrap(),
                    Ok(Line::Space) => {}
                    Err(e) => eprintln!("{e}"),
                }
            }
        }

        Ok(Self {
            version,
            data: builder.build(),
        })
    }
}

// ==== Begin DataProvider impl ====

#[derive(Debug)]
#[cfg(feature = "datagen")]
pub enum IanaDataError {
    Io(io::Error),
    Build(zerotrie::ZeroTrieBuildError),
}

impl IanaIdentifierNormalizer<'_> {
    #[cfg(feature = "datagen")]
    pub fn build(tzdata: &Path) -> Result<Self, IanaDataError> {
        let provider = TzdbDataProvider::new(tzdata).unwrap();
        let mut identifiers = BTreeSet::default();
        for zoneset_id in provider.data.zonesets.keys() {
            // Add canonical identifiers.
            let _ = identifiers.insert(zoneset_id.clone());
        }
        for links in provider.data.links.keys() {
            // Add link / non-canonical identifiers
            let _ = identifiers.insert(links.clone());
        }

        // Create trie and bin search the index from Vec
        let norm_vec: Vec<String> = identifiers.iter().cloned().collect();
        let norm_zerovec: VarZeroVec<'static, str> = norm_vec.as_slice().into();

        let identier_map: BTreeMap<Vec<u8>, usize> = identifiers
            .iter()
            .map(|id| {
                (
                    id.to_ascii_lowercase().as_bytes().to_vec(),
                    norm_vec.binary_search(id).unwrap(),
                )
            })
            .collect();

        Ok(IanaIdentifierNormalizer {
            version: provider.version.into(),
            available_id_index: ZeroAsciiIgnoreCaseTrie::try_from(&identier_map)
                .map_err(IanaDataError::Build)?
                .convert_store(),
            normalized_identifiers: norm_zerovec,
        })
    }
}

// ==== End DataProvider impl ====
