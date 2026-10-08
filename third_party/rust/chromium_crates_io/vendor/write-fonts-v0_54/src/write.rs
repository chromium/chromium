use std::collections::BTreeSet;

#[cfg(feature = "tables")]
use crate::error::{Error, PackingError};
#[cfg(feature = "tables")]
use crate::graph::Graph;
use crate::object::{ObjectId, ObjectStore};
use crate::offsets::OffsetLen;
use crate::table_type::TableType;
#[cfg(feature = "tables")]
use crate::validate::Validate;
#[cfg(feature = "tables")]
use font_types::{FixedSize, Scalar};
#[cfg(feature = "tables")]
use read_fonts::{FontData, FontRead, ReadError};

/// A type that that can be written out as part of a font file.
///
/// This both handles writing big-endian bytes as well as describing the
/// relationship between tables and their subtables.
pub trait FontWrite {
    /// Write our data and information about offsets into this [TableWriter].
    fn write_into(&self, writer: &mut TableWriter);

    /// The type of this table.
    ///
    /// This only matters in cases where a table may require additional processing
    /// after initial compilation, such as with GPOS/GSUB lookups.
    fn table_type(&self) -> TableType {
        TableType::Unknown
    }
}

/// An object that manages a collection of serialized tables.
///
/// This handles deduplicating objects and tracking offsets.
#[derive(Debug)]
pub struct TableWriter {
    /// Finished tables, associated with an ObjectId; duplicate tables share an id.
    tables: ObjectStore,
    /// Tables currently being written.
    ///
    /// Tables are processed as they are encountered (as subtables)
    stack: Vec<TableData>,
    /// An adjustment factor subtracted from written offsets.
    ///
    /// This is '0', unless a particular offset is expected to be relative some
    /// position *other* than the start of the table.
    ///
    /// This should only ever be non-zero in the body of a closure passed to
    /// [adjust_offsets](Self::adjust_offsets)
    offset_adjustment: u32,
}

/// Attempt to serialize a table.
///
/// Returns an error if the table is malformed or cannot otherwise be serialized,
/// otherwise it will return the bytes encoding the table.
#[cfg(feature = "tables")]
pub fn dump_table<T: FontWrite + Validate>(table: &T) -> Result<Vec<u8>, Error> {
    log::trace!("writing table '{}'", table.table_type());
    table.validate()?;
    let mut graph = TableWriter::make_graph(table);

    if !graph.pack_objects() {
        return Err(Error::PackingFailed(PackingError {
            graph: graph.into(),
        }));
    }
    Ok(graph.serialize())
}

impl TableWriter {
    /// A convenience method for generating a graph with the provided root object.
    #[cfg(feature = "tables")]
    pub(crate) fn make_graph(root: &impl FontWrite) -> Graph {
        let mut writer = TableWriter::default();
        let root_id = writer.add_table(root);
        Graph::from_obj_store(writer.tables, root_id)
    }

    fn add_table(&mut self, table: &dyn FontWrite) -> ObjectId {
        self.stack.push(TableData::default());
        table.write_into(self);
        let mut table_data = self.stack.pop().unwrap();
        table_data.type_ = table.table_type();
        self.tables.add(table_data)
    }

    /// Call the provided closure, adjusting any written offsets by `adjustment`.
    #[cfg(feature = "tables")]
    pub(crate) fn adjust_offsets(&mut self, adjustment: u32, f: impl FnOnce(&mut TableWriter)) {
        self.offset_adjustment = adjustment;
        f(self);
        self.offset_adjustment = 0;
    }

    /// Write raw bytes into this table.
    ///
    /// The caller is responsible for ensuring bytes are in big-endian order.
    #[inline]
    pub fn write_slice(&mut self, bytes: &[u8]) {
        self.stack.last_mut().unwrap().write_bytes(bytes)
    }

