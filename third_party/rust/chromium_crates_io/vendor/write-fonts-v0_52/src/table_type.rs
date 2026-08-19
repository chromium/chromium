//! Identifiers for specific tables
//!
//! These are used to record the type of certain serialized tables & subtables
//! that may require special attention while compiling the object graph.

use std::fmt::Display;

use font_types::Tag;

#[cfg(feature = "tables")]
use crate::tables::layout::LookupType;

/// A marker for identifying the original source of various compiled tables.
///
/// In the general case, once a table has been compiled we do not need to know
/// what the bytes represent; however in certain special cases we do need this
/// information, in order to try alternate compilation strategies.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
#[cfg_attr(not(feature = "tables"), allow(dead_code))]
#[non_exhaustive]
pub enum TableType {
    /// An unknown table
    #[default]
    Unknown,
    /// An untyped table generated for testing
    MockTable,
    /// A table with a given name (the name is used for debugging)
    Named(&'static str),
    /// A top-level table
    TopLevel(Tag),
    GposLookup(u16),
    GsubLookup(u16),
}

impl TableType {
    #[cfg(feature = "dot2")]
    pub(crate) fn is_mock(&self) -> bool {
        *self == TableType::MockTable
    }
}

#[cfg(feature = "tables")]
impl TableType {
    pub(crate) fn is_promotable(self) -> bool {
        match self {
            TableType::GposLookup(type_) => type_ != LookupType::GPOS_EXT_TYPE,
            TableType::GsubLookup(type_) => type_ != LookupType::GSUB_EXT_TYPE,
            _ => false,
        }
    }

    pub(crate) fn is_splittable(self) -> bool {
        matches!(
            self,
            TableType::GposLookup(LookupType::PAIR_POS)
                | TableType::GposLookup(LookupType::MARK_TO_BASE)
        )
    }

    pub(crate) fn to_lookup_type(self) -> Option<LookupType> {
        match self {
            TableType::GposLookup(type_) => Some(LookupType::Gpos(type_)),
            TableType::GsubLookup(type_) => Some(LookupType::Gsub(type_)),
            _ => None,
        }
    }
}

#[cfg(feature = "tables")]
impl From<LookupType> for TableType {
    fn from(src: LookupType) -> TableType {
        match src {
            LookupType::Gpos(type_) => TableType::GposLookup(type_),
            LookupType::Gsub(type_) => TableType::GsubLookup(type_),
        }
    }
}

impl Display for TableType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            TableType::Unknown => write!(f, "Unknown"),
            TableType::MockTable => write!(f, "MockTable"),
            TableType::Named(name) => name.fmt(f),
            TableType::TopLevel(tag) => tag.fmt(f),
            TableType::GposLookup(gpos) => match gpos {
                1 => "GPOS1Single",
                2 => "GPOS2Pair",
                3 => "GPOS3Cursive",
                4 => "GPOS4MarkToBase",
                5 => "GPOS5MarkToLig",
                6 => "GPOS6MarkToMark",
                7 => "GPOS7Context",
                8 => "GPOS8Chain",
                9 => "GPOS9Extension",
                _ => unreachable!("never instantiated"),
            }
            .fmt(f),
            TableType::GsubLookup(gsub) => match gsub {
                1 => "GSUB1Single",
                2 => "GSUB2Multiple",
                3 => "GSUB3ALternate",
                4 => "GSUB4Ligature",
                5 => "GSUB5Context",
                6 => "GSUB6Chain",
                7 => "GSUB7Extension",
                8 => "GSUB8ReverseChain",
                _ => unreachable!("never instantiated"),
            }
            .fmt(f),
        }
    }
}

#[cfg(all(test, feature = "tables"))]
mod tests {
    use crate::tables::{
        gpos, gsub,
        layout::{Lookup, LookupFlag},
    };
    use crate::FontWrite;

    use super::*;

    #[test]
    fn tagged_table_type() {
        assert_eq!(
            gsub::Gsub::default().table_type(),
            TableType::TopLevel(Tag::new(b"GSUB"))
        );
        assert_eq!(
            gpos::Gpos::default().table_type(),
            TableType::TopLevel(Tag::new(b"GPOS"))
        );

        assert_eq!(
            crate::tables::name::Name::default().table_type(),
            TableType::TopLevel(Tag::new(b"name"))
        );
    }

    #[test]
    fn promotable() {
        assert_eq!(
            gsub::SubstitutionLookup::Single(Default::default()).table_type(),
            TableType::GsubLookup(1)
        );
        assert_eq!(
            gsub::SubstitutionLookup::Extension(Default::default()).table_type(),
            TableType::GsubLookup(7)
        );
    }

    #[test]
    fn gpos_type() {
        let pairpos = gpos::PairPos::Format1(Default::default());
        let lookup = gpos::PositionLookup::Pair(Lookup::new(LookupFlag::empty(), vec![pairpos]));
        assert_eq!(lookup.table_type(), TableType::GposLookup(2));
    }
}
