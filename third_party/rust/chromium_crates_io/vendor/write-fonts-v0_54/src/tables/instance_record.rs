//! An fvar InstanceRecord

#[allow(unused_imports)]
use crate::codegen_prelude::*;

/// The [InstanceRecord](https://learn.microsoft.com/en-us/typography/opentype/spec/fvar#instancerecord)
#[derive(Debug, Clone, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct InstanceRecord {
    /// The name ID for entries in the 'name' table that provide subfamily names for this instance.
    pub subfamily_name_id: NameId,
    /// Reserved for future use — set to 0.
    pub flags: u16,
    /// The coordinates array for this instance.
    pub coordinates: Vec<Fixed>,
    /// Optional. The name ID for entries in the 'name' table that provide PostScript names for this instance.
    pub post_script_name_id: Option<NameId>,
}

impl FontWrite for InstanceRecord {
    fn write_into(&self, writer: &mut TableWriter) {
        self.subfamily_name_id.write_into(writer);
        self.flags.write_into(writer);
        self.coordinates.write_into(writer);
        if let Some(name_id) = self.post_script_name_id {
            name_id.write_into(writer);
        }
    }
}

impl Validate for InstanceRecord {
    fn validate_impl(&self, _ctx: &mut ValidationCtx) {}
}

impl<'a> FromObjRef<read_fonts::tables::fvar::InstanceRecord<'a>> for InstanceRecord {
    fn from_obj_ref(from: &read_fonts::tables::fvar::InstanceRecord<'a>, _data: FontData) -> Self {
        InstanceRecord {
            subfamily_name_id: from.subfamily_name_id,
            flags: from.flags,
            coordinates: from.coordinates.iter().map(|be| be.get()).collect(),
            post_script_name_id: from.post_script_name_id,
        }
    }
}
