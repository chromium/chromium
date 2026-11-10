use alloc::borrow::Cow;
use alloc::boxed::Box;
use alloc::sync::Arc;
use alloc::vec::Vec;
use core::cell::OnceCell;
use std::ffi::OsStr;
use std::fs::File;
use std::path::{Path, PathBuf};

use gimli::Reader;
use memmap2::Mmap;
use object::{Object, ObjectMapFile, ObjectSection, SymbolMap, SymbolMapName};
use typed_arena::Arena;

use crate::{
    Context, FrameIter, Location, LocationRangeIter, LookupContinuation, LookupResult,
    SplitDwarfLoad,
};

/// The type used by [`Loader`] for reading DWARF data.
///
/// This is used in the return types of the methods of [`Loader`].
type LoaderReader<'a> =
    gimli::RelocateReader<gimli::EndianSlice<'a, gimli::RunTimeEndian>, &'a LoaderRelocationMap>;

type Error = Box<dyn std::error::Error>;
type Result<T> = std::result::Result<T, Error>;

#[derive(Default)]
struct LoaderArena {
    data: Arena<Vec<u8>>,
    mmap: Arena<Mmap>,
    relocation: Arena<LoaderRelocationMap>,
}

/// A loader for the DWARF data required for a `Context`.
///
/// For performance reasons, a [`Context`] normally borrows the input data.
/// However, that means the input data must outlive the `Context`, which
/// is inconvenient for long-lived `Context`s.
/// This loader uses an arena to store the input data, together with the
/// `Context` itself. This ensures that the input data lives as long as
/// the `Context`.
///
/// The loader performs some additional tasks:
/// - Loads the symbol table from the executable file (see [`Self::find_symbol`]).
/// - Loads Mach-O dSYM files that are located next to the executable file.
/// - Locates and loads split DWARF files (DWO and DWP).
pub struct Loader {
    internal: LoaderInternal<'static>,
    arena: LoaderArena,
}

impl Loader {
    /// Load the DWARF data for an executable file and create a `Context`.
    #[inline]
    pub fn new(path: impl AsRef<Path>) -> Result<Self> {
        Self::new_with_sup(path, None::<&Path>)
    }

    /// Load the DWARF data for an executable file and create a `Context`.
    ///
    /// Optionally also use a supplementary object file.
    pub fn new_with_sup(
        path: impl AsRef<Path>,
        sup_path: Option<impl AsRef<Path>>,
    ) -> Result<Self> {
        let arena = LoaderArena::default();

        let internal =
            LoaderInternal::new(path.as_ref(), sup_path.as_ref().map(AsRef::as_ref), &arena)?;
        Ok(Loader {
            // Convert to static lifetime to allow self-reference by `internal`.
            // `internal` is only accessed through `borrow_internal`, which ensures
            // that the static lifetime does not leak.
            internal: unsafe {
                core::mem::transmute::<LoaderInternal<'_>, LoaderInternal<'static>>(internal)
            },
            arena,
        })
    }

    fn borrow_internal<'a, F, T>(&'a self, f: F) -> T
    where
        F: FnOnce(&'a LoaderInternal<'a>, &'a LoaderArena) -> T,
    {
        // Do not leak the static lifetime.
        let internal = unsafe {
            core::mem::transmute::<&LoaderInternal<'static>, &'a LoaderInternal<'a>>(&self.internal)
        };
        f(internal, &self.arena)
    }

    /// Get the base address used for relative virtual addresses.
    ///
    /// Currently this is only non-zero for PE.
    pub fn relative_address_base(&self) -> u64 {
        self.borrow_internal(|i, _arena| i.relative_address_base)
    }

    /// Find the source file and line corresponding to the given virtual memory address.
    ///
    /// This calls [`Context::find_location`] with the given address.
    pub fn find_location(&self, probe: u64) -> Result<Option<Location<'_>>> {
        self.borrow_internal(|i, arena| i.find_location(probe, arena))
    }

    /// Return source file and lines for a range of addresses.
    ///
    /// This calls [`Context::find_location_range`] with the given range.
    pub fn find_location_range(
        &self,
        probe_low: u64,
        probe_high: u64,
    ) -> Result<LocationRangeIter<'_, impl gimli::Reader + '_>> {
        self.borrow_internal(|i, arena| i.find_location_range(probe_low, probe_high, arena))
    }

    /// Return an iterator for the function frames corresponding to the given virtual
    /// memory address.
    ///
    /// This calls [`Context::find_frames`] with the given address.
    pub fn find_frames(&self, probe: u64) -> Result<FrameIter<'_, impl gimli::Reader + '_>> {
        self.borrow_internal(|i, arena| i.find_frames(probe, arena))
    }

    /// Find the symbol table entry corresponding to the given virtual memory address.
    /// Return the symbol name.
    pub fn find_symbol(&self, probe: u64) -> Option<&str> {
        self.find_symbol_info(probe).map(|symbol| symbol.name)
    }

    /// Find the symbol table entry corresponding to the given virtual memory address.
    pub fn find_symbol_info(&self, probe: u64) -> Option<Symbol<'_>> {
        self.borrow_internal(|i, _arena| i.find_symbol_info(probe))
    }

    /// Get the address of a section
    pub fn get_section_range(&self, section_name: &[u8]) -> Option<gimli::Range> {
        self.borrow_internal(|i, _arena| i.get_section_range(section_name))
    }
}

