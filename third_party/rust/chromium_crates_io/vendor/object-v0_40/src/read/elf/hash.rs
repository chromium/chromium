use core::mem;

use crate::elf;
use crate::endian::{U32, U64};
use crate::read::{ReadError, ReadRef, Result, SymbolIndex};

use super::{FileHeader, Sym, SymbolTable, Version, VersionTable};

/// A SysV symbol hash table in an ELF file.
///
/// Returned by [`SectionHeader::hash`](super::SectionHeader::hash).
#[derive(Debug)]
pub struct HashTable<'data, Elf: FileHeader> {
    buckets: &'data [U32<Elf::Endian>],
    chains: &'data [U32<Elf::Endian>],
}

impl<'data, Elf: FileHeader> HashTable<'data, Elf> {
    /// Parse a SysV hash table.
    ///
    /// `data` should be from an [`elf::SHT_HASH`] section, or from a
    /// segment pointed to via the [`elf::DT_HASH`] entry.
    ///
    /// The header is read at offset 0 in the given `data`.
    pub fn parse(endian: Elf::Endian, data: &'data [u8]) -> Result<Self> {
        let mut offset = 0;
        let header = data
            .read::<elf::HashHeader<Elf::Endian>>(&mut offset)
            .read_error("Invalid hash header")?;
        let buckets = data
            .read_slice(&mut offset, header.bucket_count.get(endian) as usize)
            .read_error("Invalid hash buckets")?;
        let chains = data
            .read_slice(&mut offset, header.chain_count.get(endian) as usize)
            .read_error("Invalid hash chains")?;
        Ok(HashTable { buckets, chains })
    }

    /// Return the symbol table length.
    pub fn symbol_table_length(&self) -> u32 {
        self.chains.len() as u32
    }

    fn bucket(&self, endian: Elf::Endian, hash: u32) -> Option<SymbolIndex> {
        let bucket_count = self.buckets.len();
        if bucket_count == 0 {
            return None;
        }
        Some(SymbolIndex(
            self.buckets[(hash as usize) % bucket_count].get(endian) as usize,
        ))
    }

    fn chain(&self, endian: Elf::Endian, index: SymbolIndex) -> Option<SymbolIndex> {
        Some(SymbolIndex(self.chains.get(index.0)?.get(endian) as usize))
    }

    /// Use the hash table to find the symbol table entry with the given name, hash and version.
    pub fn find<R: ReadRef<'data>>(
        &self,
        endian: Elf::Endian,
        name: &[u8],
        hash: u32,
        version: Option<&Version<'_>>,
        symbols: &SymbolTable<'data, Elf, R>,
        versions: &VersionTable<'data, Elf>,
    ) -> Option<(SymbolIndex, &'data Elf::Sym)> {
        // Get the chain start from the bucket for this hash.
        let mut index = self.bucket(endian, hash)?;
        // Avoid infinite loop.
        let mut i = 0;
        let strings = symbols.strings();
        while index != SymbolIndex(0) && i < self.chains.len() {
            if let Ok(symbol) = symbols.symbol(index) {
                if symbol.name(endian, strings) == Ok(name)
                    && versions.matches(endian, index, version)
                {
                    return Some((index, symbol));
                }
            }
            index = self.chain(endian, index)?;
            i += 1;
        }
        None
    }
}

/// A GNU symbol hash table in an ELF file.
///
/// Returned by [`SectionHeader::gnu_hash`](super::SectionHeader::gnu_hash).
#[derive(Debug)]
pub struct GnuHashTable<'data, Elf: FileHeader> {
    symbol_base: u32,
    bloom_shift: u32,
    bloom_filters: &'data [u8],
    buckets: &'data [U32<Elf::Endian>],
    values: &'data [U32<Elf::Endian>],
}

