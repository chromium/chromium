use crate::endian::Endian;
use crate::macho;
use crate::read::{Bytes, Error, ReadError, ReadRef, Result};

impl<E: Endian> macho::DyldInfoCommand<E> {
    /// Return the data for the rebase operations.
    pub fn rebase_data<'data, R: ReadRef<'data>>(&self, endian: E, data: R) -> Result<&'data [u8]> {
        data.read_bytes_at(
            self.rebase_off.get(endian).into(),
            self.rebase_size.get(endian).into(),
        )
        .read_error("Invalid Mach-O dyld info rebase offset or size")
    }

    /// Return the data for the bind operations.
    pub fn bind_data<'data, R: ReadRef<'data>>(&self, endian: E, data: R) -> Result<&'data [u8]> {
        data.read_bytes_at(
            self.bind_off.get(endian).into(),
            self.bind_size.get(endian).into(),
        )
        .read_error("Invalid Mach-O dyld info bind offset or size")
    }

    /// Return the data for the weak bind operations.
    pub fn weak_bind_data<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<&'data [u8]> {
        data.read_bytes_at(
            self.weak_bind_off.get(endian).into(),
            self.weak_bind_size.get(endian).into(),
        )
        .read_error("Invalid Mach-O dyld info weak bind offset or size")
    }

    /// Return the data for the lazy bind operations.
    pub fn lazy_bind_data<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<&'data [u8]> {
        data.read_bytes_at(
            self.lazy_bind_off.get(endian).into(),
            self.lazy_bind_size.get(endian).into(),
        )
        .read_error("Invalid Mach-O dyld info lazy bind offset or size")
    }

    /// Return the data for the export trie.
    pub fn export_data<'data, R: ReadRef<'data>>(&self, endian: E, data: R) -> Result<&'data [u8]> {
        data.read_bytes_at(
            self.export_off.get(endian).into(),
            self.export_size.get(endian).into(),
        )
        .read_error("Invalid Mach-O dyld info export offset or size")
    }

    /// Return an iterator over the rebase operations.
    pub fn rebase_operations<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<RebaseOperationIterator<'data>> {
        Ok(RebaseOperationIterator::new(
            self.rebase_data(endian, data)?,
        ))
    }

    /// Return an iterator over the bind operations.
    pub fn bind_operations<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<BindOperationIterator<'data>> {
        Ok(BindOperationIterator::new(self.bind_data(endian, data)?))
    }

    /// Return an iterator over the weak bind operations.
    pub fn weak_bind_operations<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<BindOperationIterator<'data>> {
        Ok(BindOperationIterator::new(
            self.weak_bind_data(endian, data)?,
        ))
    }

    /// Return an iterator over the lazy bind operations.
    pub fn lazy_bind_operations<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<BindOperationIterator<'data>> {
        Ok(BindOperationIterator::new(
            self.lazy_bind_data(endian, data)?,
        ))
    }

    /// Return an iterator over the decoded rebases.
    ///
    /// This evaluates the operations from [`Self::rebase_operations`] to generate
    /// one [`Rebase`] per location that requires rebasing.
    pub fn rebases<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
        pointer_size: u8,
    ) -> Result<RebaseIterator<'data>> {
        Ok(RebaseIterator::new(
            self.rebase_operations(endian, data)?,
            pointer_size,
        ))
    }

    /// Return an iterator over the decoded binds.
    ///
    /// This evaluates the operations from [`Self::bind_operations`] to generate
    /// one [`Bind`] per location that requires binding to an symbol.
    pub fn binds<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
        pointer_size: u8,
    ) -> Result<BindIterator<'data>> {
        Ok(BindIterator::new(
            self.bind_operations(endian, data)?,
            pointer_size,
        ))
    }

    /// Return an iterator over the decoded weak binds.
    ///
    /// This evaluates the operations from [`Self::weak_bind_operations`] to generate
    /// one [`Bind`] per location that requires binding to an symbol.
    pub fn weak_binds<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
        pointer_size: u8,
    ) -> Result<BindIterator<'data>> {
        Ok(BindIterator::new_weak(
            self.weak_bind_operations(endian, data)?,
            pointer_size,
        ))
    }

    /// Return an iterator over the decoded lazy binds.
    ///
    /// This evaluates the operations from [`Self::lazy_bind_operations`] to generate
    /// one [`Bind`] per location that requires binding to an symbol.
    pub fn lazy_binds<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
        pointer_size: u8,
    ) -> Result<BindIterator<'data>> {
        Ok(BindIterator::new_lazy(
            self.lazy_bind_operations(endian, data)?,
            pointer_size,
        ))
    }

    /// Return an iterator over the exported symbols.
    pub fn exports_trie<'data, R: ReadRef<'data>>(
        &self,
        endian: E,
        data: R,
    ) -> Result<super::ExportsTrieIterator<'data>> {
        Ok(super::ExportsTrieIterator::new(
            self.export_data(endian, data)?,
        ))
    }
}