struct LoaderInternal<'a> {
    ctx: Context<LoaderReader<'a>>,
    object: object::File<'a>,
    relative_address_base: u64,
    symbols: SymbolMap<SymbolMapName<'a>>,
    dwarf_package: Option<gimli::DwarfPackage<LoaderReader<'a>>>,
    // Map from address to Mach-O object file path.
    object_map: object::ObjectMap<'a>,
    // A context for each Mach-O object file.
    objects: Vec<OnceCell<Option<ObjectContext<'a>>>>,
}

impl<'a> LoaderInternal<'a> {
    fn new(path: &Path, sup_path: Option<&Path>, arena: &'a LoaderArena) -> Result<Self> {
        let file = File::open(path)?;
        let map = arena.mmap.alloc(unsafe { Mmap::map(&file)? });
        let object = object::File::parse(&**map)?;

        let relative_address_base = object.relative_address_base();
        let symbols = object.symbol_map();
        let object_map = object.object_map();
        let mut objects = Vec::new();
        objects.resize_with(object_map.objects().len(), OnceCell::new);

        // Load supplementary object file.
        // TODO: use debuglink and debugaltlink
        let sup_map;
        let sup_object = if let Some(sup_path) = sup_path {
            let sup_file = File::open(sup_path)?;
            sup_map = arena.mmap.alloc(unsafe { Mmap::map(&sup_file)? });
            Some(object::File::parse(&**sup_map)?)
        } else {
            None
        };

        // Load Mach-O dSYM file, ignoring errors.
        let dsym = if let Some(map) = (|| {
            let uuid = object.mach_uuid().ok()??;
            path.parent()?.read_dir().ok()?.find_map(|candidate| {
                let candidate = candidate.ok()?;
                let path = candidate.path();
                if path.extension().and_then(OsStr::to_str) != Some("dSYM") {
                    return None;
                }
                let path = path.join("Contents/Resources/DWARF");
                path.read_dir().ok()?.find_map(|candidate| {
                    let candidate = candidate.ok()?;
                    let path = candidate.path();
                    let file = File::open(path).ok()?;
                    let map = unsafe { Mmap::map(&file) }.ok()?;
                    let object = object::File::parse(&*map).ok()?;
                    if object.mach_uuid() == Ok(Some(uuid)) {
                        Some(map)
                    } else {
                        None
                    }
                })
            })
        })() {
            let map = arena.mmap.alloc(map);
            Some(object::File::parse(&**map)?)
        } else {
            None
        };
        let dwarf_object = dsym.as_ref().unwrap_or(&object);

        // Load the DWARF sections.
        let endian = if dwarf_object.is_little_endian() {
            gimli::RunTimeEndian::Little
        } else {
            gimli::RunTimeEndian::Big
        };
        let mut dwarf =
            gimli::Dwarf::load(|id| load_section(Some(id.name()), dwarf_object, endian, arena))?;
        if let Some(sup_object) = &sup_object {
            dwarf.load_sup(|id| load_section(Some(id.name()), sup_object, endian, arena))?;
        }
        dwarf.populate_abbreviations_cache(gimli::AbbreviationsCacheStrategy::Duplicates);

        let ctx = Context::from_dwarf(dwarf)?;

        // Load the DWP file, ignoring errors.
        let dwarf_package = (|| {
            let mut dwp_path = path.to_path_buf();
            let dwp_extension = path
                .extension()
                .map(|previous_extension| {
                    let mut previous_extension = previous_extension.to_os_string();
                    previous_extension.push(".dwp");
                    previous_extension
                })
                .unwrap_or_else(|| "dwp".into());
            dwp_path.set_extension(dwp_extension);
            let dwp_file = File::open(&dwp_path).ok()?;
            let map = arena.mmap.alloc(unsafe { Mmap::map(&dwp_file) }.ok()?);
            let dwp_object = object::File::parse(&**map).ok()?;

            let endian = if dwp_object.is_little_endian() {
                gimli::RunTimeEndian::Little
            } else {
                gimli::RunTimeEndian::Big
            };
            let empty_relocation = arena.relocation.alloc(LoaderRelocationMap::default());
            let empty =
                LoaderReader::new(gimli::EndianSlice::new(&[][..], endian), empty_relocation);
            gimli::DwarfPackage::load(
                |id| load_section(id.dwo_name(), &dwp_object, endian, arena),
                empty,
            )
            .ok()
        })();

        Ok(LoaderInternal {
            ctx,
            object,
            relative_address_base,
            symbols,
            dwarf_package,
            object_map,
            objects,
        })
    }

