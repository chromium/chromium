// Symphonia
// Copyright (c) 2019-2026 The Project Symphonia Developers.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::io::SeekFrom;

use symphonia_core::io::{MediaSource, ReadBytes, SeekBuffered};
use symphonia_core::util::bits::sign_extend_leq64_to_i64;

/// `EbmlError` provides an enumeration of all possible EBML iterator errors.
#[non_exhaustive]
#[derive(Debug)]
pub enum EbmlError {
    /// An IO error occured while reading, writing, or seeking the EBML document.
    IoError(std::io::Error),
    /// The encoding of an EBML element ID was invalid.
    InvalidEbmlElementIdLength,
    /// The encoding of an EBML element data size was invalid.
    InvalidEbmlDataLength,
    /// The operation could not be completed because the element type is unknown.
    UnknownElement,
    /// The operation could not be completed because the element's data size is not known.
    UnknownElementDataSize,
    /// An unexpected element was encountered. Resync recommended.
    UnexpectedElement,
    /// The element's data type is unexpected.
    UnexpectedElementDataType,
    /// The element's data size is unexpected for its data type.
    UnexpectedElementDataSize,
    /// The current element of the iterator is `None`.
    NoElement,
    /// There is no parent element.
    NoParent,
    /// The specified element is not an ancestor.
    NotAnAncestor,
    /// The parent element was overrun while reading the current element.
    Overrun,
    /// A master element was expected.
    ExpectedMasterElement,
    /// A non-master element was expected.
    ExpectedNonMasterElement,
    /// The seek position is out-of-range.
    SeekOutOfRange,
    /// The provided buffer is too small.
    BufferTooSmall,
    /// Maximum depth reached.
    MaximumDepthReached,
    /// A user-defined error for element decoding/parsing errors.
    ElementError(&'static str),
}

impl From<std::io::Error> for EbmlError {
    fn from(err: std::io::Error) -> EbmlError {
        EbmlError::IoError(err)
    }
}

pub type Result<T> = std::result::Result<T, EbmlError>;

/// A super-trait of `ReadBytes` and `SeekBuffered` that all readers of `EbmlIterator` must
/// implement.
pub(crate) trait ReadEbml: ReadBytes + SeekBuffered {}

/// EBML data types.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub(crate) enum EbmlDataType {
    Master,
    Unsigned,
    Signed,
    Binary,
    String,
    Float,
    Date,
}

/// Trait for an object providing element information in an EBML document schema.
pub(crate) trait EbmlElementInfo: Copy + Clone {
    /// The element type enumeration for the schema.
    type ElementType: Copy + Clone + Default + PartialEq + Eq + PartialOrd + Ord + std::fmt::Debug;

    /// Get the element type.
    fn element_type(&self) -> Self::ElementType;

    /// Get the data type of the element.
    fn data_type(&self) -> EbmlDataType;

    /// Get the EBML element ID of the element's parent.
    ///
    /// For root or global elements, this is 0.
    fn parent_id(&self) -> u32;

    /// The minimum depth in the EBML document this element may exist at.
    ///
    /// For elements that have no recursive parents, this is the *exact* depth the element may
    /// exist.
    fn min_depth(&self) -> u8;

    /// Returns true if the element is a global element.
    fn is_global(&self) -> bool;

    /// Returns true if the element may be nested in itself in addition to it's parent.
    fn is_recursive(&self) -> bool;

    /// Returns true if the element can be of unknown size.
    #[allow(dead_code)]
    fn allow_unknown_size(&self) -> bool;
}

/// Trait implemented for an EBML document schema.
pub(crate) trait EbmlSchema {
    /// The maximum allowed element depth in the EBML document.
    const MAX_DEPTH: usize;

    /// Type describing information about EBML elements in the EBML document schema.
    type ElementInfo: EbmlElementInfo;

    /// Get element information for the given element ID.
    fn get_element_info(&self, id: u32) -> Option<&Self::ElementInfo>;
}

/// An EBML element header.
#[derive(Debug)]
pub(crate) struct EbmlElementHeader<S: EbmlSchema> {
    /// The element ID.
    id: u32,
    /// The depth of the element in the EBML document when it was read.
    depth: u8,
    /// The length of the header.
    header_size: u8,
    /// Element schema information.
    element_info: Option<S::ElementInfo>,
    /// The total size of the element including the header.
    data_size: Option<u64>,
    /// The element's absolute position in the stream.
    pos: u64,
}