/// A low-level iterator over the operations in a Mach-O rebase information stream.
///
/// Returned by [`macho::DyldInfoCommand::rebase_operations`].
#[derive(Debug, Default, Clone, Copy)]
pub struct RebaseOperationIterator<'data> {
    data: Bytes<'data>,
}

impl<'data> RebaseOperationIterator<'data> {
    pub(super) fn new(data: &'data [u8]) -> Self {
        RebaseOperationIterator { data: Bytes(data) }
    }

    /// Return the next rebase operation.
    pub fn next(&mut self) -> Result<Option<(macho::RebaseOpcode, RebaseOperation)>> {
        if self.data.is_empty() {
            return Ok(None);
        }
        let result = RebaseOperation::parse(&mut self.data);
        if result.is_err() {
            self.data = Bytes(&[]);
        }
        result
    }
}

impl<'data> Iterator for RebaseOperationIterator<'data> {
    type Item = Result<(macho::RebaseOpcode, RebaseOperation)>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A rebase operation in a Mach-O rebase information stream.
///
/// Returned by the iterator from [`macho::DyldInfoCommand::rebase_operations`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RebaseOperation {
    /// `REBASE_OPCODE_DONE`
    ///
    /// This can appear in padding at the end of the stream.
    Done,
    /// `REBASE_OPCODE_SET_TYPE_IMM`
    SetType {
        /// The rebase type.
        kind: macho::RebaseType,
    },
    /// `REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB`
    SetSegmentAndOffset {
        /// The segment index.
        index: u8,
        /// The offset within the segment.
        offset: u64,
    },
    /// `REBASE_OPCODE_ADD_ADDR_ULEB`
    AddAddr {
        /// The value to add to the offset.
        offset: u64,
    },
    /// `REBASE_OPCODE_ADD_ADDR_IMM_SCALED`
    AddAddrScaled {
        /// The number of pointer-sized units to add to the offset.
        count: u8,
    },
    /// `REBASE_OPCODE_DO_REBASE_IMM_TIMES` or `REBASE_OPCODE_DO_REBASE_ULEB_TIMES`
    ///
    /// The segment offset is increased by the pointer size after each rebase.
    DoRebaseTimes {
        /// The number of rebases to perform.
        count: u64,
    },
    /// `REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB`
    DoRebaseAddAddr {
        /// The value to add to the offset after rebasing, in addition to the pointer size.
        offset: u64,
    },
    /// `REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB`
    DoRebaseTimesSkipping {
        /// The number of rebases to perform.
        count: u64,
        /// The value to add to the offset after each rebase, in addition to the pointer size.
        skip: u64,
    },
}

impl RebaseOperation {
    fn parse(data: &mut Bytes<'_>) -> Result<Option<(macho::RebaseOpcode, RebaseOperation)>> {
        let context = "Invalid Mach-O rebase operation";
        let byte = data
            .read::<u8>()
            .read_error("Missing Mach-O rebase opcode")?;
        let opcode = macho::RebaseOpcode(byte & macho::REBASE_OPCODE_MASK);
        let immediate = byte & macho::REBASE_IMMEDIATE_MASK;
        let operation = match opcode {
            macho::REBASE_OPCODE_DONE => RebaseOperation::Done,
            macho::REBASE_OPCODE_SET_TYPE_IMM => RebaseOperation::SetType {
                kind: macho::RebaseType(immediate),
            },
            macho::REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB => {
                let offset = data.read_uleb128().read_error(context)?;
                RebaseOperation::SetSegmentAndOffset {
                    index: immediate,
                    offset,
                }
            }
            macho::REBASE_OPCODE_ADD_ADDR_ULEB => {
                let offset = data.read_uleb128().read_error(context)?;
                RebaseOperation::AddAddr { offset }
            }
            macho::REBASE_OPCODE_ADD_ADDR_IMM_SCALED => {
                RebaseOperation::AddAddrScaled { count: immediate }
            }
            macho::REBASE_OPCODE_DO_REBASE_IMM_TIMES => RebaseOperation::DoRebaseTimes {
                count: u64::from(immediate),
            },
            macho::REBASE_OPCODE_DO_REBASE_ULEB_TIMES => {
                let count = data.read_uleb128().read_error(context)?;
                RebaseOperation::DoRebaseTimes { count }
            }
            macho::REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB => {
                let offset = data.read_uleb128().read_error(context)?;
                RebaseOperation::DoRebaseAddAddr { offset }
            }
            macho::REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB => {
                let count = data.read_uleb128().read_error(context)?;
                let skip = data.read_uleb128().read_error(context)?;
                RebaseOperation::DoRebaseTimesSkipping { count, skip }
            }
            _ => return Err(Error("Unsupported Mach-O rebase opcode")),
        };
        Ok(Some((opcode, operation)))
    }
}

