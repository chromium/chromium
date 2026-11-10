use crate::macho;
use crate::read::{Bytes, Error, ReadError, Result};
use alloc::boxed::Box;
use alloc::vec::Vec;

// The exports trie is a serialized trie with the following structure:
//
// struct Node {
//     terminal_size: uleb128, // Size of export_data
//     export_data: ExportData, // Absent if terminal_size == 0
//     children_count: u8,
//     edges: [(&[u8] edge_str, uleb128 child_offset); children_count],
//     children: [Node; children_count],
// }
//
// Note that child_offset is relative to the start of the trie data, not the
// current node.
//
// ExportData is a union of the following variants:
//
// struct ExportDataRegular {
//     flags: uleb128,
//     address: uleb128,
// }
//
// struct ExportDataReexport {
//     flags: uleb128,
//     dylib_ordinal: uleb128,
//     import_name: &'data [u8],
// }
//
// struct ExportDataStubAndResolver {
//     flags: uleb128,
//     stub_address: uleb128,
//     resolver_address: uleb128,
// }
//
// ExportData is only present if the current node corresponds to an exported
// symbol. Otherwise it is just an internal node in the prefix trie.

/// Iterator over the exports trie.
#[derive(Debug)]
pub struct ExportsTrieIterator<'data> {
    node_iter: NodeIterator<'data>,
}

impl<'data> ExportsTrieIterator<'data> {
    pub(super) fn new(data: &'data [u8]) -> Self {
        ExportsTrieIterator {
            node_iter: NodeIterator::new(data),
        }
    }

    /// Returns the next exported symbol, if any.
    // All the heavy lifting is done by NodeIterator. This just skips over the internal nodes
    // with no terminal data.
    pub fn next(&mut self) -> Result<Option<ExportSymbol<'data>>> {
        for node in &mut self.node_iter {
            if let Some(export_symbol) = node? {
                return Ok(Some(export_symbol));
            }
        }
        Ok(None)
    }
}

impl<'data> Iterator for ExportsTrieIterator<'data> {
    type Item = Result<ExportSymbol<'data>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// Exported symbol information.
#[derive(Debug)]
pub struct ExportSymbol<'data> {
    name: Box<[u8]>,
    flags: macho::ExportSymbolFlags,
    data: ExportData<'data>,
}

impl<'data> ExportSymbol<'data> {
    /// The name of the exported symbol.
    pub fn name(&self) -> &[u8] {
        &self.name
    }

    /// Consume the symbol and return the name buffer.
    pub fn into_name(self) -> Vec<u8> {
        self.name.into_vec()
    }

    /// The flags for the exported symbol.
    pub fn flags(&self) -> macho::ExportSymbolFlags {
        self.flags
    }

    /// The terminal data for the exported symbol.
    pub fn data(&self) -> &ExportData<'data> {
        &self.data
    }
}

#[derive(Debug)]
struct Frame<'data> {
    data: Bytes<'data>,
    offset: u64,
    children_remaining: u8,
    name_buf_len: usize,
}

#[derive(Debug)]
struct NodeIterator<'data> {
    data: &'data [u8],
    first: bool,
    stack: Vec<Frame<'data>>,
    // Accumulates the prefix edge strings as we traverse the trie.
    name_buf: Vec<u8>,
}

// Implements a DFS pre-order traversal of the exports trie.
impl<'data> NodeIterator<'data> {
    pub(super) fn new(data: &'data [u8]) -> Self {
        NodeIterator {
            data,
            first: true,
            stack: Vec::new(),
            name_buf: Vec::new(),
        }
    }

