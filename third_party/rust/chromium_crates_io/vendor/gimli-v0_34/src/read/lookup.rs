use crate::common::{DebugInfoOffset, Format};
use crate::constants::GdbIndexSymbolKind;
use crate::read::{Error, Reader, ReaderOffset, Result, UnitOffset};

// Common parsing for the `.debug_pub*` sections (DWARF v4 Section 6.1.1, Lookup by Name).
//
// These sections consist of sets of data. Each set has a header with metadata followed by
// a series of entries.

#[derive(Clone, Debug)]
pub(crate) struct DebugPubSet<R: Reader> {
    pub(crate) section: R,
}

impl<R: Reader> DebugPubSet<R> {
    pub(crate) fn sets(&self, is_gnu: bool) -> PubSetIter<R> {
        PubSetIter {
            input: self.section.clone(),
            is_gnu,
        }
    }

    pub(crate) fn items(&self, is_gnu: bool) -> PubSetEntryIter<R> {
        PubSetEntryIter {
            current_set: None,
            sets: self.sets(is_gnu),
        }
    }
}

#[derive(Clone, Debug)]
pub(crate) struct PubSetIter<R: Reader> {
    input: R,
    is_gnu: bool,
}

impl<R: Reader> PubSetIter<R> {
    /// Advance the iterator and return the next set.
    ///
    /// Returns the newly parsed set as `Ok(Some(PubSet))`. Returns `Ok(None)` when
    /// iteration is complete. If an error occurs while parsing the next header,
    /// then this error is returned as `Err(e)`, and all subsequent calls return
    /// `Ok(None)`.
    pub(crate) fn next(&mut self) -> Result<Option<PubSet<R>>> {
        if self.input.is_empty() {
            return Ok(None);
        }
        match PubSetHeader::parse(&mut self.input) {
            Ok((header, entries)) => Ok(Some(PubSet {
                header,
                entries,
                is_gnu: self.is_gnu,
            })),
            Err(e) => {
                self.input.empty();
                Err(e)
            }
        }
    }
}

#[derive(Clone, Debug)]
pub(crate) struct PubSetEntryIter<R: Reader> {
    // Only none at the very beginning and end.
    // PubSet::entries is consumed as we iterate.
    current_set: Option<PubSet<R>>,
    sets: PubSetIter<R>,
}

impl<R: Reader> PubSetEntryIter<R> {
    /// Advance the iterator and return the next entry.
    ///
    /// Returns the newly parsed entry as `Ok(Some(PubSetEntry))`. Returns
    /// `Ok(None)` when iteration is complete and all entries have already been
    /// parsed and yielded. If an error occurs while parsing the next entry,
    /// then this error is returned as `Err(e)`, and all subsequent calls return
    /// `Ok(None)`.
    pub(crate) fn next(&mut self) -> Result<Option<PubSetEntry<R>>> {
        loop {
            if let Some(set) = &mut self.current_set
                && !set.entries.is_empty()
            {
                match PubSetEntry::parse(&mut set.entries, &set.header, set.is_gnu) {
                    Ok(Some(entry)) => return Ok(Some(entry)),
                    Ok(None) => {
                        self.current_set = None;
                    }
                    Err(e) => {
                        self.sets.input.empty();
                        self.current_set = None;
                        return Err(e);
                    }
                }
            }
            match self.sets.next() {
                Ok(Some(set)) => self.current_set = Some(set),
                Ok(None) => return Ok(None),
                Err(e) => return Err(e),
            }
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct PubSet<R: Reader> {
    pub(crate) header: PubSetHeader<R::Offset>,
    entries: R,
    is_gnu: bool,
}

impl<R: Reader> PubSet<R> {
    /// Return an iterator over just the entries in this set.
    pub(crate) fn items(&self) -> PubSetEntryIter<R> {
        // Empty set iterator.
        let mut set_input = self.entries.clone();
        set_input.empty();
        let sets = PubSetIter {
            input: set_input,
            is_gnu: self.is_gnu,
        };

        PubSetEntryIter {
            sets,
            current_set: Some(self.clone()),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct PubSetHeader<T = usize> {
    pub(crate) format: Format,
    pub(crate) length: T,
    pub(crate) version: u16,
    pub(crate) unit_offset: DebugInfoOffset<T>,
    pub(crate) unit_length: T,
}

impl<T: ReaderOffset> PubSetHeader<T> {
    /// Parse a set header. Returns a tuple of the set header and the entry data.
    fn parse<R: Reader<Offset = T>>(input: &mut R) -> Result<(PubSetHeader<R::Offset>, R)> {
        let (length, format) = input.read_initial_length()?;
        let mut rest = input.split(length)?;

        let version = rest.read_u16()?;
        if version != 2 {
            return Err(Error::UnknownVersion(u64::from(version)));
        }

        let unit_offset = rest.read_offset(format).map(DebugInfoOffset)?;
        let unit_length = rest.read_length(format)?;

        let header = PubSetHeader {
            format,
            length,
            version,
            unit_offset,
            unit_length,
        };
        Ok((header, rest))
    }
}

#[derive(Debug, Clone)]
pub(crate) struct PubSetEntry<R: Reader> {
    pub(crate) unit_header_offset: DebugInfoOffset<R::Offset>,
    pub(crate) die_offset: UnitOffset<R::Offset>,
    pub(crate) name: R,
    flags: u8,
}

impl<R: Reader> PubSetEntry<R> {
    pub(crate) fn is_static(&self) -> bool {
        self.flags & 0x80 != 0
    }

    pub(crate) fn kind(&self) -> GdbIndexSymbolKind {
        GdbIndexSymbolKind((self.flags >> 4) & 7)
    }

    pub(crate) fn flags(&self) -> u8 {
        self.flags
    }

    /// Parse a single set entry. Return `None` for the null entry.
    fn parse(
        input: &mut R,
        header: &PubSetHeader<R::Offset>,
        is_gnu: bool,
    ) -> Result<Option<PubSetEntry<R>>> {
        let offset = input.read_offset(header.format)?;
        if offset.into_u64() == 0 {
            input.empty();
            return Ok(None);
        }

        let flags = if is_gnu { input.read_u8()? } else { 0 };
        let name = input.read_null_terminated_slice()?;
        Ok(Some(PubSetEntry {
            die_offset: UnitOffset(offset),
            name,
            flags,
            unit_header_offset: header.unit_offset,
        }))
    }
}
