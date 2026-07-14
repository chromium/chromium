//! serializer
//! ported from Harfbuzz Serializer: <https://github.com/harfbuzz/harfbuzz/blob/5e32b5ca8fe430132b87c0eee6a1c056d37c35eb/src/hb-serialize.hh>
use core::ops::Range;
use std::{
    hash::{Hash, Hasher},
    mem,
};

use crate::fnv::FnvHasher;
use hashbrown::HashTable;
use std::collections::BTreeMap;
use write_fonts::types::{FixedSize, Scalar, Uint24};

/// An error which occurred during the serialization of a table using
/// Serializer.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SerializeErrorFlags(u16);

impl std::fmt::Display for SerializeErrorFlags {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "Error during serialization, error flags: {:016b}", self.0)
    }
}

impl SerializeErrorFlags {
    pub const SERIALIZE_ERROR_NONE: Self = Self(0x0000);
    pub const SERIALIZE_ERROR_OTHER: Self = Self(0x0001);
    pub const SERIALIZE_ERROR_OFFSET_OVERFLOW: Self = Self(0x0002);
    pub const SERIALIZE_ERROR_OUT_OF_ROOM: Self = Self(0x0004);
    pub const SERIALIZE_ERROR_INT_OVERFLOW: Self = Self(0x0008);
    pub const SERIALIZE_ERROR_ARRAY_OVERFLOW: Self = Self(0x0010);
    pub const SERIALIZE_ERROR_READ_ERROR: Self = Self(0x0020);
    pub const SERIALIZE_ERROR_EMPTY: Self = Self(0x0040);

    fn contains(&self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }
}

impl Default for SerializeErrorFlags {
    fn default() -> Self {
        Self::SERIALIZE_ERROR_NONE
    }
}

impl std::ops::BitOrAssign for SerializeErrorFlags {
    /// Adds the set of flags.
    #[inline]
    fn bitor_assign(&mut self, other: Self) {
        self.0 |= other.0;
    }
}

impl std::ops::Not for SerializeErrorFlags {
    type Output = bool;
    #[inline]
    fn not(self) -> bool {
        self == SerializeErrorFlags::SERIALIZE_ERROR_NONE
    }
}

#[derive(Default, Eq, PartialEq, Clone, Copy, Hash, Debug)]
/// Marks where an offset is relative from.
///
/// Offset relative to the current object head (default)/tail or
/// Absolute: from the start of the serialize buffer
pub enum OffsetWhence {
    #[default]
    Head,
    Tail,
    Absolute,
}

type PoolIdx = usize;

// Reference Harfbuzz implementation:
// <https://github.com/harfbuzz/harfbuzz/blob/5e32b5ca8fe430132b87c0eee6a1c056d37c35eb/src/hb-serialize.hh#L69>
#[derive(Default)]
pub(crate) struct Object {
    // head/tail: indices of the output buffer for this object
    head: usize,
    tail: usize,
    // real links are associated with actual offsets
    real_links: Vec<Link>,
    // virtual links not associated with actual offsets,
    // they exist merely to enforce an ordering constraint.
    virtual_links: Vec<Link>,

    // pool index of the object that will be worked on next
    next_obj: Option<PoolIdx>,
}

impl Object {
    fn reset(&mut self) {
        self.real_links.clear();
        self.virtual_links.clear();
    }

    fn add_virtual_link(&mut self, obj_idx: ObjIdx) {
        let link = Link {
            width: LinkWidth::default(),
            is_signed: false,
            whence: OffsetWhence::default(),
            bias: 0,
            position: 0,
            objidx: obj_idx,
        };

        self.virtual_links.push(link);
    }

    pub(crate) fn head(&self) -> usize {
        self.head
    }

    pub(crate) fn tail(&self) -> usize {
        self.tail
    }

    // used by repacker only, real_links vector is converted to BTreeMap
    pub(crate) fn real_links(&self) -> BTreeMap<u32, Link> {
        self.real_links.iter().map(|l| (l.position, *l)).collect()
    }

    pub(crate) fn virtual_links(&self) -> Vec<Link> {
        self.virtual_links.clone()
    }
}

// LinkWidth: 0->virtual_link, 2->offset16, 3->offset24 and 4->offset32
#[derive(Default, Eq, PartialEq, Clone, Copy, Hash, Debug)]
#[repr(u8)]
pub(crate) enum LinkWidth {
    #[default]
    Zero = 0,
    Two = 2,
    Three = 3,
    Four = 4,
}

impl LinkWidth {
    pub(crate) fn new_checked(val: usize) -> Option<LinkWidth> {
        match val {
            0 => Some(LinkWidth::Zero),
            2 => Some(LinkWidth::Two),
            3 => Some(LinkWidth::Three),
            4 => Some(LinkWidth::Four),
            _ => None,
        }
    }
}

pub(crate) type ObjIdx = usize;

#[derive(Default, Eq, PartialEq, Clone, Copy, Hash, Debug)]
pub(crate) struct Link {
    width: LinkWidth,
    is_signed: bool,
    whence: OffsetWhence,
    bias: u32,
    position: u32,
    objidx: ObjIdx,
}

impl Link {
    pub(crate) fn obj_idx(&self) -> ObjIdx {
        self.objidx
    }

    pub(crate) fn link_width(&self) -> LinkWidth {
        self.width
    }

    pub(crate) fn whence(&self) -> OffsetWhence {
        self.whence
    }

    pub(crate) fn bias(&self) -> u32 {
        self.bias
    }

    pub(crate) fn is_signed(&self) -> bool {
        self.is_signed
    }

    #[allow(dead_code)]
    pub(crate) fn partial_equals(&self, other: &Self) -> bool {
        self.width == other.width
            && self.is_signed == other.is_signed
            && self.whence == other.whence
            && self.bias == other.bias
            && self.position == other.position
    }

    pub(crate) fn update_obj_idx(&mut self, new_idx: ObjIdx) {
        self.objidx = new_idx;
    }

    pub(crate) fn update_position(&mut self, new_pos: u32) {
        self.position = new_pos;
    }

    pub(crate) fn new(link_width: LinkWidth, obj_idx: ObjIdx, position: u32) -> Self {
        Self { width: link_width, objidx: obj_idx, position, ..Default::default() }
    }
}

#[derive(Default)]
pub(crate) struct Snapshot {
    head: usize,
    tail: usize,
    current: Option<PoolIdx>,
    num_real_links: usize,
    num_virtual_links: usize,
    errors: SerializeErrorFlags,
}