// It is not possible to derive `Copy` & `Clone` for `EbmlElementHeader` because the generic
// parameter `S` is not `Copy` + `Clone` and does not need to be since `EbmlElementHeader` doesn't
// ever hold an `S`. Workaround this limitation by implementing these traits manually.
impl<S: EbmlSchema> Copy for EbmlElementHeader<S> {}

impl<S: EbmlSchema> Clone for EbmlElementHeader<S> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<S: EbmlSchema> EbmlElementHeader<S> {
    /// Minimum EBML header size.
    pub const MIN_SIZE: u64 = 2;

    /// Read an EBML element header from the stream.
    pub(crate) fn read<R: ReadBytes>(reader: &mut R, depth: u8, schema: &S) -> Result<Self> {
        // Read the variable width element ID.
        let (id, id_len) = read_element_id(reader)?;

        // Determine the starting position of the element.
        let pos = reader.pos() - u64::from(id_len);

        // Read the variable width element data size.
        let data_size = read_element_data_size(reader)?;

        // Calculate the total length of the header.
        let header_size = (reader.pos() - pos) as u8;

        Ok(EbmlElementHeader {
            id,
            depth,
            header_size,
            element_info: schema.get_element_info(id).copied(),
            data_size,
            pos,
        })
    }

    /// Get the EBML element ID.
    pub(crate) fn id(&self) -> u32 {
        self.id
    }

    /// Get the absolute start position of the element.
    pub(crate) fn pos(&self) -> u64 {
        self.pos
    }

    /// Get the end position of the element if the data size is known.
    pub(crate) fn end(&self) -> Option<u64> {
        self.size().map(|size| self.pos + size)
    }

    /// If the data size of the element is known, returns the total size of the element including
    /// the element header.
    pub(crate) fn size(&self) -> Option<u64> {
        self.data_size.map(|data_size| data_size + u64::from(self.header_size))
    }

    /// Get the absolute position in the stream where the data for the element begins.
    pub(crate) fn data_pos(&self) -> u64 {
        self.pos + u64::from(self.header_size)
    }

    /// Get the data size if it is known.
    #[allow(dead_code)]
    pub(crate) fn data_size(&self) -> Option<u64> {
        self.data_size
    }

    /// Get the elements depth in the document.
    pub(crate) fn depth(&self) -> u8 {
        self.depth
    }

    /// Get element schema information if this is not an unknown element.
    pub(crate) fn element_type(&self) -> <S::ElementInfo as EbmlElementInfo>::ElementType {
        self.element_info.map(|info| info.element_type()).unwrap_or_default()
    }

    /// Check if the element specified by this header is valid at the depth it was read when nested
    /// in the specified parent element.
    ///
    /// Returns `None` if the element is unknown.
    pub(crate) fn is_valid(&self, parent_id: u32) -> Option<bool> {
        self.is_valid_at(self.depth, parent_id)
    }

    /// Check if the element specified by this header is valid at a specific depth when nested in
    /// in the specified parent element.
    ///
    /// Returns `None` if the element is unknown.
    pub(crate) fn is_valid_at(&self, depth: u8, parent_id: u32) -> Option<bool> {
        match self.element_info {
            Some(info) => {
                // The element is known.
                let is_parent_valid = if info.is_global() {
                    // The element is global. It has no specific parent.
                    true
                }
                else {
                    // The element is non-global. It has a parent.
                    if info.is_recursive() {
                        // The element is recursive. Its parent can be itself.
                        info.parent_id() == parent_id || parent_id == self.id
                    }
                    else {
                        // The element is non-recursive.
                        info.parent_id() == parent_id
                    }
                };

                Some(is_parent_valid && info.min_depth() <= depth)
            }
            _ => None,
        }
    }
}

/// Trait for an EBML element.
pub trait EbmlElement<S: EbmlSchema>: Sized {
    /// The element type of this element.
    const TYPE: <S::ElementInfo as EbmlElementInfo>::ElementType;

    /// Read the element.
    fn read<R: ReadEbml>(it: &mut EbmlIterator<R, S>, hdr: &EbmlElementHeader<S>) -> Result<Self>;
}

/// Saved EBML iterator state.
pub(crate) struct EbmlIteratorState<S: EbmlSchema> {
    stack: Vec<EbmlElementHeader<S>>,
    current: Option<EbmlElementHeader<S>>,
    pos: u64,
}