/// A decoded rebase in a Mach-O rebase information stream.
///
/// Returned by the iterator from [`macho::DyldInfoCommand::rebases`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Rebase {
    /// The index of the segment that the rebase applies to.
    pub segment_index: u8,
    /// The offset of the rebase within the segment.
    pub segment_offset: u64,
    /// The rebase type.
    pub kind: macho::RebaseType,
}

/// An iterator over the decoded rebases in a Mach-O rebase information stream.
///
/// This evaluates the operations from [`RebaseOperationIterator`] to generate
/// one [`Rebase`] per location that requires rebasing.
///
/// Returned by [`macho::DyldInfoCommand::rebases`].
#[derive(Debug, Clone, Copy)]
pub struct RebaseIterator<'data> {
    operations: RebaseOperationIterator<'data>,
    pointer_size: u8,
    /// The number of rebases remaining in the current operation.
    remaining: u64,
    /// The amount to add to the segment offset after each remaining rebase.
    advance: u64,

    set_segment: bool,
    set_type: bool,
    rebase: Rebase,
}

impl<'data> RebaseIterator<'data> {
    pub(super) fn new(operations: RebaseOperationIterator<'data>, pointer_size: u8) -> Self {
        RebaseIterator {
            operations,
            pointer_size,
            remaining: 0,
            advance: 0,
            set_segment: false,
            set_type: false,
            rebase: Rebase {
                segment_index: 0,
                segment_offset: 0,
                kind: macho::RebaseType(0),
            },
        }
    }

    /// Return the next rebase.
    pub fn next(&mut self) -> Result<Option<Rebase>> {
        let result = self.next_inner();
        if result.is_err() {
            self.remaining = 0;
            self.operations = RebaseOperationIterator::default();
        }
        result
    }

    fn next_inner(&mut self) -> Result<Option<Rebase>> {
        if self.remaining != 0 {
            if !self.set_segment {
                return Err(Error("Missing Mach-O rebase segment and offset"));
            }
            if !self.set_type {
                return Err(Error("Missing Mach-O rebase type"));
            }
            let rebase = self.rebase;
            self.rebase.segment_offset = self.rebase.segment_offset.wrapping_add(self.advance);
            self.remaining -= 1;
            return Ok(Some(rebase));
        }
        loop {
            let Some((_opcode, operation)) = self.operations.next()? else {
                return Ok(None);
            };
            match operation {
                RebaseOperation::Done => return Ok(None),
                RebaseOperation::SetType { kind } => {
                    self.set_type = true;
                    self.rebase.kind = kind;
                }
                RebaseOperation::SetSegmentAndOffset { index, offset } => {
                    self.set_segment = true;
                    self.rebase.segment_index = index;
                    self.rebase.segment_offset = offset;
                }
                RebaseOperation::AddAddr { offset } => {
                    self.rebase.segment_offset = self.rebase.segment_offset.wrapping_add(offset);
                }
                RebaseOperation::AddAddrScaled { count } => {
                    self.rebase.segment_offset = self
                        .rebase
                        .segment_offset
                        .wrapping_add(u64::from(count) * u64::from(self.pointer_size));
                }
                RebaseOperation::DoRebaseTimes { count } => {
                    if count == 0 {
                        continue;
                    }
                    self.remaining = count;
                    self.advance = u64::from(self.pointer_size);
                    return self.next_inner();
                }
                RebaseOperation::DoRebaseAddAddr { offset } => {
                    self.remaining = 1;
                    self.advance = u64::from(self.pointer_size).wrapping_add(offset);
                    return self.next_inner();
                }
                RebaseOperation::DoRebaseTimesSkipping { count, skip } => {
                    if count == 0 {
                        continue;
                    }
                    self.remaining = count;
                    self.advance = u64::from(self.pointer_size).wrapping_add(skip);
                    return self.next_inner();
                }
            }
        }
    }
}

