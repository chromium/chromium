//! Read layout tables in a graph
use crate::fnv::FnvHashMap;
use crate::{
    graph::{Graph, RepackError},
    serialize::{Link, LinkWidth, ObjIdx},
};
use write_fonts::types::{FixedSize, Offset16, Scalar};

pub(super) struct DataBytes<'a> {
    bytes: &'a mut [u8],
}

impl<'a> DataBytes<'a> {
    pub(super) fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let bytes = graph.vertex_data_mut(obj_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
        Ok(Self { bytes })
    }

    /// Read a scalar at the provided location in the data bytes.
    /// caller is responsible for ensuring no out-of-bound reading
    pub(super) fn read_at<T: Scalar>(&self, pos: usize) -> T {
        T::read(&self.bytes[pos..pos + T::RAW_BYTE_LEN]).unwrap()
    }

    /// Write a scalar value over existing data at the provided location in the
    /// data bytes. caller is responsible for ensuring no out-of-bound
    /// writing
    pub(super) fn write_at<T: Scalar>(&mut self, value: T, pos: usize) {
        let src_bytes = value.to_raw();
        self.bytes[pos..pos + T::RAW_BYTE_LEN].copy_from_slice(src_bytes.as_ref());
    }

    pub(super) fn len(&self) -> usize {
        self.bytes.len()
    }
}
pub(crate) struct ExtensionSubtable<'a>(DataBytes<'a>);

impl<'a> ExtensionSubtable<'a> {
    const FORMAT_BYTE_POS: usize = 0;
    const LOOKUP_TYPE_POS: usize = 2;
    pub(crate) fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let ext = graph
            .vertex_data_mut(obj_idx)
            .map(|data| Self(DataBytes { bytes: data }))
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        if !ext.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }
        Ok(ext)
    }

    fn sanitize(&self) -> bool {
        self.0.len() >= EXTENSION_TABLE_SIZE
    }

    fn reset(&mut self, lookup_type: u16) {
        assert!(self.0.len() == EXTENSION_TABLE_SIZE);
        // format = 1
        self.0.write_at(1_u16, Self::FORMAT_BYTE_POS);
        self.0.write_at(lookup_type, Self::LOOKUP_TYPE_POS);
    }

    pub(crate) fn lookup_type(&self) -> u16 {
        self.0.read_at::<u16>(Self::LOOKUP_TYPE_POS)
    }
}

pub(crate) struct Lookup<'a>(DataBytes<'a>);

impl<'a> Lookup<'a> {
    pub(crate) const LOOKUP_MIN_SIZE: usize = 6;
    const LOOKUP_TYPE_POS: usize = 0;
    const LOOKUP_FLAG_POS: usize = 2;
    const NUM_SUBTABLE_BYTE_POS: usize = 4;

    pub(crate) fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let lookup = graph
            .vertex_data_mut(obj_idx)
            .map(|data| Self(DataBytes { bytes: data }))
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        if !lookup.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }
        Ok(lookup)
    }

    fn sanitize(&self) -> bool {
        if self.0.len() < Self::LOOKUP_MIN_SIZE {
            return false;
        }
        let num_subtables = self.num_subtables();

        let size = Self::LOOKUP_MIN_SIZE
            + num_subtables as usize * 2
            + if self.use_mark_filtering_set() { 2 } else { 0 };
        self.0.len() >= size
    }

    pub(crate) fn num_subtables(&self) -> u16 {
        self.0.read_at::<u16>(Self::NUM_SUBTABLE_BYTE_POS)
    }

    fn use_mark_filtering_set(&self) -> bool {
        let lookup_flag = self.0.read_at::<u16>(Self::LOOKUP_FLAG_POS);
        (lookup_flag & 0x0010_u16) != 0
    }

    pub(crate) fn lookup_type(&self) -> u16 {
        self.0.read_at::<u16>(Self::LOOKUP_TYPE_POS)
    }

    fn mark_filtering_set(&self) -> Option<u16> {
        if self.use_mark_filtering_set() {
            let num_subtables = self.num_subtables() as usize;
            let mark_filtering_set =
                self.0.read_at::<u16>(Self::NUM_SUBTABLE_BYTE_POS + 2 + 2 * num_subtables);
            Some(mark_filtering_set)
        } else {
            None
        }
    }

    fn set_lookup_type(&mut self, lookup_type: u16) {
        self.0.write_at(lookup_type, Self::LOOKUP_TYPE_POS);
    }
}

// num of bytes for an extension subtable
pub(crate) const EXTENSION_TABLE_SIZE: usize = 8;
impl Graph {
    fn create_extension_subtable(
        &mut self,
        subtable_idx: ObjIdx,
        lookup_type: u16,
    ) -> Result<ObjIdx, RepackError> {
        let ext_idx = self.new_vertex(EXTENSION_TABLE_SIZE)?;
        let mut ext_subtable = ExtensionSubtable::from_graph(self, ext_idx)?;

        ext_subtable.reset(lookup_type);

        // Make extension point at the subtable
        self.vertices[ext_idx].add_link(LinkWidth::Four, subtable_idx, 4, false);
        Ok(ext_idx)
    }