/// An EBML document iterator supporting hierarchical traversal.
pub(crate) struct EbmlIterator<R: ReadEbml, S: EbmlSchema> {
    /// The inner reader.
    reader: R,
    /// The EBML document schema.
    schema: S,
    /// Stack tracking the ancestors of the current element.
    stack: Vec<EbmlElementHeader<S>>,
    /// The header of the current element, if there is one.
    current: Option<EbmlElementHeader<S>>,
    /// The length of the container, if known.
    len: Option<u64>,
}

impl<R: ReadEbml, S: EbmlSchema> EbmlIterator<R, S> {
    pub(crate) fn new(reader: R, schema: S, len: Option<u64>) -> Self {
        // Pre-allocate the iteration stack.
        let stack = Vec::with_capacity(S::MAX_DEPTH);
        Self { reader, schema, stack, current: None, len }
    }

    /// Get a reference to the schema being used by the EBML reader.
    pub(crate) fn schema(&self) -> &S {
        &self.schema
    }

    /// Consume the element reader and return the underlying inner reader.
    pub(crate) fn into_inner(self) -> R {
        self.reader
    }

    /// Get the position of the inner reader.
    pub(crate) fn pos(&self) -> u64 {
        self.reader.pos()
    }

    /// Get the parent element in which the iterator is running.
    ///
    /// This function is guaranteed to be `Some` for all elements except root-level elements. For
    /// root-level elements the parent is the EBML document.
    pub(crate) fn parent(&self) -> Option<&EbmlElementHeader<S>> {
        self.stack.last()
    }

    /// Get the current element the iterator is
    #[allow(dead_code)]
    pub(crate) fn current(&self) -> Option<&EbmlElementHeader<S>> {
        self.current.as_ref()
    }

    /// Get the current depth in the EBML document.
    ///
    /// A depth of 0 indicates the root-level of the EBML document.
    pub(crate) fn depth(&self) -> u8 {
        self.stack.len() as u8
    }

    /// If the current element is a master element, descends iteration into the element.
    pub(crate) fn push_element(&mut self) -> Result<()> {
        // Do not exceed the maximum depth allowed by the schema.
        if self.stack.len() >= S::MAX_DEPTH {
            return Err(EbmlError::MaximumDepthReached);
        }

        if let Some(header) = self.current.take() {
            self.stack.push(header);
            Ok(())
        }
        else {
            // No element to push.
            Err(EbmlError::NoElement)
        }
    }

    /// If the iterator has descended into an element, ascends iteration to the parent element.
    pub(crate) fn pop_element(&mut self) -> Result<()> {
        if let Some(header) = self.stack.pop() {
            self.current.replace(header);
            Ok(())
        }
        else {
            // No element to pop.
            Err(EbmlError::NoParent)
        }
    }

    /// Pops elements from the stack until the specified element is the direct parent. Returns an
    /// error if the element is not in the stack.
    pub(crate) fn pop_elements_upto(
        &mut self,
        element: <S::ElementInfo as EbmlElementInfo>::ElementType,
    ) -> Result<()> {
        // Find the position of the specified parent in the stack from the top of the stack.
        match self.stack.iter().rev().position(|ancestor| ancestor.element_type() == element) {
            Some(rev_pos) => {
                // The reverse iterator position is the number of elements to pop so that the top of
                // the stack will contain the specified parent element. If the top of the stack is
                // the specified parent, the reverse position is 0 (no elements need to be popped).
                for _ in 0..rev_pos {
                    self.pop_element().expect("element should always be popped");
                }
                Ok(())
            }
            _ => {
                // The specified parent element does not exist in the stack.
                Err(EbmlError::NotAnAncestor)
            }
        }
    }

    /// Save and return the state of the iterator.
    pub(crate) fn save_state(&self) -> EbmlIteratorState<S> {
        EbmlIteratorState {
            stack: self.stack.clone(),
            current: self.current,
            pos: self.reader.pos(),
        }
    }

    /// Restore the state of the iterator.
    pub(crate) fn restore_state(&mut self, state: EbmlIteratorState<S>) -> Result<()>
    where
        R: MediaSource,
    {
        self.reader.seek(SeekFrom::Start(state.pos))?;
        self.current = state.current;
        self.stack = state.stack;
        Ok(())
    }