    fn ctx(&self, probe: u64, arena: &'a LoaderArena) -> (&Context<LoaderReader<'a>>, u64) {
        self.object_ctx(probe, arena).unwrap_or((&self.ctx, probe))
    }

    fn object_ctx(
        &self,
        probe: u64,
        arena: &'a LoaderArena,
    ) -> Option<(&Context<LoaderReader<'a>>, u64)> {
        let symbol = self.object_map.get(probe)?;
        let object_context = self.objects[symbol.object_index()]
            .get_or_init(|| ObjectContext::new(symbol.object(&self.object_map), arena))
            .as_ref()?;
        object_context.ctx(symbol.name(), probe - symbol.address())
    }

    fn find_symbol_info(&self, probe: u64) -> Option<Symbol<'a>> {
        self.symbols.containing(probe).map(|x| Symbol {
            name: x.name(),
            address: x.address(),
        })
    }

    fn get_section_range(&self, section_name: &[u8]) -> Option<gimli::Range> {
        self.object
            .section_by_name_bytes(section_name)
            .map(|section| {
                let begin = section.address();
                let end = begin + section.size();
                gimli::Range { begin, end }
            })
    }

    fn find_location(&'a self, probe: u64, arena: &'a LoaderArena) -> Result<Option<Location<'a>>> {
        let (ctx, probe) = self.ctx(probe, arena);
        Ok(ctx.find_location(probe)?)
    }

    fn find_location_range(
        &self,
        probe_low: u64,
        probe_high: u64,
        arena: &'a LoaderArena,
    ) -> Result<LocationRangeIter<'_, LoaderReader<'a>>> {
        let (ctx, probe) = self.ctx(probe_low, arena);
        // TODO: handle ranges that cover multiple objects
        let probe_high = probe + (probe_high - probe_low);
        Ok(ctx.find_location_range(probe, probe_high)?)
    }

    fn find_frames(
        &self,
        probe: u64,
        arena: &'a LoaderArena,
    ) -> Result<FrameIter<'_, LoaderReader<'a>>> {
        let (ctx, probe) = self.ctx(probe, arena);
        let mut frames = ctx.find_frames(probe);
        loop {
            let (load, continuation) = match frames {
                LookupResult::Output(output) => return Ok(output?),
                LookupResult::Load { load, continuation } => (load, continuation),
            };

            let r = self.load_dwo(load, arena)?;
            frames = continuation.resume(r);
        }
    }

    fn load_dwo(
        &self,
        load: SplitDwarfLoad<LoaderReader<'a>>,
        arena: &'a LoaderArena,
    ) -> Result<Option<Arc<gimli::Dwarf<LoaderReader<'a>>>>> {
        // Load the DWO file from the DWARF package, if available.
        if let Some(dwp) = self.dwarf_package.as_ref() {
            if let Some(cu) = dwp.find_cu(load.dwo_id, &load.parent)? {
                return Ok(Some(Arc::new(cu)));
            }
        }

        // Determine the path to the DWO file.
        let mut path = PathBuf::new();
        if let Some(p) = load.comp_dir.as_ref() {
            path.push(convert_path(&p.to_slice()?)?);
        }
        let Some(p) = load.path.as_ref() else {
            return Ok(None);
        };
        path.push(convert_path(&p.to_slice()?)?);

        // Load the DWO file, ignoring errors.
        let dwo = (|| {
            let file = File::open(&path).ok()?;
            let map = arena.mmap.alloc(unsafe { Mmap::map(&file) }.ok()?);
            let object = object::File::parse(&**map).ok()?;
            let endian = if object.is_little_endian() {
                gimli::RunTimeEndian::Little
            } else {
                gimli::RunTimeEndian::Big
            };
            let mut dwo_dwarf =
                gimli::Dwarf::load(|id| load_section(id.dwo_name(), &object, endian, arena))
                    .ok()?;
            let dwo_unit_header = dwo_dwarf.units().next().ok()??;
            let dwo_unit = dwo_dwarf.unit(dwo_unit_header).ok()?;
            if dwo_unit.dwo_id != Some(load.dwo_id) {
                return None;
            }
            dwo_dwarf.make_dwo(&load.parent);
            Some(Arc::new(dwo_dwarf))
        })();
        Ok(dwo)
    }
}

