//! The `CmpObserver` provides access to the logged values of CMP instructions
use alloc::{borrow::Cow, vec::Vec};
use core::{
    fmt::Debug,
    ops::{Deref, DerefMut},
};

use arbitrary_int::{u1, u4, u5, u6};
use bitbybit::bitfield;
use hashbrown::HashMap;
use libafl_bolts::{AsSlice, HasLen, Named, ownedref::OwnedRefMut};
use serde::{Deserialize, Serialize};

use crate::{Error, HasMetadata, executors::ExitKind, observers::Observer};

/// A bytes string for cmplog with up to 32 elements.
#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
pub struct CmplogBytes {
    buf: [u8; 32],
    len: u8,
}

impl CmplogBytes {
    /// Creates a new [`CmplogBytes`] object from the provided buf and length.
    /// Lengths above 32 are illegal but will be ignored.
    #[must_use]
    pub fn from_buf_and_len(buf: [u8; 32], len: u8) -> Self {
        debug_assert!(len <= 32, "Len too big: {len}, max: 32");
        CmplogBytes { buf, len }
    }
}

impl<'a> AsSlice<'a> for CmplogBytes {
    type Entry = u8;

    type SliceRef = &'a [u8];

    fn as_slice(&'a self) -> Self::SliceRef {
        &self.buf[0..(self.len as usize)]
    }
}

impl HasLen for CmplogBytes {
    fn len(&self) -> usize {
        self.len as usize
    }
}

/// Compare values collected during a run
#[derive(Eq, PartialEq, Debug, Serialize, Deserialize, Clone)]
pub enum CmpValues {
    /// (side 1 of comparison, side 2 of comparison, side 1 value is const)
    U8((u8, u8, bool)),
    /// (side 1 of comparison, side 2 of comparison, side 1 value is const)
    U16((u16, u16, bool)),
    /// (side 1 of comparison, side 2 of comparison, side 1 value is const)
    U32((u32, u32, bool)),
    /// (side 1 of comparison, side 2 of comparison, side 1 value is const)
    U64((u64, u64, bool)),
    /// Two vecs of u8 values/byte
    Bytes((CmplogBytes, CmplogBytes)),
}

impl CmpValues {
    /// Returns if the values are numericals
    #[must_use]
    pub fn is_numeric(&self) -> bool {
        matches!(
            self,
            CmpValues::U8(_) | CmpValues::U16(_) | CmpValues::U32(_) | CmpValues::U64(_)
        )
    }

    /// Converts the value to a u64 tuple
    #[must_use]
    pub fn to_u64_tuple(&self) -> Option<(u64, u64, bool)> {
        match self {
            CmpValues::U8(t) => Some((u64::from(t.0), u64::from(t.1), t.2)),
            CmpValues::U16(t) => Some((u64::from(t.0), u64::from(t.1), t.2)),
            CmpValues::U32(t) => Some((u64::from(t.0), u64::from(t.1), t.2)),
            CmpValues::U64(t) => Some(*t),
            CmpValues::Bytes(_) => None,
        }
    }
}

/// A state metadata holding a list of values logged from comparisons
#[derive(Debug, Default, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct CmpValuesMetadata {
    /// A `list` of values.
    #[serde(skip)]
    pub list: Vec<CmpValues>,
}

libafl_bolts::impl_serdeany!(CmpValuesMetadata);

impl Deref for CmpValuesMetadata {
    type Target = [CmpValues];
    fn deref(&self) -> &[CmpValues] {
        &self.list
    }
}

impl DerefMut for CmpValuesMetadata {
    fn deref_mut(&mut self) -> &mut [CmpValues] {
        &mut self.list
    }
}

impl CmpValuesMetadata {
    /// Creates a new [`struct@CmpValuesMetadata`]
    #[must_use]
    pub fn new() -> Self {
        Self { list: vec![] }
    }