    /// Seek the iterator to a child element at the given offset within the current parent.
    ///
    /// On a successful seek, a call to next_header or next_element will return the child element.
    pub(crate) fn seek_to_child(&mut self, offset: u64) -> Result<()>
    where
        R: MediaSource,
    {
        // Determine the current parent element ID, position, and size. If there is no parent, use
        // the EBML document.
        let (parent_id, parent_pos, parent_size) = self
            .parent()
            .map(|parent| (parent.id(), parent.data_pos(), parent.size()))
            .unwrap_or((0, 0, self.len));

        log::trace!(
            "seeking to child of parent_id={:#x} (pos={}, size={}) at offset={}",
            parent_id,
            parent_pos,
            parent_size.unwrap_or(u64::MAX),
            offset
        );

        // Verify the offset is valid if the size of the parent is known.
        match parent_size {
            Some(size) if offset > size => return Err(EbmlError::SeekOutOfRange),
            _ => (),
        }

        // Compute the absolute position of the child element using the parent's data position, and
        // verify it does not overflow.
        let child_pos = parent_pos.checked_add(offset).ok_or(EbmlError::SeekOutOfRange)?;

        // Seek to the child element position.
        self.reader.seek(SeekFrom::Start(child_pos))?;

        // Reset the iterator so that a call to next_element or next_header yields the child
        // element.
        self.current = None;
        Ok(())
    }

    /// Read the next master element.
    ///
    /// Discards any unread data from the previous element.
    pub(crate) fn next_element<E: EbmlElement<S>>(&mut self) -> Result<E> {
        let _header = self.next_header()?;
        self.read_master_element()
    }

    /// Read the header of the next element.
    ///
    /// Discards any unread data from the previous element.
    pub(crate) fn next_header(&mut self) -> Result<Option<&EbmlElementHeader<S>>> {
        // Consume the current element if it has a known size, and skip past any remaining unread
        // data.
        if let Some(elem) = self.current.take() {
            match elem.end() {
                Some(end) => {
                    // Element had a known size.
                    let pos = self.reader.pos();
                    if pos < end {
                        // Element had unread data, skip it.
                        log::debug!("skipping {} unread bytes", end - pos);
                        self.reader.ignore_bytes(end - pos)?;
                    }
                }
                _ => {
                    // Element data has an unknown size. It is not possible to know if there is
                    // unread data.
                }
            }
        }

        // Get the parent element ID and end position for the current level of iteration. If there
        // is no parent element, then default the values to the document root.
        let (parent_id, parent_end) =
            self.stack.last().map(|parent| (parent.id, parent.end())).unwrap_or((0, self.len));

        // If the end of the parent element/document is known, check if the iterator has reached the
        // end or overrun it.
        if let Some(parent_end) = parent_end {
            let pos = self.reader.pos();

            if pos == parent_end {
                // Iteration of the current parent element is done.
                return Ok(None);
            }
            else if pos > parent_end {
                // The parent element was overrun.
                log::warn!("overran parent element by {} bytes", pos - parent_end);
                return Err(EbmlError::Overrun);
            }
            else if parent_end - pos < EbmlElementHeader::<S>::MIN_SIZE {
                // Remaing byte is not enough for EbmlElementHeader MIN_SIZE of 2 bytes
                // Iteration of the current parent element is done.
                return Ok(None);
            }
        }

        // Get the current depth in the EBML document.
        let depth = self.depth();

        // Read an EBML element header.
        let header = EbmlElementHeader::read(&mut self.reader, depth, &self.schema)?;

        // let indent = 2 * header.depth as usize;
        // log::trace!(
        //     "{} type={:?} ({:#x}), depth={}, pos={}, size={}",
        //     format!("{:indent$}", ""),
        //     header.element_type(),
        //     header.id,
        //     header.depth(),
        //     header.pos,
        //     header.size().unwrap_or(u64::MAX),
        // );

        // Check if the element is valid. If the element is unknown, assume it is valid.
        let is_valid = header.is_valid(parent_id).unwrap_or(true);

        if !is_valid {
            // The element is a known, yet invalid, element (unknown elements are always considered
            // valid) for the immediate parent.

            // In all cases, seek the reader back to the start of the element so iteration could be
            // resumed if the error is resolved.
            self.reader.seek_buffered(header.pos());

            // If the parent element has an unknown size, an invalid element may indicate the end of
            // the parent element. If the element is a valid direct child of any ancestor of the
            // parent element, then the parent element has ended. Return `None` in such a case.
            if parent_end.is_none() {
                for ancestor in self.stack.iter().rev().skip(1) {
                    if header.is_valid_at(ancestor.depth, ancestor.id).unwrap_or(false) {
                        // This element is a direct child of an ancestor. Iteration will be
                        // terminated to indicate the end of the parent element.
                        return Ok(None);
                    }
                }
            }

            // The element does not belong in the parent element or any ancestor. Therefore, it
            // should be impossible to encounter this element. This could be a malformed EBML
            // document.
            log::debug!("unexpected element {:?}", header.element_type());

            return Err(EbmlError::UnexpectedElement);
        }

        // Perform checks after the header parsing
        if let Some(parent_end) = parent_end {
            let pos = self.reader.pos();

            // The parent element was overrun. This can happen during header parsing
            if pos > parent_end {
                log::debug!("element header out-of-bounds, ignoring");
                // Seek back to parent_end, it will always succeed
                self.reader.seek_buffered(parent_end);
                return Ok(None);
            }

            // It is invalid for a child element's end position to exceed its parent's end position.
            // The element may be discarded in such cases.
            if let Some(child_end) = header.end() {
                if child_end > parent_end {
                    // TODO: Maybe scan for other elements instead of skipping to the end of the
                    // parent?
                    log::debug!("element out-of-bounds, ignoring");
                    self.reader.ignore_bytes(parent_end - pos)?;
                    return Ok(None);
                }
            }
        }

        // Element is valid for the current parent and depth of the iterator. Return it.
        self.current = Some(header);
        Ok(self.current.as_ref())
    }