    fn push_node(&mut self, offset: u64) -> Result<Option<ExportSymbol<'data>>> {
        let mut data = Bytes(
            self.data
                .get(offset as usize..)
                .read_error("Invalid exports trie offset")?,
        );
        let terminal_size = data
            .read_uleb128()
            .read_error("Invalid exports trie terminal size")?;
        let export_data = if terminal_size == 0 {
            None
        } else {
            let (flags, export_data) = ExportData::parse(
                data.read_bytes(terminal_size as usize)
                    .read_error("Exports trie terminal size exceeds bounds")?,
            )?;
            Some(ExportSymbol {
                name: self.name_buf.clone().into_boxed_slice(),
                flags,
                data: export_data,
            })
        };
        let children_count = *data
            .read::<u8>()
            .read_error("Invalid exports trie children count")?;
        self.stack.push(Frame {
            data,
            offset,
            children_remaining: children_count,
            name_buf_len: self.name_buf.len(),
        });
        Ok(export_data)
    }

    // Returns:
    // - `Ok(Some(Some(ExportSymbol)))` if we have terminal data at the current node.
    // - `Ok(Some(None))` if we don't have terminal data at the current node.
    // - `Ok(None)` if we've reached the end of the trie.
    fn next(&mut self) -> Result<Option<Option<ExportSymbol<'data>>>> {
        if self.first {
            self.first = false;
            // The root node is at offset 0.
            return Ok(Some(self.push_node(0)?));
        }
        loop {
            let Some(frame) = self.stack.last_mut() else {
                // The stack only drains once the root node's subtree is fully
                // traversed, so an empty stack here means we are done.
                return Ok(None);
            };
            self.name_buf.truncate(frame.name_buf_len);
            if frame.children_remaining == 0 {
                self.stack.pop();
                continue;
            }
            let edge_str = frame
                .data
                .read_string()
                .read_error("Invalid exports trie edge string")?;
            let child_offset = frame
                .data
                .read_uleb128()
                .read_error("Invalid exports trie child offset")?;
            frame.children_remaining -= 1;
            self.name_buf.extend(edge_str);
            if self.stack.iter().any(|frame| frame.offset == child_offset) {
                return Err(Error("Invalid exports trie child offset"));
            }
            return Ok(Some(self.push_node(child_offset)?));
        }
    }
}

impl<'data> Iterator for NodeIterator<'data> {
    type Item = Result<Option<ExportSymbol<'data>>>;

    fn next(&mut self) -> Option<Self::Item> {
        self.next().transpose()
    }
}

/// Terminal data for an exports trie node.
#[derive(Debug)]
pub enum ExportData<'data> {
    /// A regular export.
    Regular {
        /// The address of the export.
        address: u64,
    },
    /// A re-exported symbol.
    Reexport {
        /// The ordinal of the dylib to re-export from.
        dylib_ordinal: u64,
        /// The name of the symbol to re-export.
        import_name: &'data [u8],
    },
    /// A stub-and-resolver symbol.
    StubAndResolver {
        /// The address of the stub.
        stub_address: u64,
        /// The address of the resolver.
        resolver_address: u64,
    },
}

impl<'data> ExportData<'data> {
    pub(super) fn parse(mut data: Bytes<'data>) -> Result<(macho::ExportSymbolFlags, Self)> {
        let flags = data
            .read_uleb128()
            .map(macho::ExportSymbolFlags)
            .read_error("Invalid exports trie flags")?;
        if flags.has_unknown_bits() {
            return Err(Error("Exports trie flags too large"));
        }
        if flags.contains(macho::EXPORT_SYMBOL_FLAGS_REEXPORT) {
            let dylib_ordinal = data
                .read_uleb128()
                .read_error("Invalid exports trie dylib ordinal")?;
            let import_name = data
                .read_string()
                .read_error("Invalid exports trie import name")?;
            return Ok((
                flags,
                ExportData::Reexport {
                    dylib_ordinal,
                    import_name,
                },
            ));
        }
        if flags.contains(macho::EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER) {
            let stub_address = data
                .read_uleb128()
                .read_error("Invalid exports trie stub address")?;
            let resolver_address = data
                .read_uleb128()
                .read_error("Invalid exports trie resolver address")?;
            return Ok((
                flags,
                ExportData::StubAndResolver {
                    stub_address,
                    resolver_address,
                },
            ));
        }
        let address = data
            .read_uleb128()
            .read_error("Invalid exports trie address")?;
        Ok((flags, ExportData::Regular { address }))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_export() {
        let data = [
            // Root node at offset 0.
            0x00, // terminal_size
            0x01, // children_count
            b'a', 0x00, // edge_str
            0x05, // child_offset
            // Terminal node at offset 5.
            0x02, // terminal_size
            0x00, // flags
            0x10, // address
            0x00, // children_count
        ];
        let mut exports = ExportsTrieIterator::new(&data);
        let symbol = exports.next().unwrap().unwrap();
        assert_eq!(symbol.name(), b"a");
        assert_eq!(symbol.flags(), macho::ExportSymbolFlags(0));
        let ExportData::Regular { address: 0x10 } = symbol.data() else {
            panic!();
        };

        assert!(exports.next().unwrap().is_none());
    }

    #[test]
    fn root_with_no_children() {
        let data = [
            0x00, // terminal_size
            0x00, // children_count
        ];
        let mut exports = ExportsTrieIterator::new(&data);
        assert!(exports.next().unwrap().is_none());
    }

    #[test]
    fn cycle_to_root() {
        // The root's only child points back at offset 0.
        let data = [
            0x00, // terminal_size
            0x01, // children_count
            b'a', 0x00, // edge_str
            0x00, // child_offset (points at the root)
        ];
        let mut exports = ExportsTrieIterator::new(&data);
        assert!(exports.next().is_err());
    }
}