    /// Add comparisons to a metadata from a `CmpObserver`. `cmp_map` is mutable in case
    /// it is needed for a custom map, but this is not utilized for `CmpObserver` or
    /// `AflppCmpLogObserver`.
    pub fn add_from<CM>(&mut self, usable_count: usize, cmp_map: &mut CM)
    where
        CM: CmpMap,
    {
        self.list.clear();
        let count = usable_count;
        for i in 0..count {
            let execs = cmp_map.usable_executions_for(i);
            if execs > 0 {
                // Recongize loops and discard if needed
                if execs > 4 {
                    let mut increasing_v0 = 0;
                    let mut increasing_v1 = 0;
                    let mut decreasing_v0 = 0;
                    let mut decreasing_v1 = 0;

                    let mut last: Option<CmpValues> = None;
                    for j in 0..execs {
                        if let Some(val) = cmp_map.values_of(i, j) {
                            if let Some(l) = last.and_then(|x| x.to_u64_tuple()) {
                                if let Some(v) = val.to_u64_tuple() {
                                    if l.0.wrapping_add(1) == v.0 {
                                        increasing_v0 += 1;
                                    }
                                    if l.1.wrapping_add(1) == v.1 {
                                        increasing_v1 += 1;
                                    }
                                    if l.0.wrapping_sub(1) == v.0 {
                                        decreasing_v0 += 1;
                                    }
                                    if l.1.wrapping_sub(1) == v.1 {
                                        decreasing_v1 += 1;
                                    }
                                }
                            }
                            last = Some(val);
                        }
                    }
                    // We check for execs-2 because the logged execs may wrap and have something like
                    // 8 9 10 3 4 5 6 7
                    if increasing_v0 >= execs - 2
                        || increasing_v1 >= execs - 2
                        || decreasing_v0 >= execs - 2
                        || decreasing_v1 >= execs - 2
                    {
                        continue;
                    }
                }
                for j in 0..execs {
                    if let Some(val) = cmp_map.values_of(i, j) {
                        self.list.push(val);
                    }
                }
            }
        }
    }
}

/// A [`CmpMap`] traces comparisons during the current execution
pub trait CmpMap: Debug {
    /// Get the number of cmps
    fn len(&self) -> usize;

    /// Get if it is empty
    #[must_use]
    fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get the number of executions for a cmp
    fn executions_for(&self, idx: usize) -> usize;

    /// Get the number of logged executions for a cmp
    fn usable_executions_for(&self, idx: usize) -> usize;

    /// Get the logged values for a cmp
    fn values_of(&self, idx: usize, execution: usize) -> Option<CmpValues>;

    /// Reset the state
    fn reset(&mut self) -> Result<(), Error>;
}

/// A [`CmpObserver`] observes the traced comparisons during the current execution using a [`CmpMap`]
pub trait CmpObserver {
    /// The underlying map
    type Map;
    /// Get the number of usable cmps (all by default)
    fn usable_count(&self) -> usize;

    /// Get the `CmpMap`
    fn cmp_map(&self) -> &Self::Map;

    /// Get the mut `CmpMap`
    fn cmp_map_mut(&mut self) -> &mut Self::Map;
}

/// A standard [`CmpObserver`] observer
#[derive(Serialize, Deserialize, Debug)]
#[serde(bound = "CM: serde::de::DeserializeOwned + Serialize")]
pub struct StdCmpObserver<'a, CM> {
    cmp_map: OwnedRefMut<'a, CM>,
    size: Option<OwnedRefMut<'a, usize>>,
    name: Cow<'static, str>,
    add_meta: bool,
}

impl<CM> CmpObserver for StdCmpObserver<'_, CM>
where
    CM: HasLen,
{
    type Map = CM;

    /// Get the number of usable cmps (all by default)
    fn usable_count(&self) -> usize {
        match &self.size {
            None => self.cmp_map.as_ref().len(),
            Some(o) => *o.as_ref(),
        }
    }

    fn cmp_map(&self) -> &Self::Map {
        self.cmp_map.as_ref()
    }

    fn cmp_map_mut(&mut self) -> &mut Self::Map {
        self.cmp_map.as_mut()
    }
}

impl<CM, I, S> Observer<I, S> for StdCmpObserver<'_, CM>
where
    CM: Serialize + CmpMap + HasLen,
    S: HasMetadata,
{
    fn pre_exec(&mut self, _state: &mut S, _input: &I) -> Result<(), Error> {
        self.cmp_map.as_mut().reset()?;
        Ok(())
    }

    fn post_exec(&mut self, state: &mut S, _input: &I, _exit_kind: &ExitKind) -> Result<(), Error> {
        if self.add_meta {
            let meta = state.metadata_or_insert_with(CmpValuesMetadata::new);

            meta.add_from(self.usable_count(), self.cmp_map_mut());
        }
        Ok(())
    }
}

impl<CM> Named for StdCmpObserver<'_, CM> {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

impl<'a, CM> StdCmpObserver<'a, CM>
where
    CM: CmpMap,
{
    /// Creates a new [`StdCmpObserver`] with the given name and map.
    #[must_use]
    pub fn new(name: &'static str, map: OwnedRefMut<'a, CM>, add_meta: bool) -> Self {
        Self {
            name: Cow::from(name),
            size: None,
            cmp_map: map,
            add_meta,
        }
    }

    /// Creates a new [`StdCmpObserver`] with the given name, map and reference to variable size.
    #[must_use]
    pub fn with_size(
        name: &'static str,
        cmp_map: OwnedRefMut<'a, CM>,
        add_meta: bool,
        size: OwnedRefMut<'a, usize>,
    ) -> Self {
        Self {
            name: Cow::from(name),
            size: Some(size),
            cmp_map,
            add_meta,
        }
    }
}