    /// Read the contents of the current element if it is a master element.
    pub(crate) fn read_master_element<E: EbmlElement<S>>(&mut self) -> Result<E> {
        // Get a copy of the current element's header.
        let header = *self.current_or_err()?;

        if let Some(info) = header.element_info {
            // The current element is a known element type.
            if info.data_type() == EbmlDataType::Master && info.element_type() == E::TYPE {
                // The current element is a master element with the same type of the element being
                // read.
                self.push_element()?;
                let element = E::read(self, &header)?;
                self.pop_element()?;
                Ok(element)
            }
            else {
                // Element is not a master element, it cannot be read as an element.
                Err(EbmlError::ExpectedMasterElement)
            }
        }
        else {
            // The current element is not a known element type.
            Err(EbmlError::UnknownElement)
        }
    }

    /// Skip element data instead of reading it.
    pub(crate) fn skip_data(&mut self) -> Result<()> {
        let header = self.current_or_err()?;

        // The data length should be known for all non-master elements.
        let size = header.data_size.ok_or(EbmlError::UnknownElementDataSize)?;
        self.reader.ignore_bytes(size)?;
        self.discard_current();

        Ok(())
    }

    /// Read the value of an element containing unsigned integer data. If the element is empty,
    /// returns `None`.
    pub(crate) fn read_u64(&mut self) -> Result<Option<u64>> {
        let element = self.current_or_err()?;

        match element.element_info {
            Some(info) => match info.data_type() {
                EbmlDataType::Master => Err(EbmlError::ExpectedNonMasterElement),
                EbmlDataType::Unsigned => {
                    let size = element.data_size.ok_or(EbmlError::UnknownElementDataSize)? as usize;
                    match size {
                        0 => Ok(None),
                        1..=8 => {
                            let mut buf = [0u8; 8];
                            self.reader.read_buf_exact(&mut buf[8 - size..])?;
                            self.discard_current();
                            Ok(Some(u64::from_be_bytes(buf)))
                        }
                        _ => Err(EbmlError::UnexpectedElementDataSize),
                    }
                }
                _ => Err(EbmlError::UnexpectedElementDataType),
            },
            _ => Err(EbmlError::UnknownElement),
        }
    }