impl<'data> Iterator for RebaseIterator<'data> {
    type Item = Result<Rebase>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A low-level iterator over the operations in a Mach-O bind information stream.
///
/// Returned by [`macho::DyldInfoCommand::bind_operations`],
/// [`macho::DyldInfoCommand::weak_bind_operations`], and
/// [`macho::DyldInfoCommand::lazy_bind_operations`].
#[derive(Debug, Default, Clone, Copy)]
pub struct BindOperationIterator<'data> {
    data: Bytes<'data>,
}

impl<'data> BindOperationIterator<'data> {
    pub(super) fn new(data: &'data [u8]) -> Self {
        BindOperationIterator { data: Bytes(data) }
    }

    /// Return the next bind operation.
    pub fn next(&mut self) -> Result<Option<(macho::BindOpcode, BindOperation<'data>)>> {
        if self.data.is_empty() {
            return Ok(None);
        }
        let result = BindOperation::parse(&mut self.data);
        if result.is_err() {
            self.data = Bytes(&[]);
        }
        result
    }
}

impl<'data> Iterator for BindOperationIterator<'data> {
    type Item = Result<(macho::BindOpcode, BindOperation<'data>)>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// A bind operation in a Mach-O bind information stream.
///
/// Returned by the iterator from [`macho::DyldInfoCommand::bind_operations`],
/// [`macho::DyldInfoCommand::weak_bind_operations`], and
/// [`macho::DyldInfoCommand::lazy_bind_operations`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BindOperation<'data> {
    /// `BIND_OPCODE_DONE`
    ///
    /// This can appear between binds for lazy binds, as well as in padding at the end of
    /// the stream.
    Done,
    /// `BIND_OPCODE_SET_DYLIB_ORDINAL_IMM` or `BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB`
    SetDylibOrdinal {
        /// The dylib ordinal.
        ordinal: u64,
    },
    /// `BIND_OPCODE_SET_DYLIB_SPECIAL_IMM`
    SetDylibSpecial {
        /// The special dylib ordinal.
        ordinal: macho::BindDylib,
    },
    /// `BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM`
    SetSymbol {
        /// The symbol flags.
        flags: macho::BindSymbolFlags,
        /// The symbol name.
        name: &'data [u8],
    },
    /// `BIND_OPCODE_SET_TYPE_IMM`
    SetType {
        /// The bind type.
        kind: macho::BindType,
    },
    /// `BIND_OPCODE_SET_ADDEND_SLEB`
    SetAddend {
        /// The addend.
        addend: i64,
    },
    /// `BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB`
    SetSegmentAndOffset {
        /// The segment index.
        segment_index: u8,
        /// The offset within the segment.
        offset: u64,
    },
    /// `BIND_OPCODE_ADD_ADDR_ULEB`
    AddAddr {
        /// The value to add to the offset.
        offset: u64,
    },
    /// `BIND_OPCODE_DO_BIND`
    DoBind,
    /// `BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB`
    DoBindAddAddr {
        /// The value to add to the offset after binding.
        offset: u64,
    },
    /// `BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED`
    DoBindAddAddrScaled {
        /// The number of pointer-sized units to add to the offset after binding.
        count: u8,
    },
    /// `BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB`
    DoBindTimesSkipping {
        /// The number of binds to perform.
        count: u64,
        /// The value to add to the offset after each bind.
        skip: u64,
    },
}

impl<'data> BindOperation<'data> {
    fn parse(data: &mut Bytes<'data>) -> Result<Option<(macho::BindOpcode, BindOperation<'data>)>> {
        let context = "Invalid Mach-O bind operation";
        let byte = data.read::<u8>().read_error("Missing Mach-O bind opcode")?;
        let opcode = macho::BindOpcode(byte & macho::BIND_OPCODE_MASK);
        let immediate = byte & macho::BIND_IMMEDIATE_MASK;
        let operation = match opcode {
            macho::BIND_OPCODE_DONE => BindOperation::Done,
            macho::BIND_OPCODE_SET_DYLIB_ORDINAL_IMM => BindOperation::SetDylibOrdinal {
                ordinal: u64::from(immediate),
            },
            macho::BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB => {
                let ordinal = data.read_uleb128().read_error(context)?;
                BindOperation::SetDylibOrdinal { ordinal }
            }
            macho::BIND_OPCODE_SET_DYLIB_SPECIAL_IMM => {
                // The immediate is a sign-extended 4-bit value.
                let ordinal = if immediate == 0 {
                    0
                } else {
                    ((!macho::BIND_IMMEDIATE_MASK | immediate) as i8).into()
                };
                BindOperation::SetDylibSpecial {
                    ordinal: macho::BindDylib(ordinal),
                }
            }
            macho::BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM => {
                let name = data.read_string().read_error(context)?;
                BindOperation::SetSymbol {
                    flags: macho::BindSymbolFlags(immediate),
                    name,
                }
            }
            macho::BIND_OPCODE_SET_TYPE_IMM => BindOperation::SetType {
                kind: macho::BindType(immediate),
            },
            macho::BIND_OPCODE_SET_ADDEND_SLEB => {
                let addend = data.read_sleb128().read_error(context)?;
                BindOperation::SetAddend { addend }
            }
            macho::BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB => {
                let offset = data.read_uleb128().read_error(context)?;
                BindOperation::SetSegmentAndOffset {
                    segment_index: immediate,
                    offset,
                }
            }
            macho::BIND_OPCODE_ADD_ADDR_ULEB => {
                let offset = data.read_uleb128().read_error(context)?;
                BindOperation::AddAddr { offset }
            }
            macho::BIND_OPCODE_DO_BIND => BindOperation::DoBind,
            macho::BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB => {
                let offset = data.read_uleb128().read_error(context)?;
                BindOperation::DoBindAddAddr { offset }
            }
            macho::BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED => {
                BindOperation::DoBindAddAddrScaled { count: immediate }
            }
            macho::BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB => {
                let count = data.read_uleb128().read_error(context)?;
                let skip = data.read_uleb128().read_error(context)?;
                BindOperation::DoBindTimesSkipping { count, skip }
            }
            _ => return Err(Error("Unsupported Mach-O bind opcode")),
        };
        Ok(Some((opcode, operation)))
    }
}

/// A decoded bind in a Mach-O bind information stream.
///
/// Returned by the iterator from [`macho::DyldInfoCommand::binds`],
/// [`macho::DyldInfoCommand::weak_binds`], and
/// [`macho::DyldInfoCommand::lazy_binds`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Bind<'data> {
    /// The index of the segment that the bind applies to.
    pub segment_index: u8,
    /// The offset of the bind within the segment.
    pub segment_offset: u64,
    /// The bind type.
    pub kind: macho::BindType,
    /// The ordinal of the library that the symbol is imported from.
    pub dylib: macho::BindDylib,
    /// The symbol name.
    pub symbol: &'data [u8],
    /// The symbol flags.
    pub flags: macho::BindSymbolFlags,
    /// The addend to add to the symbol value.
    pub addend: i64,
}