/* From AFL++ cmplog.h

#define CMP_MAP_W 65536
#define CMP_MAP_H 32
#define CMP_MAP_RTN_H (CMP_MAP_H / 4)

struct cmp_header {

  unsigned hits : 24;
  unsigned id : 24;
  unsigned shape : 5;
  unsigned type : 2;
  unsigned attribute : 4;
  unsigned overflow : 1;
  unsigned reserved : 4;

} __attribute__((packed));

struct cmp_operands {

  u64 v0;
  u64 v1;
  u64 v0_128;
  u64 v1_128;

} __attribute__((packed));

struct cmpfn_operands {

  u8 v0[31];
  u8 v0_len;
  u8 v1[31];
  u8 v1_len;

} __attribute__((packed));

typedef struct cmp_operands cmp_map_list[CMP_MAP_H];

struct cmp_map {

  struct cmp_header   headers[CMP_MAP_W];
  struct cmp_operands log[CMP_MAP_W][CMP_MAP_H];

};
*/

/// A state metadata holding a list of values logged from comparisons. AFL++ RQ version.
#[derive(Debug, Default, Serialize, Deserialize)]
#[cfg_attr(
    any(not(feature = "serdeany_autoreg"), miri),
    expect(clippy::unsafe_derive_deserialize)
)] // for SerdeAny
pub struct AflppCmpValuesMetadata {
    /// The first map of `AflppCmpLogVals` retrieved by running the un-mutated input
    #[serde(skip)]
    pub orig_cmpvals: HashMap<usize, Vec<CmpValues>>,
    /// The second map of `AflppCmpLogVals` retrieved by runnning the mutated input
    #[serde(skip)]
    pub new_cmpvals: HashMap<usize, Vec<CmpValues>>,
    /// The list of logged idx and headers retrieved by runnning the mutated input
    #[serde(skip)]
    pub headers: Vec<(usize, AflppCmpLogHeader)>,
}

libafl_bolts::impl_serdeany!(AflppCmpValuesMetadata);

impl AflppCmpValuesMetadata {
    /// Constructor for `AflppCmpValuesMetadata`
    #[must_use]
    pub fn new() -> Self {
        Self {
            orig_cmpvals: HashMap::new(),
            new_cmpvals: HashMap::new(),
            headers: Vec::new(),
        }
    }

    /// Getter for `orig_cmpvals`
    #[must_use]
    pub fn orig_cmpvals(&self) -> &HashMap<usize, Vec<CmpValues>> {
        &self.orig_cmpvals
    }

    /// Getter for `new_cmpvals`
    #[must_use]
    pub fn new_cmpvals(&self) -> &HashMap<usize, Vec<CmpValues>> {
        &self.new_cmpvals
    }

    /// Getter for `headers`
    #[must_use]
    pub fn headers(&self) -> &Vec<(usize, AflppCmpLogHeader)> {
        &self.headers
    }
}

/// Comparison header, used to describe a set of comparison values efficiently.
///
/// # Bitfields
///
/// - hits:      The number of hits of a particular comparison
/// - id:        Unused by ``LibAFL``, a unique ID for a particular comparison
/// - shape:     Whether a comparison is u8/u8, u16/u16, etc.
/// - type_:     Whether the comparison value represents an instruction (like a `cmp`) or function
///              call arguments
/// - attribute: OR-ed bitflags describing whether the comparison is <, >, =, <=, >=, or transform
/// - overflow:  Whether the comparison overflows
/// - reserved:  Reserved for future use
#[bitfield(u16)]
#[derive(Debug)]
pub struct AflppCmpLogHeader {
    /// The number of hits of a particular comparison
    ///
    /// 6 bits up to 63 entries, we have `CMP_MAP_H = 32` (so using half of it)
    #[bits(0..=5, r)]
    hits: u6,
    /// Whether a comparison is u8/u8, u16/u16, etc.
    ///
    /// 31 + 1 bytes max
    #[bits(6..=10, r)]
    shape: u5,
    /// Whether the comparison value represents an instruction (like a `cmp`) or function call
    /// arguments
    ///
    /// 2: cmp, rtn
    #[bit(11, r)]
    type_: u1,
    /// OR-ed bitflags describing whether the comparison is <, >, =, <=, >=, or transform
    ///
    /// 16 types for arithmetic comparison types
    #[bits(12..=15, r)]
    attribute: u4,
}