    /// Create an offset to another table.
    ///
    /// The `width` argument is the size in bytes of the offset, e.g. 2 for
    /// an `Offset16`, and 4 for an `Offset32`.
    ///
    /// The provided table will be serialized immediately, and the position
    /// of the offset within the current table will be recorded. Offsets
    /// are resolved when the root table object is serialized, at which point
    /// we overwrite each recorded offset position with the final offset of the
    /// appropriate table.
    pub fn write_offset(&mut self, obj: &dyn FontWrite, width: usize) {
        let obj_id = self.add_table(obj);
        let data = self.stack.last_mut().unwrap();
        data.add_offset(obj_id, width, self.offset_adjustment);
    }

    /// Add a padding byte of necessary to ensure the table length is an even number.
    ///
    /// This is necessary for things like the glyph table, which require offsets
    /// to be aligned on 2-byte boundaries.
    #[cfg(feature = "tables")]
    pub fn pad_to_2byte_aligned(&mut self) {
        if self.stack.last().unwrap().bytes.len() % 2 != 0 {
            self.write_slice(&[0]);
        }
    }

    /// used when writing top-level font objects, which are done more manually.
    pub(crate) fn into_data(mut self) -> TableData {
        assert_eq!(self.stack.len(), 1);
        let result = self.stack.pop().unwrap();
        assert!(result.offsets.is_empty());
        result
    }

    /// A reference to the current table data.
    ///
    /// This is currently only used to figure out the glyph positions when
    /// compiling the glyf table.
    #[cfg(feature = "tables")]
    pub(crate) fn current_data(&self) -> &TableData {
        self.stack.last().unwrap() // there is always at least one
    }
}

impl Default for TableWriter {
    fn default() -> Self {
        TableWriter {
            tables: ObjectStore::default(),
            stack: vec![TableData::default()],
            offset_adjustment: 0,
        }
    }
}

/// The encoded data for a given table, along with info on included offsets
#[derive(Debug, Default, Clone)] // DO NOT DERIVE MORE TRAITS! we want to ignore name field
pub(crate) struct TableData {
    pub(crate) type_: TableType,
    pub(crate) bytes: Vec<u8>,
    pub(crate) offsets: Vec<OffsetRecord>,
}

impl std::hash::Hash for TableData {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.bytes.hash(state);
        self.offsets.hash(state);
    }
}

impl PartialEq for TableData {
    fn eq(&self, other: &Self) -> bool {
        self.bytes == other.bytes && self.offsets == other.offsets
    }
}

impl Eq for TableData {}

/// The position and type of an offset, along with the id of the pointed-to entity
#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub(crate) struct OffsetRecord {
    /// the position of the offset within the parent table
    pub(crate) pos: u32,
    /// the offset length in bytes
    pub(crate) len: OffsetLen,
    /// The object pointed to by the offset
    pub(crate) object: ObjectId,
    /// a value subtracted from the resolved offset before writing.
    ///
    /// In general we assume that offsets are relative to the start of the parent
    /// table, but in some cases this is not true (for instance, offsets to
    /// strings in the name table are relative to the end of the table.)
    pub(crate) adjustment: u32,
}

impl TableData {
    #[cfg(feature = "tables")]
    pub(crate) fn new(type_: TableType) -> Self {
        TableData {
            type_,
            ..Default::default()
        }
    }

    /// the 'adjustment' param is used to modify the written position.
    pub(crate) fn add_offset(&mut self, object: ObjectId, width: usize, adjustment: u32) {
        const PLACEHOLDER_BYTES: &[u8] = &[0xff; 4];
        self.offsets.push(OffsetRecord {
            pos: self.bytes.len() as u32,
            len: match width {
                2 => OffsetLen::Offset16,
                3 => OffsetLen::Offset24,
                _ => OffsetLen::Offset32,
            },
            object,
            adjustment,
        });

        // we don't want to use zeros as our placeholder, since we want to
        // distinguish from a null offset during splitting/promotion.
        // we write all ones, since maybe it will stand out in debugging
        let placeholder = PLACEHOLDER_BYTES.get(..width.min(4)).unwrap();
        self.write_bytes(placeholder);
    }

    #[cfg(feature = "tables")]
    pub(crate) fn write<T: Scalar>(&mut self, value: T) {
        self.write_bytes(value.to_raw().as_ref())
    }

