//! the [GDEF] table
//!
//! [GDEF]: https://docs.microsoft.com/en-us/typography/opentype/spec/gdef

pub use super::layout::{
    ChainedSequenceContext, ClassDef, CoverageTable, Device, FeatureList, FeatureVariations,
    Lookup, LookupList, ScriptList, SequenceContext,
};

use super::variations::ItemVariationStore;

#[cfg(test)]
#[path = "../tests/test_gdef.rs"]
mod tests;

include!("../../generated/generated_gdef.rs");

//include!("../../generated/gpos.rs");

//#[cfg(feature = "compile")]
//pub mod compile {
//use types::{Offset, Offset16, OffsetHost};

//use crate::{
//compile::{FontWrite, TableWriter, ToOwnedObj},
//layout::{compile::CoverageTableBuilder, CoverageTable as CoverageTableRef},
//};

//include!("../../generated/generated_gdef_compile.rs");

//use std::collections::BTreeMap;

//// a more ergonimic representation
//#[derive(Debug, Default, PartialEq)]
//pub struct AttachList {
//pub items: BTreeMap<GlyphId, Vec<u16>>,
//}

//#[derive(Debug, Default, PartialEq)]
//pub struct CaretValueFormat3 {
//pub coordinate: i16,
//pub device_offset: OffsetMarker<Offset16, Box<dyn FontWrite>>,
//}

//impl ToOwnedObj for super::AttachList<'_> {
//type Owned = AttachList;

//fn to_owned_obj(&self, _offset_data: &[u8]) -> Option<Self::Owned> {
//let offset_data = self.bytes();
//let coverage = self
//.coverage_offset()
//.read::<CoverageTableRef>(offset_data)?;

//let attach_points = self.attach_point_offsets().iter().map(|off| {
//off.get()
//.read::<super::AttachPoint>(offset_data)
//.map(|x| {
//x.point_indices()
//.iter()
//.map(|pt| pt.get())
//.collect::<Vec<_>>()
//})
//.expect("invalid offset in AttachList")
//});
//Some(AttachList {
//items: coverage.iter().zip(attach_points).collect(),
//})
//}
//}

//impl FontWrite for AttachList {
//fn write_into(&self, writer: &mut TableWriter) {
//let coverage = self.items.keys().copied().collect::<CoverageTableBuilder>();
//writer.write_offset::<Offset16>(&coverage);
//(self.items.len() as u16).write_into(writer);
//for points in self.items.values() {
//writer.write_offset::<Offset16>(&AttachPointTemp { points })
//}
//}
//}

//struct AttachPointTemp<'a> {
//points: &'a [u16],
//}

//impl FontWrite for AttachPointTemp<'_> {
//fn write_into(&self, writer: &mut TableWriter) {
//(self.points.len() as u16).write_into(writer);
//self.points.write_into(writer);
//}
//}

//impl ToOwnedObj for super::CaretValueFormat3<'_> {
//type Owned = CaretValueFormat3;
//fn to_owned_obj(&self, _offset_data: &[u8]) -> Option<Self::Owned> {
//Some(CaretValueFormat3 {
//coordinate: self.coordinate(),
//device_offset: Default::default(),
//})
//}
//}

//impl FontWrite for CaretValueFormat3 {
//fn write_into(&self, writer: &mut crate::compile::TableWriter) {
//(3 as u16).write_into(writer);
//self.coordinate.write_into(writer);
//self.device_offset.write_into(writer);
//}
//}
//}