/// An iterator over the decoded binds in a Mach-O bind information stream.
///
/// This evaluates the operations from [`BindOperationIterator`] to generate
/// one [`Bind`] per location that requires binding.
///
/// Returned by [`macho::DyldInfoCommand::binds`], [`macho::DyldInfoCommand::weak_binds`],
/// and [`macho::DyldInfoCommand::lazy_binds`].
#[derive(Debug, Clone, Copy)]
pub struct BindIterator<'data> {
    operations: BindOperationIterator<'data>,
    pointer_size: u8,
    lazy: bool,
    /// The number of binds remaining in the current operation.
    remaining: u64,
    /// The amount to add to the segment offset after each remaining bind.
    advance: u64,

    set_segment: bool,
    set_type: bool,
    set_dylib: bool,
    set_symbol: bool,
    bind: Bind<'data>,
}

impl<'data> BindIterator<'data> {
    pub(super) fn new(operations: BindOperationIterator<'data>, pointer_size: u8) -> Self {
        Self::new_internal(operations, pointer_size, false, None, None)
    }

    pub(super) fn new_weak(operations: BindOperationIterator<'data>, pointer_size: u8) -> Self {
        Self::new_internal(
            operations,
            pointer_size,
            false,
            Some(macho::BIND_TYPE_POINTER),
            Some(macho::BIND_SPECIAL_DYLIB_WEAK_LOOKUP),
        )
    }