impl<'data, Elf: FileHeader> GnuHashTable<'data, Elf> {
    /// Parse a GNU hash table.
    ///
    /// `data` should be from an [`elf::SHT_GNU_HASH`] section, or from a
    /// segment pointed to via the [`elf::DT_GNU_HASH`] entry.
    ///
    /// The header is read at offset 0 in the given `data`.
    ///
    /// The header does not contain a length field, and so all of `data`
    /// will be used as the hash table values. It does not matter if this
    /// is longer than needed, and this will often the case when accessing
    /// the hash table via the [`elf::DT_GNU_HASH`] entry.
    pub fn parse(endian: Elf::Endian, data: &'data [u8]) -> Result<Self> {
        let mut offset = 0;
        let header = data
            .read::<elf::GnuHashHeader<Elf::Endian>>(&mut offset)
            .read_error("Invalid GNU hash header")?;
        let bloom_len =
            u64::from(header.bloom_count.get(endian)) * mem::size_of::<Elf::Word>() as u64;
        let bloom_filters = data
            .read_bytes(&mut offset, bloom_len)
            .read_error("Invalid GNU hash bloom filters")?;
        let buckets = data
            .read_slice(&mut offset, header.bucket_count.get(endian) as usize)
            .read_error("Invalid GNU hash buckets")?;
        let chain_count = (data.len() - offset as usize) / 4;
        let values = data
            .read_slice(&mut offset, chain_count)
            .read_error("Invalid GNU hash values")?;
        Ok(GnuHashTable {
            symbol_base: header.symbol_base.get(endian),
            bloom_shift: header.bloom_shift.get(endian),
            bloom_filters,
            buckets,
            values,
        })
    }

    /// Return the symbol table index of the first symbol in the hash table.
    pub fn symbol_base(&self) -> u32 {
        self.symbol_base
    }

    /// Determine the symbol table length by finding the last entry in the hash table.
    ///
    /// Returns `None` if the hash table is empty or invalid.
    pub fn symbol_table_length(&self, endian: Elf::Endian) -> Option<u32> {
        // Ensure we find a non-empty bucket.
        if self.symbol_base == 0 {
            return None;
        }

        // Find the highest chain index in a bucket.
        let max_bucket = self.buckets.iter().map(|b| b.get(endian)).max()?;

        // Find the end of the chain.
        let mut chain_length = 0;
        for value in self
            .values
            .get(max_bucket.checked_sub(self.symbol_base)? as usize..)?
        {
            chain_length += 1;
            if value.get(endian) & 1 != 0 {
                return max_bucket.checked_add(chain_length);
            }
        }

        None
    }

    fn bucket(&self, endian: Elf::Endian, hash: u32) -> Option<SymbolIndex> {
        let bucket_count = self.buckets.len();
        if bucket_count == 0 {
            return None;
        }
        Some(SymbolIndex(
            self.buckets[(hash as usize) % bucket_count].get(endian) as usize,
        ))
    }