    /// Read the value of an element containing unsigned integer data. If the element is empty,
    /// returns a user-provided default value instead.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_u64_default(&mut self, default: u64) -> Result<u64> {
        Ok(self.read_u64()?.unwrap_or(default))
    }

    /// Read the value of an element containing unsigned integer data. If the element is empty,
    /// returns 0, the EBML-defined default value for an empty unsigned integer element.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_u64_no_default(&mut self) -> Result<u64> {
        Ok(self.read_u64()?.unwrap_or_default())
    }

    /// Read the value of an element containing signed integer data. If the element is empty,
    /// returns `None`.
    pub(crate) fn read_i64(&mut self) -> Result<Option<i64>> {
        let element = self.current_or_err()?;

        match element.element_info {
            Some(info) => match info.data_type() {
                EbmlDataType::Master => Err(EbmlError::ExpectedNonMasterElement),
                EbmlDataType::Signed => {
                    let size = element.data_size.ok_or(EbmlError::UnknownElementDataSize)? as usize;

                    match size {
                        0 => Ok(None),
                        1..=8 => {
                            let mut buf = [0u8; 8];
                            self.reader.read_buf_exact(&mut buf[8 - size..])?;
                            self.discard_current();
                            let signed =
                                sign_extend_leq64_to_i64(u64::from_be_bytes(buf), 8 * size as u32);
                            Ok(Some(signed))
                        }
                        _ => Err(EbmlError::UnexpectedElementDataSize),
                    }
                }
                _ => Err(EbmlError::UnexpectedElementDataType),
            },
            _ => Err(EbmlError::UnknownElement),
        }
    }

    /// Read the value of an element containing signed integer data. If the element is empty,
    /// returns a user-provided default value instead.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i64_default(&mut self, default: i64) -> Result<i64> {
        Ok(self.read_i64()?.unwrap_or(default))
    }

    /// Read the value of an element containing unsigned integer data. If the element is empty,
    /// returns 0, the EBML-defined default value for an empty signed integer element.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_i64_no_default(&mut self) -> Result<i64> {
        Ok(self.read_i64()?.unwrap_or_default())
    }

    /// Read the value of an element containing floating-point data. If the element is empty,
    /// returns `None`.
    pub(crate) fn read_f64(&mut self) -> Result<Option<f64>> {
        let element = self.current_or_err()?;

        match element.element_info {
            Some(info) => match info.data_type() {
                EbmlDataType::Master => Err(EbmlError::ExpectedNonMasterElement),
                EbmlDataType::Float => {
                    let size = element.data_size.ok_or(EbmlError::UnknownElementDataSize)?;
                    match size {
                        0 => Ok(None),
                        4 => {
                            let value = self.reader.read_be_f32()?;
                            self.discard_current();
                            Ok(Some(f64::from(value)))
                        }
                        8 => {
                            let value = self.reader.read_be_f64()?;
                            self.discard_current();
                            Ok(Some(value))
                        }
                        _ => Err(EbmlError::UnexpectedElementDataSize),
                    }
                }
                _ => Err(EbmlError::UnexpectedElementDataType),
            },
            _ => Err(EbmlError::UnknownElement),
        }
    }

    /// Read the value of an element containing floating-point data. If the element is empty,
    /// returns a user-provided default value instead.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_f64_default(&mut self, default: f64) -> Result<f64> {
        Ok(self.read_f64()?.unwrap_or(default))
    }

    /// Read the value of an element containing floating-point data. If the element is empty,
    /// returns 0.0, the EBML-defined default value for an empty floating-point element.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_f64_no_default(&mut self) -> Result<f64> {
        Ok(self.read_f64()?.unwrap_or_default())
    }

    /// Read the value of an element containing a date. If the element is empty, returns `None`.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_date(&mut self) -> Result<Option<i64>> {
        self.read_i64()
    }

    /// Read the value of an element containing a date. If the element is empty, returns a
    /// user-provided default value instead.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_date_default(&mut self, default: i64) -> Result<i64> {
        Ok(self.read_date()?.unwrap_or(default))
    }

    /// Read the value of an element containing a date. If the element is empty, returns 0, the
    /// EBML-defined default value for an empty date element.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_date_no_default(&mut self) -> Result<i64> {
        Ok(self.read_date()?.unwrap_or_default())
    }

    /// Read the value of an element containing string data. If the element is empty, returns
    /// `None`.
    pub(crate) fn read_string(&mut self) -> Result<Option<String>> {
        let element = self.current_or_err()?;

        match element.element_info {
            Some(info) => match info.data_type() {
                EbmlDataType::Master => Err(EbmlError::ExpectedNonMasterElement),
                EbmlDataType::String => {
                    let size = element.data_size.ok_or(EbmlError::UnknownElementDataSize)? as usize;
                    match size {
                        0 => Ok(None),
                        _ => {
                            let data = self.reader.read_boxed_slice_exact(size)?;
                            self.discard_current();
                            let bytes = data.split(|b| *b == 0).next().unwrap_or(&data);
                            Ok(Some(String::from_utf8_lossy(bytes).into_owned()))
                        }
                    }
                }
                _ => Err(EbmlError::UnexpectedElementDataType),
            },
            _ => Err(EbmlError::UnknownElement),
        }
    }

    /// Read the value of an element containing string data. If the element is empty, returns a
    /// user-provided default value instead.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_string_default(&mut self, default: &str) -> Result<String> {
        Ok(self.read_string()?.unwrap_or_else(|| default.into()))
    }

    /// Read the value of an element containing string data. If the element is empty, returns "",
    /// the EBML-defined default value for an empty string element.
    #[allow(dead_code)]
    #[inline]
    pub(crate) fn read_string_no_default(&mut self) -> Result<String> {
        Ok(self.read_string()?.unwrap_or_default())
    }

    /// Read a boxed slice of the binary data carried by a binary element.
    pub(crate) fn read_binary(&mut self) -> Result<Box<[u8]>> {
        let element = self.current_or_err()?;

        match element.element_info {
            Some(info) => match info.data_type() {
                EbmlDataType::Master => Err(EbmlError::ExpectedNonMasterElement),
                EbmlDataType::Binary => {
                    let size = element.data_size.ok_or(EbmlError::UnknownElementDataSize)? as usize;
                    let data = self.reader.read_boxed_slice_exact(size)?;
                    self.discard_current();
                    Ok(data)
                }
                _ => Err(EbmlError::UnexpectedElementDataType),
            },
            _ => Err(EbmlError::UnknownElement),
        }
    }

    /// Read the binary data carried by a binary element into a provided byte slice.
    ///
    /// It is an error if the buffer is too small.
    pub(crate) fn read_binary_into(&mut self, buf: &mut [u8]) -> Result<usize> {
        let element = self.current_or_err()?;

        match element.element_info {
            Some(info) => match info.data_type() {
                EbmlDataType::Master => Err(EbmlError::ExpectedNonMasterElement),
                EbmlDataType::Binary => {
                    let size = element.data_size.ok_or(EbmlError::UnknownElementDataSize)? as usize;
                    if size > buf.len() {
                        return Err(EbmlError::BufferTooSmall);
                    }
                    let read_len = self.reader.read_buf(&mut buf[..size])?;
                    self.discard_current();
                    Ok(read_len)
                }
                _ => Err(EbmlError::UnexpectedElementDataType),
            },
            _ => Err(EbmlError::UnknownElement),
        }
    }

    /// Get the current element or return an error if there is no current element.
    #[inline]
    fn current_or_err(&self) -> Result<&EbmlElementHeader<S>> {
        self.current.as_ref().ok_or(EbmlError::NoElement)
    }

    /// Discard the current element.
    ///
    /// NOTE: It is a logic error to call this before fully consuming the element. Will panic in
    /// debug builds if not.
    #[inline(always)]
    fn discard_current(&mut self) {
        debug_assert!(
            self.current.as_ref().and_then(|c| c.end()) == Some(self.reader.pos()),
            "discarding current element before consuming it fully"
        );
        self.current = None;
    }
}