    fn make_subtable_extension(
        &mut self,
        lookup_idx: ObjIdx,
        subtable_idx: ObjIdx,
        lookup_type: u16,
        idx_map: &mut FnvHashMap<usize, usize>,
    ) -> Result<usize, RepackError> {
        let ext_idx = if let Some(idx) = idx_map.get(&subtable_idx) {
            let subtable_v =
                self.mut_vertex(subtable_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
            subtable_v.remove_parent(lookup_idx, false);
            *idx
        } else {
            let idx = self.create_extension_subtable(subtable_idx, lookup_type)?;
            idx_map.insert(subtable_idx, idx);

            let subtable_v =
                self.mut_vertex(subtable_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
            subtable_v.remap_parent(lookup_idx, idx);
            idx
        };

        for l in self.vertices[lookup_idx].real_links.values_mut() {
            if l.obj_idx() == subtable_idx {
                l.update_obj_idx(ext_idx);
            }
        }
        self.vertices[ext_idx].add_parent(lookup_idx, false);
        Ok(ext_idx)
    }

    pub(crate) fn make_extension(
        &mut self,
        lookup_idx: ObjIdx,
        lookup_type: u16,
        extension_type: Option<u16>,
        idx_map: &mut FnvHashMap<usize, usize>,
    ) -> Result<(), RepackError> {
        let Some(ext_type) = extension_type else {
            return Ok(());
        };
        let lookup_v = self.vertex(lookup_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        let subtable_idxes = lookup_v.child_idxes();
        for subtable_idx in subtable_idxes.keys() {
            self.make_subtable_extension(lookup_idx, *subtable_idx, lookup_type, idx_map)?;
        }

        let mut lookup_table = Lookup::from_graph(self, lookup_idx)?;
        lookup_table.set_lookup_type(ext_type);
        Ok(())
    }

    /// Make a Lookup table at the specified lookup vertex
    pub(crate) fn make_lookup(
        &mut self,
        lookup_index: ObjIdx,
        subtable_idxes: &[ObjIdx],
    ) -> Result<(), RepackError> {
        let lookup = Lookup::from_graph(self, lookup_index)?;
        let mark_filtering_set = lookup.mark_filtering_set();
        let num_subtables = subtable_idxes.len() as u16;

        let lookup_type_flag_data = self
            .vertex_data(lookup_index)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .get(0..Lookup::LOOKUP_MIN_SIZE)
            .ok_or(RepackError::GraphErrorInvalidVertex)?;

        let new_lookup_size = Lookup::LOOKUP_MIN_SIZE
            + subtable_idxes.len() * Offset16::RAW_BYTE_LEN
            + if mark_filtering_set.is_some() { 2 } else { 0 };

        let mut new_lookup_data = vec![0; new_lookup_size];
        new_lookup_data
            .get_mut(0..Lookup::LOOKUP_MIN_SIZE)
            .unwrap()
            .copy_from_slice(lookup_type_flag_data);

        new_lookup_data
            .get_mut(Lookup::NUM_SUBTABLE_BYTE_POS..Lookup::NUM_SUBTABLE_BYTE_POS + 2)
            .unwrap()
            .copy_from_slice(&num_subtables.to_be_bytes());

        if let Some(mark_filtering_set) = mark_filtering_set {
            new_lookup_data
                .get_mut(new_lookup_size - 2..new_lookup_size)
                .unwrap()
                .copy_from_slice(&mark_filtering_set.to_be_bytes());
        }

        self.update_vertex_data(lookup_index, &new_lookup_data)?;

        let start_pos = Lookup::NUM_SUBTABLE_BYTE_POS as u32 + 2;
        let new_lookup_v =
            self.mut_vertex(lookup_index).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
        for i in 0..num_subtables as u32 {
            let subtable_idx =
                subtable_idxes.get(i as usize).ok_or(RepackError::ErrorSplitSubtable)?;

            let pos = start_pos + Offset16::RAW_BYTE_LEN as u32 * i;
            new_lookup_v
                .real_links
                .entry(pos)
                .and_modify(|l| l.update_obj_idx(*subtable_idx))
                .or_insert(Link::new(LinkWidth::Two, *subtable_idx, pos));
        }

        for subtable in subtable_idxes {
            let subtable_v =
                self.mut_vertex(*subtable).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
            if subtable_v.parents.contains_key(&lookup_index) {
                continue;
            }
            subtable_v.add_parent(lookup_index, false);
        }

        Ok(())
    }

    pub(crate) fn add_extension(
        &mut self,
        lookup_type: u16,
        subtables: &mut [ObjIdx],
    ) -> Result<(), RepackError> {
        for subtable_idx in subtables {
            let new_ext_idx = self.create_extension_subtable(*subtable_idx, lookup_type)?;

            self.mut_vertex(*subtable_idx)
                .ok_or(RepackError::GraphErrorInvalidObjIndex)?
                .add_parent(new_ext_idx, false);

            *subtable_idx = new_ext_idx;
        }
        Ok(())
    }
}