    /// Use the hash table to find the symbol table entry with the given name, hash, and version.
    pub fn find<R: ReadRef<'data>>(
        &self,
        endian: Elf::Endian,
        name: &[u8],
        hash: u32,
        version: Option<&Version<'_>>,
        symbols: &SymbolTable<'data, Elf, R>,
        versions: &VersionTable<'data, Elf>,
    ) -> Option<(SymbolIndex, &'data Elf::Sym)> {
        let word_bits = mem::size_of::<Elf::Word>() as u32 * 8;

        // Test against bloom filter.
        let bloom_count = self.bloom_filters.len() / mem::size_of::<Elf::Word>();
        if bloom_count == 0 {
            return None;
        }
        let offset =
            ((hash / word_bits) & (bloom_count as u32 - 1)) * mem::size_of::<Elf::Word>() as u32;
        let filter = if word_bits == 64 {
            self.bloom_filters
                .read_at::<U64<Elf::Endian>>(offset.into())
                .ok()?
                .get(endian)
        } else {
            self.bloom_filters
                .read_at::<U32<Elf::Endian>>(offset.into())
                .ok()?
                .get(endian)
                .into()
        };
        if filter & (1 << (hash % word_bits)) == 0 {
            return None;
        }
        if filter & (1 << ((hash >> self.bloom_shift) % word_bits)) == 0 {
            return None;
        }

        // Get the chain start from the bucket for this hash.
        let mut index = self.bucket(endian, hash)?;
        if index == SymbolIndex(0) {
            return None;
        }

        // Test symbols in the chain.
        let strings = symbols.strings();
        let symbols = symbols.symbols().get(index.0..)?;
        let values = self
            .values
            .get(index.0.checked_sub(self.symbol_base as usize)?..)?;
        for (symbol, value) in symbols.iter().zip(values.iter()) {
            let value = value.get(endian);
            if value | 1 == hash | 1 {
                if symbol.name(endian, strings) == Ok(name)
                    && versions.matches(endian, index, version)
                {
                    return Some((index, symbol));
                }
            }
            if value & 1 != 0 {
                break;
            }
            index.0 += 1;
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::elf;
    use crate::endian::LittleEndian;

    type FileHeader = elf::FileHeader64<LittleEndian>;

    #[test]
    fn hash_table_zero_buckets() {
        // bucket_count = 0, chain_count = 0.
        let data = [0u8; 8];
        let table = HashTable::<FileHeader>::parse(LittleEndian, &data).unwrap();
        let symbols = SymbolTable::<FileHeader, &[u8]>::default();
        let versions = VersionTable::default();
        assert!(
            table
                .find(LittleEndian, b"foo", 0, None, &symbols, &versions)
                .is_none()
        );
    }

    #[test]
    fn hash_table_chain_out_of_bounds() {
        let data = [
            1, 0, 0, 0, // bucket_count = 1
            1, 0, 0, 0, // chain_count = 1
            1, 0, 0, 0, // buckets[0] = 1 (invalid)
            0, 0, 0, 0, // chains[0] = 0
        ];
        let table = HashTable::<FileHeader>::parse(LittleEndian, &data).unwrap();
        let symbols = SymbolTable::<FileHeader, &[u8]>::default();
        let versions = VersionTable::default();
        // hash 0 selects buckets[0] = 1, which is out of range for the chains.
        assert!(
            table
                .find(LittleEndian, b"foo", 0, None, &symbols, &versions)
                .is_none()
        );
    }

    #[test]
    fn gnu_hash_table_zero_bloom() {
        let data = [
            1, 0, 0, 0, // bucket_count = 1
            1, 0, 0, 0, // symbol_base = 1
            0, 0, 0, 0, // bloom_count = 0
            0, 0, 0, 0, // bloom_shift = 0
            0, 0, 0, 0, // buckets[0] = 0
            1, 0, 0, 0, // values[0] (chain end bit set)
        ];
        let table = GnuHashTable::<FileHeader>::parse(LittleEndian, &data).unwrap();
        let symbols = SymbolTable::<FileHeader, &[u8]>::default();
        let versions = VersionTable::default();
        assert!(
            table
                .find(LittleEndian, b"foo", 0, None, &symbols, &versions)
                .is_none()
        );
    }

    #[test]
    fn gnu_hash_table_zero_buckets() {
        let data = [
            0, 0, 0, 0, // bucket_count = 0
            1, 0, 0, 0, // symbol_base = 1
            1, 0, 0, 0, // bloom_count = 1
            0, 0, 0, 0, // bloom_shift = 0
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // bloom_filters[0]
            1, 0, 0, 0, // values[0] (chain end bit set)
        ];
        let table = GnuHashTable::<FileHeader>::parse(LittleEndian, &data).unwrap();
        let symbols = SymbolTable::<FileHeader, &[u8]>::default();
        let versions = VersionTable::default();
        // The bloom filter passes so that the empty bucket lookup is reached.
        assert!(
            table
                .find(LittleEndian, b"foo", 0, None, &symbols, &versions)
                .is_none()
        );
    }

    #[test]
    fn gnu_hash_table_symbol_length_overflow() {
        let data = [
            1, 0, 0, 0, // bucket_count = 1
            0xff, 0xff, 0xff, 0xff, // symbol_base = u32::MAX
            1, 0, 0, 0, // bloom_count = 1
            0, 0, 0, 0, // bloom_shift = 0
            0, 0, 0, 0, 0, 0, 0, 0, // bloom_filters[0]
            0xff, 0xff, 0xff, 0xff, // buckets[0] = u32::MAX
            1, 0, 0, 0, // values[0] (chain end bit set)
        ];
        let table = GnuHashTable::<FileHeader>::parse(LittleEndian, &data).unwrap();
        // max_bucket (u32::MAX) + 1 overflows, so this returns None
        assert_eq!(table.symbol_table_length(LittleEndian), None);
    }
}