/// Constructs a sequential stream of bytes from one or more subsequences of
/// bytes/objects.
///
/// Notably this allows construction of open type tables that make use of
/// offsets (eg. GSUB, GPOS) to form graphs of sub tables. The serializer is
/// capable of automatically placing object data in a topological sorting
/// and resolving offsets given an object graph.
///
/// Port of the harfbuzz serializer:
/// <https://github.com/harfbuzz/harfbuzz/blob/5e32b5ca8fe430132b87c0eee6a1c056d37c35eb/src/hb-serialize.hh#L57>
///
/// For a higher level overview of serializer concepts see:
/// <https://github.com/harfbuzz/harfbuzz/blob/main/docs/serializer.md>
///
/// Note: currently repacking to resolve offset overflows is not yet implemented
/// (context: <https://github.com/harfbuzz/harfbuzz/blob/main/docs/repacker.md>)
#[derive(Default)]
pub struct Serializer {
    start: usize,
    end: usize,
    head: usize,
    tail: usize,
    // TODO: zerocopy, debug_depth
    errors: SerializeErrorFlags,

    data: Vec<u8>,
    object_pool: ObjectPool,
    // index for current Object in the object_pool
    current: Option<PoolIdx>,

    packed: Vec<PoolIdx>,
    packed_map: PoolIdxHashTable,
}

impl Serializer {
    pub fn new(size: usize) -> Self {
        Serializer {
            data: vec![0; size],
            end: size,
            tail: size,
            packed: Vec::new(),
            ..Default::default()
        }
    }

    /// Appends a the byte representation of a single scalar type onto the
    /// buffer.
    pub fn embed(&mut self, obj: impl Scalar) -> Result<usize, SerializeErrorFlags> {
        let raw = obj.to_raw();
        let bytes = raw.as_ref();
        let size = bytes.len();

        let ret = self.allocate_size(size, false)?;
        self.data[ret..ret + size].copy_from_slice(bytes);
        Ok(ret)
    }

    /// Appends a copy of a slice of bytes onto the buffer.
    pub fn embed_bytes(&mut self, bytes: &[u8]) -> Result<usize, SerializeErrorFlags> {
        let len = bytes.len();
        let ret = self.allocate_size(len, false)?;
        self.data[ret..ret + len].copy_from_slice(bytes);
        Ok(ret)
    }

    /// Adds count bytes of padding into the serialization buffer.
    pub fn pad(&mut self, count: usize) -> Result<usize, SerializeErrorFlags> {
        self.allocate_size(count, false)
    }

    /// get single Scalar value at certain position
    pub(crate) fn get_value_at<T: Scalar>(&self, pos: usize) -> Option<T> {
        let len = T::RAW_BYTE_LEN;
        let bytes = self.data.get(pos..pos + len)?;
        T::read(bytes)
    }

    pub(crate) fn check_assign<T: TryFrom<usize> + Scalar>(
        &mut self,
        pos: usize,
        obj: usize,
        err_type: SerializeErrorFlags,
    ) -> Result<(), SerializeErrorFlags> {
        let Ok(val) = T::try_from(obj) else {
            return Err(self.set_err(err_type));
        };
        self.copy_assign(pos, val);
        Ok(())
    }

    /// copy from a single Scalar type
    pub(crate) fn copy_assign(&mut self, pos: usize, obj: impl Scalar) {
        let raw = obj.to_raw();
        let bytes = raw.as_ref();
        let size = bytes.len();

        let Some(to) = self.data.get_mut(pos..pos + size) else {
            self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            return;
        };

        to.copy_from_slice(bytes);
    }

    /// copy from bytes
    pub(crate) fn copy_assign_from_bytes(&mut self, pos: usize, from_bytes: &[u8]) {
        let size = from_bytes.len();
        let Some(to) = self.data.get_mut(pos..pos + size) else {
            self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            return;
        };

        to.copy_from_slice(from_bytes);
    }

    pub(crate) fn get_mut_data(&mut self, range: Range<usize>) -> Option<&mut [u8]> {
        self.data.get_mut(range)
    }