// EBML Primitives

/// Read an EBML element ID (as in RFC8794) from the current position of the reader and returns
/// its value, and length in bytes (1-4).
fn read_element_id<R: ReadBytes>(reader: &mut R) -> Result<(u32, u8)> {
    // Read the leading byte of the element ID.
    let byte = reader.read_byte()?;

    // The number of leading zeros indicate length of the element ID in bytes.
    let len = byte.leading_zeros() as u8 + 1;
    if len > 4 {
        // First byte should be ignored since we know it could not start a tag.
        // We immediately proceed to seek a first valid tag.
        return Err(EbmlError::InvalidEbmlElementIdLength);
    }

    // Read remaining octets
    let mut id = u32::from(byte);
    for _ in 1..len {
        let byte = reader.read_byte()?;
        id = (id << 8) | u32::from(byte);
    }

    // log::debug!("element with tag: {:X}", id);
    Ok((id, len))
}

/// Read the size of an EBML element.
fn read_element_data_size<R: ReadBytes>(reader: &mut R) -> Result<Option<u64>> {
    let (size, len) = read_vint(reader)?;

    // If the VINT_DATA portion of the variable sized unsigned integer representing the data size is
    // all 1s, then the element data size is unknown. The VINT_DATA portion of the decoded integer
    // is contained in the lower 7 * n bits, here n is the length of bytes of integer.
    let mask = (1 << (7 * u32::from(len))) - 1;

    if size & mask == mask {
        return Ok(None);
    }

    Ok(Some(size))
}