    /// Write the value over existing data at the provided position.
    ///
    /// Only used in very special cases. The caller is responsible for knowing
    /// what they are doing.
    #[cfg(feature = "tables")]
    pub(crate) fn write_over<T: Scalar>(&mut self, value: T, pos: usize) {
        let raw = value.to_raw();
        let len = raw.as_ref().len();
        self.bytes[pos..pos + len].copy_from_slice(raw.as_ref());
    }

    fn write_bytes(&mut self, bytes: &[u8]) {
        self.bytes.extend_from_slice(bytes)
    }

    /// A helper function to reparse this table data as some type.
    ///
    /// Used internally when modifying the graph after initial compilation,
    /// such as during table splitting.
    #[cfg(feature = "tables")]
    pub(crate) fn reparse<'a, T: FontRead<'a, Args = ()>>(&'a self) -> Result<T, ReadError> {
        let data = FontData::new(&self.bytes);
        T::read(data)
    }

    // see above
    #[cfg(feature = "tables")]
    pub(crate) fn reparse_with_args<'a, A, T: FontRead<'a, Args = A>>(
        &'a self,
        args: A,
    ) -> Result<T, ReadError> {
        let data = FontData::new(&self.bytes);
        T::read_with_args(data, args)
    }

    /// A helper function to read a value out of this data.
    #[cfg(feature = "tables")]
    pub(crate) fn read_at<T: Scalar>(&self, pos: usize) -> Option<T> {
        let len = T::RAW_BYTE_LEN;
        self.bytes.get(pos..pos + len).and_then(T::read)
    }

    #[cfg(all(test, feature = "tables"))]
    pub fn make_mock(size: usize) -> Self {
        TableData {
            bytes: vec![0xca; size], // has no special meaning
            offsets: Vec::new(),
            type_: TableType::MockTable,
        }
    }

    #[cfg(all(test, feature = "tables"))]
    pub fn add_mock_offset(&mut self, object: ObjectId, len: OffsetLen) {
        let pos = self.offsets.iter().map(|off| off.len as u8 as u32).sum();
        self.offsets.push(OffsetRecord {
            pos,
            len,
            object,
            adjustment: 0,
        });
    }
}

macro_rules! write_be_bytes {
    ($ty:ty) => {
        impl FontWrite for $ty {
            #[inline]
            fn write_into(&self, writer: &mut TableWriter) {
                writer.write_slice(&self.to_be_bytes())
            }
        }
    };
}

//NOTE: not implemented for offsets! it would be too easy to accidentally write them.
write_be_bytes!(u8);
write_be_bytes!(i8);
write_be_bytes!(u16);
write_be_bytes!(i16);
write_be_bytes!(u32);
write_be_bytes!(i32);
write_be_bytes!(i64);
write_be_bytes!(types::Uint24);
write_be_bytes!(types::Int24);
write_be_bytes!(types::F2Dot14);
write_be_bytes!(types::F4Dot12);
write_be_bytes!(types::F6Dot10);
write_be_bytes!(types::Fixed);
write_be_bytes!(types::FWord);
write_be_bytes!(types::UfWord);
write_be_bytes!(types::LongDateTime);
write_be_bytes!(types::Tag);
write_be_bytes!(types::Version16Dot16);
write_be_bytes!(types::MajorMinor);
write_be_bytes!(types::GlyphId16);
write_be_bytes!(types::NameId);

impl<T: FontWrite> FontWrite for [T] {
    fn write_into(&self, writer: &mut TableWriter) {
        self.iter().for_each(|item| item.write_into(writer))
    }
}

impl<T: FontWrite> FontWrite for BTreeSet<T> {
    fn write_into(&self, writer: &mut TableWriter) {
        self.iter().for_each(|item| item.write_into(writer))
    }
}

impl<T: FontWrite> FontWrite for Vec<T> {
    fn write_into(&self, writer: &mut TableWriter) {
        self.iter().for_each(|item| item.write_into(writer))
    }
}

impl<T: FontWrite> FontWrite for Option<T> {
    fn write_into(&self, writer: &mut TableWriter) {
        if let Some(obj) = self {
            obj.write_into(writer)
        }
    }
}
