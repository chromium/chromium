use alloc::vec::Vec;

use crate::read::{Object, ObjectSection, ObjectSymbol, ObjectSymbolTable};
use crate::{SymbolKind, SymbolScope};

/// An entry in a [`SymbolMap`].
pub trait SymbolMapEntry {
    /// The symbol address.
    fn address(&self) -> u64;

    /// The symbol size.
    ///
    /// 0 means the symbol continues to the next entry in the symbol map,
    /// or to the end of the address space if it is the last entry.
    fn size(&self) -> u64 {
        0
    }
}

/// A map from addresses to symbol information.
///
/// The symbol information depends on the chosen entry type, such as [`SymbolMapName`].
///
/// Returned by [`Object::symbol_map`].
#[derive(Debug, Default, Clone)]
pub struct SymbolMap<T: SymbolMapEntry> {
    symbols: Vec<T>,
}

impl<T: SymbolMapEntry> SymbolMap<T> {
    /// Construct a new symbol map.
    ///
    /// This function will sort the symbols by address.
    pub fn new(mut symbols: Vec<T>) -> Self {
        symbols.sort_by_key(|s| s.address());
        SymbolMap { symbols }
    }

    /// Get the symbol before the given address.
    #[deprecated = "use before or containing"]
    pub fn get(&self, address: u64) -> Option<&T> {
        self.before(address)
    }

    /// Get the symbol at or before the given address.
    pub fn before(&self, address: u64) -> Option<&T> {
        let index = match self
            .symbols
            .binary_search_by_key(&address, |symbol| symbol.address())
        {
            Ok(index) => index,
            Err(index) => index.checked_sub(1)?,
        };
        self.symbols.get(index)
    }

    /// Get the symbol containing the given address.
    pub fn containing(&self, address: u64) -> Option<&T> {
        self.before(address).filter(|entry| {
            entry.size() == 0 || address.wrapping_sub(entry.address()) < entry.size()
        })
    }

    /// Get all symbols in the map.
    #[inline]
    pub fn symbols(&self) -> &[T] {
        &self.symbols
    }
}

/// The type used for entries in a [`SymbolMap`] that maps from addresses to names.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SymbolMapName<'data> {
    address: u64,
    size: u64,
    name: &'data str,
}

impl<'data> SymbolMapName<'data> {
    /// Construct a `SymbolMapName`.
    pub fn new(address: u64, size: u64, name: &'data str) -> Self {
        SymbolMapName {
            address,
            size,
            name,
        }
    }

    /// The symbol address.
    #[inline]
    pub fn address(&self) -> u64 {
        self.address
    }

    /// The symbol size.
    #[inline]
    pub fn size(&self) -> u64 {
        self.size
    }

    /// The symbol name.
    #[inline]
    pub fn name(&self) -> &'data str {
        self.name
    }
}

impl<'data> SymbolMapEntry for SymbolMapName<'data> {
    #[inline]
    fn address(&self) -> u64 {
        self.address
    }

    #[inline]
    fn size(&self) -> u64 {
        self.size
    }
}

/// A map from addresses to symbol names and object files.
///
/// This is derived from STAB entries in Mach-O files.
///
/// Returned by [`Object::object_map`].
#[derive(Debug, Default, Clone)]
pub struct ObjectMap<'data> {
    symbols: SymbolMap<ObjectMapEntry<'data>>,
    objects: Vec<ObjectMapFile<'data>>,
}

impl<'data> ObjectMap<'data> {
    #[cfg(feature = "macho")]
    pub(super) fn new(
        symbols: Vec<ObjectMapEntry<'data>>,
        objects: Vec<ObjectMapFile<'data>>,
    ) -> Self {
        ObjectMap {
            symbols: SymbolMap::new(symbols),
            objects,
        }
    }

    /// Get the entry containing the given address.
    pub fn get(&self, address: u64) -> Option<&ObjectMapEntry<'data>> {
        self.symbols.containing(address)
    }

    /// Get all symbols in the map.
    #[inline]
    pub fn symbols(&self) -> &[ObjectMapEntry<'data>] {
        self.symbols.symbols()
    }

    /// Get all objects in the map.
    #[inline]
    pub fn objects(&self) -> &[ObjectMapFile<'data>] {
        &self.objects
    }
}

/// A symbol in an [`ObjectMap`].
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ObjectMapEntry<'data> {
    address: u64,
    size: u64,
    name: &'data [u8],
    object: usize,
}

impl<'data> ObjectMapEntry<'data> {
    #[cfg(feature = "macho")]
    pub(super) fn new(address: u64, size: u64, name: &'data [u8], object: usize) -> Self {
        ObjectMapEntry {
            address,
            size,
            name,
            object,
        }
    }

    /// Get the symbol address.
    #[inline]
    pub fn address(&self) -> u64 {
        self.address
    }

    /// Get the symbol size.
    ///
    /// This may be 0 if the size is unknown.
    #[inline]
    pub fn size(&self) -> u64 {
        self.size
    }

    /// Get the symbol name.
    #[inline]
    pub fn name(&self) -> &'data [u8] {
        self.name
    }

    /// Get the index of the object file name.
    #[inline]
    pub fn object_index(&self) -> usize {
        self.object
    }

    /// Get the object file name.
    #[inline]
    pub fn object<'a>(&self, map: &'a ObjectMap<'data>) -> &'a ObjectMapFile<'data> {
        &map.objects[self.object]
    }
}