    pub(super) fn new_lazy(operations: BindOperationIterator<'data>, pointer_size: u8) -> Self {
        Self::new_internal(
            operations,
            pointer_size,
            true,
            Some(macho::BIND_TYPE_POINTER),
            None,
        )
    }

    fn new_internal(
        operations: BindOperationIterator<'data>,
        pointer_size: u8,
        lazy: bool,
        kind: Option<macho::BindType>,
        dylib: Option<macho::BindDylib>,
    ) -> Self {
        BindIterator {
            operations,
            pointer_size,
            lazy,
            remaining: 0,
            advance: 0,

            set_segment: false,
            set_type: kind.is_some(),
            set_dylib: dylib.is_some(),
            set_symbol: false,
            bind: Bind {
                segment_index: 0,
                segment_offset: 0,
                kind: kind.unwrap_or(macho::BindType(0)),
                dylib: dylib.unwrap_or(macho::BindDylib(0)),
                symbol: &[],
                flags: macho::BindSymbolFlags(0),
                addend: 0,
            },
        }
    }

    /// Return the next bind.
    pub fn next(&mut self) -> Result<Option<Bind<'data>>> {
        let result = self.next_inner();
        if result.is_err() {
            self.remaining = 0;
            self.operations = BindOperationIterator::default();
        }
        result
    }

    fn next_inner(&mut self) -> Result<Option<Bind<'data>>> {
        if self.remaining != 0 {
            if !self.set_segment {
                return Err(Error("Missing Mach-O bind segment and offset"));
            }
            if !self.set_type {
                return Err(Error("Missing Mach-O bind type"));
            }
            if !self.set_dylib {
                return Err(Error("Missing Mach-O bind dylib"));
            }
            if !self.set_symbol {
                return Err(Error("Missing Mach-O bind symbol"));
            }
            let bind = self.bind;
            self.bind.segment_offset = self.bind.segment_offset.wrapping_add(self.advance);
            self.remaining -= 1;
            return Ok(Some(bind));
        }
        loop {
            let Some((_opcode, operation)) = self.operations.next()? else {
                return Ok(None);
            };
            match operation {
                BindOperation::Done => {
                    if self.lazy {
                        *self = Self::new_lazy(self.operations, self.pointer_size);
                    } else {
                        return Ok(None);
                    }
                }
                BindOperation::SetDylibOrdinal { ordinal } => {
                    let ordinal = i32::try_from(ordinal)
                        .ok()
                        .read_error("Invalid Mach-O bind ordinal")?;
                    self.set_dylib = true;
                    self.bind.dylib = macho::BindDylib(ordinal);
                }
                BindOperation::SetDylibSpecial { ordinal } => {
                    self.set_dylib = true;
                    self.bind.dylib = ordinal;
                }
                BindOperation::SetSymbol { flags, name } => {
                    self.set_symbol = true;
                    self.bind.flags = flags;
                    self.bind.symbol = name;
                }
                BindOperation::SetType { kind } => {
                    self.set_type = true;
                    self.bind.kind = kind;
                }
                BindOperation::SetAddend { addend } => {
                    self.bind.addend = addend;
                }
                BindOperation::SetSegmentAndOffset {
                    segment_index,
                    offset,
                } => {
                    self.set_segment = true;
                    self.bind.segment_index = segment_index;
                    self.bind.segment_offset = offset;
                }
                BindOperation::AddAddr { offset } => {
                    self.bind.segment_offset = self.bind.segment_offset.wrapping_add(offset);
                }
                BindOperation::DoBind => {
                    self.remaining = 1;
                    self.advance = u64::from(self.pointer_size);
                    return self.next_inner();
                }
                BindOperation::DoBindAddAddr { offset } => {
                    self.remaining = 1;
                    self.advance = u64::from(self.pointer_size).wrapping_add(offset);
                    return self.next_inner();
                }
                BindOperation::DoBindAddAddrScaled { count } => {
                    self.remaining = 1;
                    self.advance = u64::from(self.pointer_size) * (u64::from(count) + 1);
                    return self.next_inner();
                }
                BindOperation::DoBindTimesSkipping { count, skip } => {
                    if count == 0 {
                        continue;
                    }
                    self.remaining = count;
                    self.advance = u64::from(self.pointer_size).wrapping_add(skip);
                    return self.next_inner();
                }
            }
        }
    }
}

impl<'data> Iterator for BindIterator<'data> {
    type Item = Result<Bind<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}