/// Read an unsigned variable size integer (as in RFC8794) from the reader and return it or an
/// error.
pub(crate) fn read_unsigned_vint<R: ReadBytes>(reader: &mut R) -> Result<u64> {
    Ok(read_vint(reader)?.0)
}

/// Read a signed variable size integer (as in RFC8794) from the reader and return it or an error.
pub(crate) fn read_signed_vint<R: ReadBytes>(reader: &mut R) -> Result<i64> {
    let (value, len) = read_vint(reader)?;
    // Convert to a signed integer by range shifting.
    let half_range = i64::pow(2, (u32::from(len) * 7) - 1) - 1;
    Ok(value as i64 - half_range)
}

/// Read an unsigned variable size integer (as in RFC8794) from the stream and return both its value
/// and length in byte, or an error.
fn read_vint<R: ReadBytes>(mut reader: R) -> Result<(u64, u8)> {
    let byte = reader.read_byte()?;

    // Determine VINT_WIDTH + 1, the total length of the variable size integer.
    let len = byte.leading_zeros() as u8 + 1;
    if len > 8 {
        return Err(EbmlError::InvalidEbmlDataLength);
    }

    let mut vint = u64::from(byte);
    // Clear VINT_MARKER bit
    vint ^= 1 << (8 - len);

    // Read remaining bytes.
    for _ in 1..len {
        let byte = reader.read_byte()?;
        vint = (vint << 8) | u64::from(byte);
    }

    Ok((vint, len))
}

#[cfg(test)]
mod tests {
    use symphonia_core::io::BufReader;

    use super::{read_element_id, read_signed_vint, read_unsigned_vint};

    #[test]
    fn verify_read_element_id() {
        assert_eq!(read_element_id(&mut BufReader::new(&[0x82])).unwrap(), (0x82, 1));
        assert_eq!(read_element_id(&mut BufReader::new(&[0x40, 0x02])).unwrap(), (0x4002, 2));
        assert_eq!(
            read_element_id(&mut BufReader::new(&[0x20, 0x00, 0x02])).unwrap(),
            (0x200002, 3)
        );
        assert_eq!(
            read_element_id(&mut BufReader::new(&[0x10, 0x00, 0x00, 0x02])).unwrap(),
            (0x10000002, 4)
        );
    }

    #[test]
    fn verify_read_unsigned_vint() {
        assert_eq!(read_unsigned_vint(&mut BufReader::new(&[0x82])).unwrap(), 2);
        assert_eq!(read_unsigned_vint(&mut BufReader::new(&[0x40, 0x02])).unwrap(), 2);
        assert_eq!(read_unsigned_vint(&mut BufReader::new(&[0x20, 0x00, 0x02])).unwrap(), 2);
        assert_eq!(read_unsigned_vint(&mut BufReader::new(&[0x10, 0x00, 0x00, 0x02])).unwrap(), 2);
        assert_eq!(
            read_unsigned_vint(&mut BufReader::new(&[0x08, 0x00, 0x00, 0x00, 0x02])).unwrap(),
            2
        );
        assert_eq!(
            read_unsigned_vint(&mut BufReader::new(&[0x04, 0x00, 0x00, 0x00, 0x00, 0x02])).unwrap(),
            2
        );
        assert_eq!(
            read_unsigned_vint(&mut BufReader::new(&[0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02]))
                .unwrap(),
            2
        );
        assert_eq!(
            read_unsigned_vint(&mut BufReader::new(&[
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
            ]))
            .unwrap(),
            2
        );
    }

    #[test]
    fn verify_read_signed_vint() {
        assert_eq!(read_signed_vint(&mut BufReader::new(&[0x80])).unwrap(), -63);
        assert_eq!(read_signed_vint(&mut BufReader::new(&[0x40, 0x00])).unwrap(), -8191);
    }
}