struct ObjectContext<'a> {
    ctx: Context<LoaderReader<'a>>,
    symbols: SymbolMap<SymbolMapName<'a>>,
}

impl<'a> ObjectContext<'a> {
    fn new(object: &ObjectMapFile<'a>, arena: &'a LoaderArena) -> Option<Self> {
        let file = File::open(convert_path(object.path()).ok()?).ok()?;
        let map = &**arena.mmap.alloc(unsafe { Mmap::map(&file) }.ok()?);
        let data = if let Some(member_name) = object.member() {
            let archive = object::read::archive::ArchiveFile::parse(map).ok()?;
            let member = archive.members().find_map(|member| {
                let member = member.ok()?;
                if member.name() == member_name {
                    Some(member)
                } else {
                    None
                }
            })?;
            member.data(map).ok()?
        } else {
            map
        };
        let object = object::File::parse(data).ok()?;
        let endian = if object.is_little_endian() {
            gimli::RunTimeEndian::Little
        } else {
            gimli::RunTimeEndian::Big
        };
        let dwarf =
            gimli::Dwarf::load(|id| load_section(Some(id.name()), &object, endian, arena)).ok()?;
        let ctx = Context::from_dwarf(dwarf).ok()?;
        let symbols = object.symbol_map();
        Some(ObjectContext { ctx, symbols })
    }

    fn ctx(&self, symbol_name: &[u8], probe: u64) -> Option<(&Context<LoaderReader<'a>>, u64)> {
        self.symbols
            .symbols()
            .iter()
            .find(|symbol| symbol.name().as_bytes() == symbol_name)
            .map(|symbol| (&self.ctx, probe + symbol.address()))
    }
}

fn load_section<'input>(
    name: Option<&'static str>,
    file: &object::File<'input>,
    endian: gimli::RunTimeEndian,
    arena: &'input LoaderArena,
) -> Result<LoaderReader<'input>> {
    let mut relocations = LoaderRelocationMap::default();
    let data = match name.and_then(|name| file.section_by_name(name)) {
        Some(section) => {
            relocations.add(file, &section);
            match section.uncompressed_data()? {
                Cow::Borrowed(b) => b,
                Cow::Owned(b) => arena.data.alloc(b),
            }
        }
        None => &[],
    };
    let relocations = arena.relocation.alloc(relocations);
    Ok(LoaderReader::new(
        gimli::EndianSlice::new(data, endian),
        relocations,
    ))
}

#[cfg(unix)]
fn convert_path(bytes: &[u8]) -> Result<PathBuf> {
    use std::os::unix::ffi::OsStrExt;
    let s = OsStr::from_bytes(bytes);
    Ok(PathBuf::from(s))
}

#[cfg(not(unix))]
fn convert_path(bytes: &[u8]) -> Result<PathBuf> {
    let s = std::str::from_utf8(bytes)?;
    Ok(PathBuf::from(s))
}

/// Information from a symbol table entry.
pub struct Symbol<'a> {
    name: &'a str,
    address: u64,
}

impl<'a> Symbol<'a> {
    /// Get the symbol name.
    pub fn name(&self) -> &'a str {
        self.name
    }

    /// Get the symbol address.
    pub fn address(&self) -> u64 {
        self.address
    }
}

#[derive(Debug, Default)]
struct LoaderRelocationMap(object::read::RelocationMap);

impl LoaderRelocationMap {
    fn add(&mut self, file: &object::File, section: &object::Section) {
        let mut warned = false;
        for (offset, relocation) in section.relocations() {
            if let Err(e) = self.0.add(file, offset, relocation) {
                if !warned {
                    warned = true;
                    std::eprintln!(
                        "Relocation error for section {} at offset 0x{:08x}: {}",
                        section.name().unwrap(),
                        offset,
                        e
                    );
                }
            }
        }
    }
}

impl gimli::read::Relocate for &'_ LoaderRelocationMap {
    fn relocate_address(&self, offset: usize, value: u64) -> gimli::Result<u64> {
        Ok(self.0.relocate(offset as u64, value))
    }

    fn relocate_offset(&self, offset: usize, value: usize) -> gimli::Result<usize> {
        <usize as gimli::ReaderOffset>::from_u64(self.0.relocate(offset as u64, value as u64))
    }
}