    /// Allocate size
    pub(crate) fn allocate_size(
        &mut self,
        size: usize,
        clear: bool,
    ) -> Result<usize, SerializeErrorFlags> {
        if self.in_error() {
            return Err(self.errors);
        }

        if size > u32::MAX as usize || self.tail - self.head < size {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OUT_OF_ROOM));
        }

        if clear {
            self.data.get_mut(self.head..self.head + size).unwrap().fill(0);
        }
        let ret = self.head;
        self.head += size;
        Ok(ret)
    }

    pub(crate) fn successful(&self) -> bool {
        !self.errors
    }

    pub(crate) fn in_error(&self) -> bool {
        !!self.errors
    }

    pub(crate) fn ran_out_of_room(&self) -> bool {
        self.errors.contains(SerializeErrorFlags::SERIALIZE_ERROR_OUT_OF_ROOM)
    }

    pub(crate) fn offset_overflow(&self) -> bool {
        self.errors.contains(SerializeErrorFlags::SERIALIZE_ERROR_OFFSET_OVERFLOW)
    }

    pub(crate) fn only_offset_overflow(&self) -> bool {
        self.errors == SerializeErrorFlags::SERIALIZE_ERROR_OFFSET_OVERFLOW
    }

    pub(crate) fn only_overflow(&self) -> bool {
        self.errors == SerializeErrorFlags::SERIALIZE_ERROR_OFFSET_OVERFLOW
            || self.errors == SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW
            || self.errors == SerializeErrorFlags::SERIALIZE_ERROR_ARRAY_OVERFLOW
    }

    pub(crate) fn set_err(&mut self, error_type: SerializeErrorFlags) -> SerializeErrorFlags {
        self.errors |= error_type;
        self.errors
    }

    /// Returns any currently set error flags.
    pub fn error(&self) -> SerializeErrorFlags {
        self.errors
    }

    pub(crate) fn reset_size(&mut self, size: usize) {
        self.start = 0;
        self.end = size;
        self.reset();
        self.current = None;
        self.data.resize(size, 0);
    }

    fn reset(&mut self) {
        self.errors = SerializeErrorFlags::SERIALIZE_ERROR_NONE;
        self.head = self.start;
        self.tail = self.end;

        self.fini();
    }

    fn fini(&mut self) {
        for pool_idx in self.packed.iter() {
            self.object_pool.release(*pool_idx);
        }
        self.packed.clear();
        self.packed_map.clear();

        while let Some(current) = self.current.take() {
            self.current = self.object_pool.next_idx(current);
            self.object_pool.release(current);
        }
        self.data.clear();
    }

    pub(crate) fn snapshot(&self) -> Snapshot {
        let mut s = Snapshot {
            head: self.head,
            tail: self.tail,
            current: self.current,
            errors: self.errors,
            ..Default::default()
        };

        if self.current.is_none() {
            return s;
        }
        let Some(cur_obj) = self.object_pool.get_obj(self.current.unwrap()) else {
            return s;
        };

        s.num_real_links = cur_obj.real_links.len();
        s.num_virtual_links = cur_obj.virtual_links.len();
        s
    }

    /// Start a new sub table object which may be pointed to via offset from any
    /// object that has already been packed.
    pub fn push(&mut self) -> Result<(), SerializeErrorFlags> {
        if self.in_error() {
            return Err(self.errors);
        }

        let pool_idx = self.object_pool.alloc();
        let Some(obj) = self.object_pool.get_obj_mut(pool_idx) else {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };

        obj.head = self.head;
        obj.tail = self.tail;
        obj.next_obj = self.current;
        self.current = Some(pool_idx);

        Ok(())
    }

    /// Finalize the currently active object and copy it into the buffer.
    ///
    /// On success returns an index identifying the packed object.
    ///
    /// If share is true and there is an existing packed object which is
    /// identical this will not pack the object and instead return the index
    /// of the existing object.
    pub fn pop_pack(&mut self, share: bool) -> Option<ObjIdx> {
        self.current?;

        // Allow cleanup when we've error'd out on int overflows which don't compromise
        // the serializer state
        if self.in_error() && !self.only_overflow() {
            return None;
        }

        let pool_idx = self.current.unwrap();
        // code logic is a bit different from Harfbuzz serializer here
        // because I need to move code around to fix mutable borrow/immutable borrow
        // issue
        let obj = self.object_pool.get_obj_mut(pool_idx)?;

        self.current = obj.next_obj;
        obj.tail = self.head;
        obj.next_obj = None;

        if obj.tail < obj.head {
            self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            return None;
        }

        let len = obj.tail - obj.head;

        // TODO: consider zerocopy
        // Rewind head
        self.head = obj.head;

        if len == 0 {
            if !obj.real_links.is_empty() || !obj.virtual_links.is_empty() {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            }
            return None;
        }

        self.tail -= len;

        let obj_head = obj.head;
        obj.head = self.tail;
        obj.tail = self.tail + len;

        //TODO: consider zerocopy here
        self.data.copy_within(obj_head..obj_head + len, self.tail);

        let mut hash = 0_u64;
        // introduce flags to avoid mutable borrow and immutable borrow issues
        let mut obj_duplicate = None;
        if share {
            hash = hash_one_pool_idx(pool_idx, &self.data, &self.object_pool);
            if let Some(entry) =
                self.packed_map.get_with_hash(pool_idx, hash, &self.data, &self.object_pool)
            {
                obj_duplicate = Some(*entry);
            };
        }

        if let Some(dup_obj_idxes) = obj_duplicate {
            self.merge_virtual_links(pool_idx, dup_obj_idxes);
            self.object_pool.release(pool_idx);

            // rewind tail because we discarded duplicate obj
            self.tail += len;
            return Some(dup_obj_idxes.1);
        }

        self.packed.push(pool_idx);
        let obj_idx = self.packed.len() - 1;

        if share {
            self.packed_map.set_with_hash(pool_idx, hash, obj_idx, &self.data, &self.object_pool);
        }
        Some(obj_idx)
    }

    pub(crate) fn pop_discard(&mut self) {
        if self.current.is_none() {
            return;
        }
        // Allow cleanup when we've error'd out on int overflows which don't compromise
        // the serializer state.
        if self.in_error() && !self.only_overflow() {
            return;
        }

        let pool_idx = self.current.unwrap();
        let Some(obj) = self.object_pool.get_obj(pool_idx) else {
            return;
        };
        self.current = obj.next_obj;
        //TODO: consider zerocopy
        self.revert(obj.head, obj.tail);
        self.object_pool.release(pool_idx);
    }

    fn revert(&mut self, snap_head: usize, snap_tail: usize) {
        if self.in_error() || self.head < snap_head || self.tail > snap_tail {
            return;
        }
        self.head = snap_head;
        self.tail = snap_tail;
        self.discard_stale_objects();
    }

    pub(crate) fn revert_snapshot(&mut self, snapshot: Snapshot) {
        // Overflows that happened after the snapshot will be erased by the revert.
        if self.in_error() && !self.only_overflow() {
            return;
        }
        if snapshot.current != self.current {
            return;
        }
        self.errors = snapshot.errors;

        if let Some(current) = self.current {
            let Some(obj) = self.object_pool.get_obj_mut(current) else {
                return;
            };
            obj.real_links.truncate(snapshot.num_real_links);
            obj.virtual_links.truncate(snapshot.num_virtual_links);
        }
        self.revert(snapshot.head, snapshot.tail);
    }

    fn discard_stale_objects(&mut self) {
        if self.in_error() {
            return;
        }

        let len = self.packed.len();
        for i in (0..len).rev() {
            let pool_idx = self.packed[i];
            let Some(obj) = self.object_pool.get_obj(pool_idx) else {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                return;
            };

            if obj.next_obj.is_some() {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                return;
            }

            if obj.head >= self.tail {
                break;
            }

            let hash = hash_one_pool_idx(pool_idx, &self.data, &self.object_pool);
            self.packed_map.del(pool_idx, hash, &self.data, &self.object_pool);
            self.object_pool.release(pool_idx);
            self.packed.pop();
        }

        if let Some(pool_idx) = self.packed.last() {
            let Some(obj) = self.object_pool.get_obj(*pool_idx) else {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                return;
            };

            if obj.head != self.tail {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            }
        }
    }

    /// Creates an offset in the current object which will point to the object
    /// identified by obj_idx.
    ///
    /// When serialization finishes the appropriate offset value will be written
    /// into offset_byte_range, which is relative to the start of the
    /// current object.
    ///
    /// whence controls what the offset value is relative too.
    pub fn add_link(
        &mut self,
        offset_byte_range: Range<usize>,
        obj_idx: ObjIdx,
        whence: OffsetWhence,
        bias: u32,
        is_signed: bool,
    ) -> Result<(), SerializeErrorFlags> {
        if self.in_error() {
            return Err(self.errors);
        }

        let pool_idx =
            self.current.ok_or_else(|| self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?;
        let Some(current) = self.object_pool.get_obj_mut(pool_idx) else {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };

        let Some(link_width) = LinkWidth::new_checked(offset_byte_range.len()) else {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };

        if current.head > offset_byte_range.start {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        }

        let link = Link {
            width: link_width,
            is_signed,
            whence,
            bias,
            position: (offset_byte_range.start - current.head) as u32,
            objidx: obj_idx,
        };
        current.real_links.push(link);
        Ok(())
    }

    // Adds a virtual link from the current object to objidx. A virtual link is not
    // associated with an actual offset field. They are solely used to enforce
    // ordering constraints between objects. Adding a virtual link from object a
    // to object b will ensure that object b is always packed after object a in
    // the final serialized order. This is useful in certain situations where
    // there needs to be a specific ordering in the final serialization. Such as
    // when platform bugs require certain orderings, or to provide  guidance to
    // the repacker for better offset overflow resolution. ref: <https://github.com/harfbuzz/harfbuzz/blob/be79d5426553db9ce4074507c3a7c9afc175078d/src/hb-serialize.hh#L480>
    pub fn add_virtual_link(&mut self, obj_idx: ObjIdx) -> Result<(), SerializeErrorFlags> {
        let Some(cur_idx) = self.current else {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };
        let Some(current) = self.object_pool.get_obj_mut(cur_idx) else {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };

        current.add_virtual_link(obj_idx);
        Ok(())
    }

    pub fn last_added_child_index(&self) -> Option<ObjIdx> {
        let cur_pool_idx = self.current?;
        let cur_obj = self.object_pool.get_obj(cur_pool_idx)?;
        cur_obj.real_links.last().map(|l| l.obj_idx())
    }

    // For the current object ensure that the sub-table bytes for child objidx are
    // always placed after the subtable bytes for any other existing children.
    // This only ensures that the repacker will not move the target subtable
    // before the other children (by adding virtual links). It is up to the
    // caller to ensure the initial serialization order is correct.
    // ref: <https://github.com/harfbuzz/harfbuzz/blob/be79d5426553db9ce4074507c3a7c9afc175078d/src/hb-serialize.hh#L509>
    pub fn repack_last(&mut self, obj_idx: ObjIdx) -> Result<(), SerializeErrorFlags> {
        let cur_pool_idx =
            self.current.ok_or_else(|| self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?;

        let Some(cur_obj) = self.object_pool.get_obj(cur_pool_idx) else {
            return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };

        let other_child_idxes: Vec<ObjIdx> =
            cur_obj.real_links.iter().map(|l| l.obj_idx()).filter(|&idx| idx != obj_idx).collect();

        for other_idx in other_child_idxes {
            let Some(pool_idx) = self.packed.get(other_idx) else {
                return Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
            };
            let Some(other_obj) = self.object_pool.get_obj_mut(*pool_idx) else {
                continue;
            };
            other_obj.add_virtual_link(obj_idx);
        }
        Ok(())
    }

    fn merge_virtual_links(&mut self, from: PoolIdx, to: (PoolIdx, ObjIdx)) {
        let from_obj = self.object_pool.get_obj_mut(from).unwrap();
        if from_obj.virtual_links.is_empty() {
            return;
        }
        let mut from_vec = mem::take(&mut from_obj.virtual_links);

        // hash value will change after merge, so delete existing old entry in the hash
        // table
        let hash_old = hash_one_pool_idx(to.0, &self.data, &self.object_pool);
        self.packed_map.del(to.0, hash_old, &self.data, &self.object_pool);

        let to_obj = self.object_pool.get_obj_mut(to.0).unwrap();
        to_obj.virtual_links.append(&mut from_vec);

        let hash = hash_one_pool_idx(to.0, &self.data, &self.object_pool);
        self.packed_map.set_with_hash(to.0, hash, to.1, &self.data, &self.object_pool);
    }

    fn resolve_links(&mut self) {
        if self.in_error() {
            return;
        }

        if self.current.is_some() || self.packed.is_empty() {
            self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            return;
        }

        let mut offset_links = Vec::new();
        for parent_obj_idx in self.packed.iter() {
            let Some(parent_obj) = self.object_pool.get_obj(*parent_obj_idx) else {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                return;
            };

            for link in parent_obj.real_links.iter() {
                let Some(child_pool_idx) = self.packed.get(link.objidx) else {
                    self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                    return;
                };
                let Some(child_obj) = self.object_pool.get_obj(*child_pool_idx) else {
                    self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                    return;
                };

                let offset = match link.whence {
                    OffsetWhence::Head => child_obj.head - parent_obj.head,
                    OffsetWhence::Tail => child_obj.head - parent_obj.tail,
                    OffsetWhence::Absolute => self.head - self.start + child_obj.head - self.tail,
                };

                if offset < link.bias as usize {
                    self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                    return;
                }

                let offset = offset - link.bias as usize;
                let offset_start_pos = parent_obj.head + link.position as usize;
                offset_links.push((offset_start_pos, link.width, offset));
            }
        }

        for (offset_start_pos, offset_width, offset) in offset_links.iter() {
            self.assign_offset(*offset_start_pos, *offset_width, *offset);
        }
    }

    //TODO: take care of signed offset
    fn assign_offset(&mut self, offset_start_pos: usize, link_width: LinkWidth, offset: usize) {
        let (offset_width, max_offset) = match link_width {
            LinkWidth::Two => (2, u16::MAX as usize),
            LinkWidth::Three => (3, (Uint24::MAX).to_u32() as usize),
            LinkWidth::Four => (4, u32::MAX as usize),
            _ => return,
        };

        if offset > max_offset {
            self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OFFSET_OVERFLOW);
            return;
        }

        let Some(offset_data_bytes) =
            self.data.get_mut(offset_start_pos..offset_start_pos + offset_width)
        else {
            self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            return;
        };

        let be_bytes = (offset as u32).to_be_bytes();
        match link_width {
            LinkWidth::Two => offset_data_bytes.copy_from_slice(&be_bytes[2..=3]),
            LinkWidth::Three => offset_data_bytes.copy_from_slice(&be_bytes[1..=3]),
            LinkWidth::Four => offset_data_bytes.copy_from_slice(&be_bytes),
            _ => (),
        }
    }

    /// Create and return a copy of the serialization buffer.
    ///
    /// end_serialize() should be called prior to this to perform offset
    /// resolution.
    pub fn copy_bytes(mut self) -> Vec<u8> {
        if !self.successful() {
            return Vec::new();
        }
        let len = (self.head - self.start) + (self.end - self.tail);
        if len == 0 {
            return Vec::new();
        }

        self.data.copy_within(self.tail..self.end, self.head);
        self.data.truncate(len);
        self.data
    }

    pub(crate) fn length(&self) -> usize {
        let Some(current) = self.current else {
            return 0;
        };
        let Some(cur_obj) = self.object_pool.get_obj(current) else {
            return 0;
        };
        self.head - cur_obj.head
    }

    pub(crate) fn head(&self) -> usize {
        self.head
    }

    pub(crate) fn tail(&self) -> usize {
        self.tail
    }

    pub(crate) fn allocated(&self) -> usize {
        self.data.len()
    }

    /// Starts the serialization of an object graph.
    pub fn start_serialize(&mut self) -> Result<(), SerializeErrorFlags> {
        if self.current.is_some() {
            Err(self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?
        }

        self.push()
    }

    /// Ends the serialization of an object graph and resolves all offsets.
    pub fn end_serialize(&mut self) {
        if self.current.is_none() {
            return;
        }

        // Offset overflows that occur before link resolution cannot be handled by
        // repacking, so set a more general error.
        if self.in_error() {
            if self.offset_overflow() {
                self.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            }
            return;
        }

        if self.packed.is_empty() {
            return;
        }

        self.pop_pack(false);
        self.resolve_links();
    }

    pub(crate) fn packed_idxs(&self) -> &[PoolIdx] {
        &self.packed
    }

    // used by graph only
    pub(crate) fn get_obj(&self, pool_idx: PoolIdx) -> Option<&Object> {
        self.object_pool.get_obj(pool_idx)
    }

    pub(crate) fn data(&self) -> Vec<u8> {
        self.data.clone()
    }
}

#[derive(Default)]
struct ObjectWrap {
    pub obj: Object,
    pub next: Option<PoolIdx>,
}

// reference Harfbuzz implementation:
//<https://github.com/harfbuzz/harfbuzz/blob/5e32b5ca8fe430132b87c0eee6a1c056d37c35eb/src/hb-pool.hh>
#[derive(Default)]
struct ObjectPool {
    chunks: Vec<ObjectWrap>,
    next: Option<PoolIdx>,
}

impl ObjectPool {
    const ALLOC_CHUNKS_LEN: usize = 64;
    fn alloc(&mut self) -> PoolIdx {
        let len = self.chunks.len();
        if self.next.is_none() {
            let new_len = len + Self::ALLOC_CHUNKS_LEN;
            self.chunks.resize_with(new_len, Default::default);

            for (idx, obj) in self.chunks.get_mut(len..new_len - 1).unwrap().iter_mut().enumerate()
            {
                obj.next = Some(len + idx + 1);
            }

            self.chunks.last_mut().unwrap().next = None;
            self.next = Some(len);
        }

        let pool_idx = self.next.unwrap();
        self.next = self.chunks[pool_idx].next;

        pool_idx
    }

    fn release(&mut self, pool_idx: PoolIdx) {
        let Some(obj_wrap) = self.chunks.get_mut(pool_idx) else {
            return;
        };

        obj_wrap.obj.reset();
        obj_wrap.next = self.next;
        self.next = Some(pool_idx);
    }

    fn get_obj_mut(&mut self, pool_idx: PoolIdx) -> Option<&mut Object> {
        self.chunks.get_mut(pool_idx).map(|o| &mut o.obj)
    }

    fn get_obj(&self, pool_idx: PoolIdx) -> Option<&Object> {
        self.chunks.get(pool_idx).map(|o| &o.obj)
    }

    fn next_idx(&self, pool_idx: PoolIdx) -> Option<PoolIdx> {
        self.get_obj(pool_idx)?.next_obj
    }
}

// Hash an Object: Virtual links aren't considered for equality since they don't
// affect the functionality of the object.
fn hash_one_pool_idx(pool_idx: PoolIdx, data: &[u8], obj_pool: &ObjectPool) -> u64 {
    let mut hasher = FnvHasher::default();
    let Some(obj) = obj_pool.get_obj(pool_idx) else {
        return hasher.finish();
    };
    // hash data bytes
    // Only hash at most 128 bytes for Object. Byte objects differ in their early
    // bytes anyway. reference: <https://github.com/harfbuzz/harfbuzz/blob/622e9c33c39e9c2a6491763841d6d6ad715f6abf/src/hb-serialize.hh#L136>
    let byte_len = 128.min(obj.tail - obj.head);
    let data_bytes = data.get(obj.head..obj.head + byte_len);
    data_bytes.hash(&mut hasher);
    // hash real_links
    obj.real_links.hash(&mut hasher);

    hasher.finish()
}

// Virtual links aren't considered for equality since they don't affect the
// functionality of the object.
fn cmp_pool_idx(idx_a: PoolIdx, idx_b: PoolIdx, data: &[u8], obj_pool: &ObjectPool) -> bool {
    if idx_a == idx_b {
        return true;
    }

    match (obj_pool.get_obj(idx_a), obj_pool.get_obj(idx_b)) {
        (Some(_), None) => false,
        (None, Some(_)) => false,
        (None, None) => true,
        (Some(obj_a), Some(obj_b)) => {
            data.get(obj_a.head..obj_a.tail) == data.get(obj_b.head..obj_b.tail)
                && obj_a.real_links == obj_b.real_links
        }
    }
}

#[derive(Default)]
struct PoolIdxHashTable {
    hash_table: HashTable<(PoolIdx, ObjIdx)>,
}

impl PoolIdxHashTable {
    fn get_with_hash(
        &self,
        pool_idx: PoolIdx,
        hash: u64,
        data: &[u8],
        obj_pool: &ObjectPool,
    ) -> Option<&(PoolIdx, ObjIdx)> {
        self.hash_table
            .find(hash, |val: &(PoolIdx, ObjIdx)| cmp_pool_idx(pool_idx, val.0, data, obj_pool))
    }

    fn set_with_hash(
        &mut self,
        pool_idx: PoolIdx,
        hash: u64,
        obj_idx: ObjIdx,
        data: &[u8],
        obj_pool: &ObjectPool,
    ) {
        let hasher = |val: &(PoolIdx, ObjIdx)| hash_one_pool_idx(val.0, data, obj_pool);

        self.hash_table.insert_unique(hash, (pool_idx, obj_idx), hasher);
    }

    fn del(&mut self, pool_idx: PoolIdx, hash: u64, data: &[u8], obj_pool: &ObjectPool) {
        let Ok(entry) = self.hash_table.find_entry(hash, |val: &(PoolIdx, ObjIdx)| {
            cmp_pool_idx(pool_idx, val.0, data, obj_pool)
        }) else {
            return;
        };
        entry.remove();
    }

    fn clear(&mut self) {
        self.hash_table.clear();
    }
}

#[cfg(test)]
mod test {
    use write_fonts::types::{Offset16, Offset32, UfWord, Uint24};

    use super::*;

    // test Serializer::embed() works for different Scalar types
    #[test]
    fn test_serializer_embed() {
        let mut s = Serializer::new(2);
        let gid = 1_u32;
        //fail when out of room
        assert_eq!(s.embed(gid), Err(SerializeErrorFlags::SERIALIZE_ERROR_OUT_OF_ROOM));

        let mut s = Serializer::new(16384);
        assert_eq!(s.embed(gid), Ok(0));

        //check that head is advancing accordingly
        let offset = Offset16::new(20);
        assert_eq!(s.embed(offset), Ok(4));

        let n = Uint24::new(30);
        assert_eq!(s.embed(n), Ok(6));

        let w = UfWord::new(40);
        assert_eq!(s.embed(w), Ok(9));
        assert_eq!(s.head, 11);
        assert_eq!(s.start, 0);
        assert_eq!(s.tail, 16384);
        assert_eq!(s.end, 16384);

        let out = s.copy_bytes();
        assert_eq!(out, [0, 0, 0, 1, 0, 20, 0, 0, 30, 0, 40]);
    }

    #[test]
    fn test_serializer_embed_bytes() {
        let mut s = Serializer::new(2);
        let bytes = vec![1_u8, 2, 3, 4, 5];
        //fail when out of room
        assert_eq!(s.embed_bytes(&bytes), Err(SerializeErrorFlags::SERIALIZE_ERROR_OUT_OF_ROOM));

        let mut s = Serializer::new(10);
        assert_eq!(s.embed_bytes(&bytes), Ok(0));

        //check that head is advancing accordingly
        assert_eq!(s.head, 5);
        assert_eq!(s.start, 0);
        assert_eq!(s.tail, 10);
        assert_eq!(s.end, 10);

        let out = s.copy_bytes();
        assert_eq!(out, [1, 2, 3, 4, 5]);
    }

    #[test]
    fn test_push() {
        let mut s = Serializer::new(2);
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.embed(1_u32), Err(SerializeErrorFlags::SERIALIZE_ERROR_OUT_OF_ROOM));
        assert_eq!(s.push(), Err(SerializeErrorFlags::SERIALIZE_ERROR_OUT_OF_ROOM));
    }

    #[test]
    fn test_object_pool() {
        let mut p = ObjectPool::default();

        let i = p.alloc();
        assert_eq!(i, 0);
        let i = p.alloc();
        assert_eq!(i, 1);
        let i = p.alloc();
        assert_eq!(i, 2);
        let i = p.alloc();
        assert_eq!(i, 3);

        p.release(1);
        let i = p.alloc();
        assert_eq!(i, 1);

        p.release(3);
        let i = p.alloc();
        assert_eq!(i, 3);

        let i = p.alloc();
        assert_eq!(i, 4);

        let o = p.get_obj(0).unwrap();
        assert_eq!(o.head, 0);
        assert_eq!(o.tail, 0);
        assert!(o.real_links.is_empty());
        assert!(o.virtual_links.is_empty());
        assert_eq!(o.next_obj, None);

        //test alloc more than 64 chunks, which would grow the internal vector
        for i in 0..64 {
            let idx = p.alloc();
            assert_eq!(idx, 5 + i);
        }
    }

    #[test]
    fn test_hash_table() {
        let mut obj_pool = ObjectPool::default();
        let data: Vec<u8> =
            vec![0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 0, 0, 0, 0, 0];
        let obj_0 = obj_pool.alloc();
        assert_eq!(obj_0, 0);
        let obj_1 = obj_pool.alloc();
        assert_eq!(obj_1, 1);
        let obj_2 = obj_pool.alloc();
        assert_eq!(obj_2, 2);
        let obj_3 = obj_pool.alloc();
        assert_eq!(obj_3, 3);

        //obj_0, obj_1 and obj_3 point to the same bytes
        {
            let obj_0 = obj_pool.get_obj_mut(0).unwrap();
            obj_0.head = 0;
            obj_0.tail = 5;
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 21,
                objidx: 2,
            };
            obj_0.real_links.push(link);
        }

        {
            // obj_1 has the same real_links with obj_0
            let obj_1 = obj_pool.get_obj_mut(1).unwrap();
            obj_1.head = 5;
            obj_1.tail = 10;
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 21,
                objidx: 2,
            };
            obj_1.real_links.push(link);
        }

        {
            let obj_2 = obj_pool.get_obj_mut(2).unwrap();
            obj_2.head = 10;
            obj_2.tail = 15;
        }

        {
            // obj_3 doesn't have real_links
            let obj_3 = obj_pool.get_obj_mut(3).unwrap();
            obj_3.head = 15;
            obj_3.tail = 20;
        }

        assert!(cmp_pool_idx(0, 1, &data, &obj_pool));
        assert!(!cmp_pool_idx(0, 2, &data, &obj_pool));
        assert!(!cmp_pool_idx(0, 3, &data, &obj_pool));
        assert!(!cmp_pool_idx(1, 3, &data, &obj_pool));

        let mut hash_table = PoolIdxHashTable::default();
        let hash_0 = hash_one_pool_idx(0, &data, &obj_pool);
        let hash_1 = hash_one_pool_idx(1, &data, &obj_pool);
        assert_eq!(hash_0, hash_1);
        let hash_2 = hash_one_pool_idx(2, &data, &obj_pool);
        let hash_3 = hash_one_pool_idx(3, &data, &obj_pool);
        assert_ne!(hash_0, hash_2);
        assert_ne!(hash_0, hash_3);
        assert_ne!(hash_2, hash_3);

        hash_table.set_with_hash(0, hash_0, 0, &data, &obj_pool);
        assert_eq!(hash_table.get_with_hash(1, hash_1, &data, &obj_pool), Some(&(0, 0)));
        assert_eq!(hash_table.get_with_hash(2, hash_2, &data, &obj_pool), None);
        assert_eq!(hash_table.get_with_hash(3, hash_3, &data, &obj_pool), None);

        hash_table.set_with_hash(2, hash_2, 2, &data, &obj_pool);
        assert_eq!(hash_table.get_with_hash(2, hash_2, &data, &obj_pool), Some(&(2, 2)));

        hash_table.set_with_hash(3, hash_3, 3, &data, &obj_pool);
        assert_eq!(hash_table.get_with_hash(3, hash_3, &data, &obj_pool), Some(&(3, 3)));

        // update obj_3 to have the same real links as obj_0
        {
            let obj_3 = obj_pool.get_obj_mut(3).unwrap();
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 21,
                objidx: 2,
            };
            obj_3.real_links.push(link);
        }

        let hash_3_new = hash_one_pool_idx(3, &data, &obj_pool);
        assert_ne!(hash_3, hash_3_new);
        // old entries found with old hash
        assert_eq!(hash_table.get_with_hash(3, hash_3, &data, &obj_pool), Some(&(3, 3)));

        // test del
        hash_table.del(3, hash_3, &data, &obj_pool);
        assert_eq!(hash_table.get_with_hash(3, hash_3, &data, &obj_pool), None);

        // duplicate obj_0 entry found with new hash
        assert_eq!(hash_table.get_with_hash(3, hash_3_new, &data, &obj_pool), Some(&(0, 0)));
    }

    #[test]
    fn test_push_and_pop_pack() {
        let mut s = Serializer::new(100);
        let n = Uint24::new(80);
        assert_eq!(s.embed(n), Ok(0));
        assert_eq!(s.head, 3);
        assert_eq!(s.current, None);

        {
            //single pop_pack()
            assert_eq!(s.push(), Ok(()));
            //an object is allocated during push
            assert_eq!(s.current, Some(0));
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);
            assert_eq!(s.tail, 100);
            assert_eq!(s.pop_pack(true), Some(0));
            assert_eq!(s.packed_map.hash_table.len(), 1);
            assert_eq!(s.packed.len(), 1);
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 97);
        }

        {
            //test de-duplicate
            assert_eq!(s.push(), Ok(()));
            assert_eq!(s.current, Some(1));
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);
            assert_eq!(s.pop_pack(true), Some(0));
            assert_eq!(s.packed_map.hash_table.len(), 1);
            // share=true, duplicate object won't be added into packed vector
            assert_eq!(s.packed.len(), 1);
            //check to make sure head is rewinded
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 97);

            assert_eq!(s.push(), Ok(()));
            // check that we released the previous duplicate object
            assert_eq!(s.current, Some(1));
            let n = UfWord::new(10);
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 5);
            assert_eq!(s.pop_pack(true), Some(1));
            assert_eq!(s.packed_map.hash_table.len(), 2);
            assert_eq!(s.packed.len(), 2);
            //check to make sure head is rewinded
            assert_eq!(s.head, 3);
            //check that tail is updated
            assert_eq!(s.tail, 95);
        }

        {
            //test pop_pack(false) when duplicate objects exist
            assert_eq!(s.push(), Ok(()));
            assert_eq!(s.current, Some(2));
            let n = Uint24::new(80);
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);
            assert_eq!(s.pop_pack(false), Some(2));
            // when share=false, we don't set this object in hash table, so the len of hash
            // table is the same
            assert_eq!(s.packed_map.hash_table.len(), 2);
            // share=true, duplicate object will be added into the packed vector
            assert_eq!(s.packed.len(), 3);
            //check to make sure head is rewinded
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 92);
        }

        {
            // test de-duplicate with virtual links
            // virtual links don't affect equality and merge virtual links works
            assert_eq!(s.push(), Ok(()));
            assert_eq!(s.current, Some(3));
            let n = Uint24::new(123);
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);

            //add virtual links
            let obj = s.object_pool.get_obj_mut(3).unwrap();
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 0,
                objidx: 0,
            };
            obj.virtual_links.push(link);
            assert_eq!(s.pop_pack(true), Some(3));
            assert_eq!(s.packed_map.hash_table.len(), 3);
            assert_eq!(s.packed.len(), 4);
            //check to make sure head is rewinded
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 89);

            // another obj which differs only in virtual links
            assert_eq!(s.push(), Ok(()));
            assert_eq!(s.current, Some(4));
            let n = Uint24::new(123);
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);

            //add virtual links
            let obj = s.object_pool.get_obj_mut(4).unwrap();
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 0,
                objidx: 1,
            };
            obj.virtual_links.push(link);
            // check that virtual links doesn't affect euqality
            assert_eq!(s.pop_pack(true), Some(3));
            assert_eq!(s.packed_map.hash_table.len(), 3);
            assert_eq!(s.packed.len(), 4);
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 89);
            // check that duplicate obj is released, virtual links are emptied
            assert!(s.object_pool.get_obj(4).unwrap().virtual_links.is_empty());
            // check that merge_virtual_links works
            let obj = s.object_pool.get_obj_mut(3).unwrap();
            assert_eq!(obj.virtual_links.len(), 2);
            assert_eq!(obj.virtual_links[0].objidx, 0);
            assert_eq!(obj.virtual_links[1].objidx, 1);
        }

        {
            // test de-duplicate with real links
            // real links should be included in hash and equality computation
            assert_eq!(s.push(), Ok(()));
            assert_eq!(s.current, Some(4));
            let n = Uint24::new(321);
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);

            //add real links
            let obj = s.object_pool.get_obj_mut(4).unwrap();
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 10,
                objidx: 2,
            };
            obj.real_links.push(link);
            assert_eq!(s.pop_pack(true), Some(4));
            assert_eq!(s.packed_map.hash_table.len(), 4);
            assert_eq!(s.packed.len(), 5);
            //check to make sure head is rewinded
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 86);

            // another obj which differs only in real links
            assert_eq!(s.push(), Ok(()));
            assert_eq!(s.current, Some(5));
            let n = Uint24::new(321);
            assert_eq!(s.embed(n), Ok(3));
            assert_eq!(s.head, 6);

            //add real links
            let obj = s.object_pool.get_obj_mut(5).unwrap();
            let link = Link {
                width: LinkWidth::Two,
                is_signed: false,
                whence: OffsetWhence::Head,
                bias: 0,
                position: 20,
                objidx: 3,
            };
            obj.real_links.push(link);
            // pop_pack with a different obj_idx
            assert_eq!(s.pop_pack(true), Some(5));
            // obj is added into both hash_table and packed vector
            assert_eq!(s.packed_map.hash_table.len(), 5);
            assert_eq!(s.packed.len(), 6);
            //check to make sure head is rewinded
            assert_eq!(s.head, 3);
            assert_eq!(s.tail, 83);
        }
    }

    #[test]
    fn test_push_and_pop_discard() {
        let mut s = Serializer::new(100);
        let n = Uint24::new(80);
        assert_eq!(s.embed(n), Ok(0));
        assert_eq!(s.head, 3);
        assert_eq!(s.current, None);

        //single pop_pack()
        assert_eq!(s.push(), Ok(()));
        //an object is allocated during push
        assert_eq!(s.current, Some(0));
        assert_eq!(s.embed(n), Ok(3));
        assert_eq!(s.head, 6);
        assert_eq!(s.tail, 100);
        assert_eq!(s.pop_pack(true), Some(0));
        assert_eq!(s.packed.len(), 1);
        assert_eq!(s.head, 3);
        assert_eq!(s.tail, 97);

        //pop_discard()
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(1));
        assert_eq!(s.embed(n), Ok(3));
        assert_eq!(s.head, 6);
        assert_eq!(s.tail, 97);
        s.pop_discard();
        //check that head is rewinded
        assert_eq!(s.head, 3);
        assert_eq!(s.tail, 97);
        //discarded obj is not added into packed vector
        assert_eq!(s.packed.len(), 1);
        //check that discard object is reused by new push()
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(1));
    }

    #[test]
    fn test_add_link_resolve_links() {
        let mut s = Serializer::new(100);
        let header: u32 = 1;
        //parent header
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.embed(header), Ok(0));
        //first offset field
        let offset_1 = Offset16::new(0);
        assert_eq!(s.embed(offset_1), Ok(4));
        //second offset field
        let offset_2 = Offset32::new(0);
        assert_eq!(s.embed(offset_2), Ok(6));
        assert_eq!(s.head, 10);
        assert_eq!(s.current, Some(0));

        //pack first child
        let n = Uint24::new(123);
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(1));
        assert_eq!(s.embed(n), Ok(10));
        assert_eq!(s.head, 13);
        assert_eq!(s.tail, 100);
        //pop_pack this child
        assert_eq!(s.pop_pack(true), Some(0));
        assert_eq!(s.packed.len(), 1);
        assert_eq!(s.head, 10);
        assert_eq!(s.tail, 97);
        //add_link to offset_1
        assert_eq!(s.add_link(4..6, 0, OffsetWhence::Head, 0, false), Ok(()));

        //pack another child
        let n = Uint24::new(234);
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(2));
        assert_eq!(s.embed(n), Ok(10));
        assert_eq!(s.head, 13);
        assert_eq!(s.tail, 97);
        //pop_pack this child
        assert_eq!(s.pop_pack(true), Some(1));
        assert_eq!(s.packed.len(), 2);
        assert_eq!(s.head, 10);
        assert_eq!(s.tail, 94);
        //add_link to offset_1
        assert_eq!(s.add_link(6..10, 1, OffsetWhence::Head, 0, false), Ok(()));

        //pop_pack parent header
        assert_eq!(s.pop_pack(false), Some(2));
        assert_eq!(s.head, 0);
        assert_eq!(s.tail, 84);
        assert_eq!(s.packed.len(), 3);
        s.resolve_links();
        assert_eq!(
            s.data.get(84..100).unwrap(),
            [0, 0, 0, 1, 0, 13, 0, 0, 0, 10, 0, 0, 234, 0, 0, 123]
        );
    }

    #[test]
    fn test_length() {
        let mut s = Serializer::new(100);
        let n = Uint24::new(80);
        assert_eq!(s.embed(n), Ok(0));
        assert_eq!(s.head, 3);
        assert_eq!(s.tail, 100);
        assert_eq!(s.current, None);

        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(0));
        assert_eq!(s.head, 3);
        //embed 2 Uint24 numbers
        assert_eq!(s.embed(n), Ok(3));
        assert_eq!(s.embed(n), Ok(6));
        // check length() = 6
        assert_eq!(s.length(), 6);
    }

    #[test]
    fn test_snapshot_and_revert() {
        let mut s = Serializer::new(100);
        //test when current is None
        let snapshot = s.snapshot();
        let n = Uint24::new(80);
        assert_eq!(s.embed(n), Ok(0));
        assert_eq!(s.head, 3);
        assert_eq!(s.tail, 100);
        assert_eq!(s.current, None);

        s.revert_snapshot(snapshot);
        assert!(!s.in_error());
        assert_eq!(s.head, 0);
        assert_eq!(s.tail, 100);
        assert_eq!(s.current, None);

        //snapshot and revert after single push(), current is not None
        assert_eq!(s.push(), Ok(()));
        let snapshot = s.snapshot();
        assert_eq!(s.current, Some(0));
        assert_eq!(s.embed(n), Ok(0));
        assert_eq!(s.head, 3);
        assert_eq!(s.tail, 100);

        s.revert_snapshot(snapshot);
        assert!(!s.in_error());
        assert_eq!(s.head, 0);
        assert_eq!(s.tail, 100);

        //test after a couple of push/pop_pack
        let snapshot = s.snapshot();
        assert_eq!(s.current, Some(0));
        assert_eq!(s.head, 0);
        assert_eq!(s.tail, 100);

        assert_eq!(s.embed(n), Ok(0));
        //another push, now current = 1
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(1));

        assert_eq!(s.embed(n), Ok(3));
        assert_eq!(s.pop_pack(true), Some(0));
        assert_eq!(s.add_link(0..3, 0, OffsetWhence::Head, 0, false), Ok(()));
        //check that after pop_pack, now current is back to 0, and real_links length
        // =1;
        assert_eq!(s.current, Some(0));
        assert_eq!(s.object_pool.get_obj(0).unwrap().real_links.len(), 1);

        s.revert_snapshot(snapshot);
        assert!(!s.in_error());
        assert_eq!(s.head, 0);
        assert_eq!(s.tail, 100);
        assert_eq!(s.current, Some(0));
        // check that real_links is truncated
        assert_eq!(s.object_pool.get_obj(0).unwrap().real_links.len(), 0);
        // another push to check that obj_idx=1 is released in previous revert_snapshot
        assert_eq!(s.push(), Ok(()));
        assert_eq!(s.current, Some(1));
    }
}