impl<'data> SymbolMapEntry for ObjectMapEntry<'data> {
    #[inline]
    fn address(&self) -> u64 {
        self.address
    }

    #[inline]
    fn size(&self) -> u64 {
        self.size
    }
}

/// An object file name in an [`ObjectMap`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ObjectMapFile<'data> {
    path: &'data [u8],
    member: Option<&'data [u8]>,
}

impl<'data> ObjectMapFile<'data> {
    #[cfg(feature = "macho")]
    pub(super) fn new(path: &'data [u8], member: Option<&'data [u8]>) -> Self {
        ObjectMapFile { path, member }
    }

    /// Get the path to the file containing the object.
    #[inline]
    pub fn path(&self) -> &'data [u8] {
        self.path
    }

    /// If the file is an archive, get the name of the member containing the object.
    #[inline]
    pub fn member(&self) -> Option<&'data [u8]> {
        self.member
    }
}

/// A builder for a [`SymbolMap`].
// TODO: builder options for
// - synthetic function start/end (from LC_FUNCTION_STARTS or exception handling)
// - section filter (e.g. text only)
// - map entries (e.g. subtract base address)
#[derive(Debug, Default)]
pub struct SymbolMapBuilder(());

impl SymbolMapBuilder {
    /// Construct a new symbol map builder.
    pub fn new() -> Self {
        Self::default()
    }

    /// Read the symbols from an object file to create a symbol map.
    ///
    /// The map will only contain defined text and data symbols.
    /// The dynamic symbol table will only be used if there are no debugging symbols.
    ///
    /// If symbol sizes are unknown then we guess the size based on the next symbol
    /// or end of section.
    ///
    /// This does not work well if multiple sections use the same base address, which is
    /// common for relocatable object files. The symbols will be overlapping, and the
    /// symbol returned by lookups may be indeterministic. Additionally, if the symbol
    /// size is unknown then we may use a symbol or section end address from a different
    /// section to guess its size.
    pub fn build<'data, O>(self, object: &O) -> SymbolMap<SymbolMapName<'data>>
    where
        O: Object<'data> + ?Sized,
    {
        let mut symbols = Vec::new();
        if let Some(table) = object
            .symbol_table()
            .or_else(|| object.dynamic_symbol_table())
        {
            // Sometimes symbols share addresses. Collect them all then choose the "best".
            let mut all_symbols = Vec::new();
            for symbol in table.symbols() {
                // Must have an address.
                if !symbol.is_definition() {
                    continue;
                }
                // Must have a name.
                let name = match symbol.name() {
                    Ok(name) => name,
                    _ => continue,
                };
                if name.is_empty() {
                    continue;
                }
                let address = symbol.address();
                let size = symbol.size();

                // Lower is better.
                let mut priority = 0u32;

                // Prefer known kind.
                match symbol.kind() {
                    SymbolKind::Text | SymbolKind::Data => {}
                    SymbolKind::Unknown => priority += 1,
                    _ => continue,
                }

                // Prefer XCOFF labels over csects.
                // This special case is needed because labels don't have sizes.
                priority *= 2;
                #[cfg(feature = "xcoff")]
                if let crate::SymbolFlags::Xcoff { x_smtyp, .. } = symbol.flags() {
                    priority += (x_smtyp.typ() != crate::xcoff::XTY_LD) as u32;
                    if size != 0 {
                        // Add end of sized symbols (typically csects) to bound label sizes.
                        all_symbols.push((address.saturating_add(size), !0, !0, !0, ""));
                    }
                }

                // Prefer symbols that have a size.
                priority *= 2;
                priority += (size == 0) as u32;

                // Prefer global visibility.
                priority *= 4;
                priority += match symbol.scope() {
                    SymbolScope::Unknown => 3,
                    SymbolScope::Compilation => 2,
                    SymbolScope::Linkage => 1,
                    SymbolScope::Dynamic => 0,
                };

                // Prefer later entries (earlier symbol is likely to be less specific).
                let index = !0 - symbol.index().0;

                // Tuple is ordered for sort.
                all_symbols.push((address, priority, index, size, name));
            }

            // Add end of sections to improve guesses for unknown sizes.
            for section in object.sections() {
                let address = section.address().saturating_add(section.size());
                all_symbols.push((address, !0, !0, !0, ""));
            }

            // Unstable sort is okay because tuple includes index.
            all_symbols.sort_unstable();

            let mut previous_address = !0;
            for (address, _priority, _index, size, name) in all_symbols {
                if address != previous_address {
                    symbols.push(SymbolMapName::new(address, size, name));
                    previous_address = address;
                }
            }

            // Guess size for symbols with zero size.
            let mut symbol_iter = symbols.iter_mut().rev();
            let mut previous_address = symbol_iter.next().map(|s| s.address).unwrap_or(0);
            for symbol in symbol_iter {
                if symbol.size == 0 {
                    symbol.size = previous_address.saturating_sub(symbol.address);
                }
                previous_address = symbol.address;
            }

            // Remove the entries for end of symbol/section.
            symbols.retain(|x| !x.name.is_empty());
        }
        SymbolMap::new(symbols)
    }
}
