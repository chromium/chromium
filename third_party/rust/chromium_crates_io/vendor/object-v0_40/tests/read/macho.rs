#[cfg(feature = "std")]
use object::{Object, ObjectSection as _};

// Test that entry points are correctly extracted from both LC_MAIN and LC_UNIXTHREAD.
#[cfg(feature = "std")]
#[test]
fn test_macho_entry_point() {
    let macho_testfiles = std::path::Path::new("testfiles/macho");

    // go-x86_64 uses LC_UNIXTHREAD (legacy thread command with register state)
    let path = macho_testfiles.join("go-x86_64");
    let file = std::fs::File::open(&path).unwrap();
    let reader = object::read::ReadCache::new(file);
    let object = object::read::File::parse(&reader).unwrap();
    assert_eq!(
        object.entry(),
        0x10637e0,
        "go-x86_64: entry point from LC_UNIXTHREAD"
    );

    // base-x86_64 uses LC_MAIN (modern entry point command)
    let path = macho_testfiles.join("base-x86_64");
    let file = std::fs::File::open(&path).unwrap();
    let reader = object::read::ReadCache::new(file);
    let object = object::read::File::parse(&reader).unwrap();
    assert_eq!(
        object.entry(),
        0x3f60, // entryoff from LC_MAIN
        "base-x86_64: entry point from LC_MAIN"
    );

    // go-aarch64 uses LC_MAIN (not LC_UNIXTHREAD like go-x86_64)
    let path = macho_testfiles.join("go-aarch64");
    let file = std::fs::File::open(&path).unwrap();
    let reader = object::read::ReadCache::new(file);
    let object = object::read::File::parse(&reader).unwrap();
    assert_eq!(
        object.entry(),
        0x64450, // entryoff from LC_MAIN
        "go-aarch64: entry point from LC_MAIN"
    );

    // static-aarch64 uses LC_UNIXTHREAD (static binary with thread command)
    let path = macho_testfiles.join("static-aarch64");
    let file = std::fs::File::open(&path).unwrap();
    let reader = object::read::ReadCache::new(file);
    let object = object::read::File::parse(&reader).unwrap();
    assert_eq!(
        object.entry(),
        0x1000002f0, // pc from LC_UNIXTHREAD
        "static-aarch64: entry point from LC_UNIXTHREAD"
    );

    // static-x86 uses LC_UNIXTHREAD (32-bit static binary)
    let path = macho_testfiles.join("static-x86");
    let file = std::fs::File::open(&path).unwrap();
    let reader = object::read::ReadCache::new(file);
    let object = object::read::File::parse(&reader).unwrap();
    assert_eq!(
        object.entry(),
        0x1ff0, // eip from LC_UNIXTHREAD
        "static-x86: entry point from LC_UNIXTHREAD"
    );
}

// Test that we can read compressed sections in Mach-O files as produced
// by the Go compiler.
#[cfg(feature = "std")]
#[test]
fn test_go_macho() {
    let macho_testfiles = std::path::Path::new("testfiles/macho");

    // Section names we expect to find, whether they should be
    // compressed, and the actual name of the section in the file.
    const EXPECTED: &[(&str, bool, &str)] = &[
        (".debug_abbrev", true, "__zdebug_abbrev"),
        (".debug_gdb_scripts", false, "__debug_gdb_scri"),
        (".debug_ranges", true, "__zdebug_ranges"),
        ("__data", false, "__data"),
    ];

    for file in &["go-aarch64", "go-x86_64"] {
        let path = macho_testfiles.join(file);
        let file = std::fs::File::open(path).unwrap();
        let reader = object::read::ReadCache::new(file);
        let object = object::read::File::parse(&reader).unwrap();
        for &(name, compressed, actual_name) in EXPECTED {
            let section = object.section_by_name(name).unwrap();
            assert_eq!(section.name(), Ok(actual_name));
            let compressed_file_range = section.compressed_file_range().unwrap();
            let size = section.size();
            if compressed {
                assert_eq!(
                    compressed_file_range.format,
                    object::CompressionFormat::Zlib
                );
                assert_eq!(compressed_file_range.compressed_size, size - 12);
                assert!(
                    compressed_file_range.uncompressed_size > compressed_file_range.compressed_size,
                    "decompressed size is greater than compressed size"
                );
            } else {
                assert_eq!(
                    compressed_file_range.format,
                    object::CompressionFormat::None
                );
                assert_eq!(compressed_file_range.compressed_size, size);
            }
        }
    }
}

#[cfg(feature = "std")]
#[test]
fn test_macho_load_command_offset() {
    use object::LittleEndian;
    use object::read::macho::MachOFile64;

    let path = std::path::Path::new("testfiles/macho").join("base-x86_64");
    let data = std::fs::read(&path).unwrap();
    let object = MachOFile64::<LittleEndian>::parse(&*data).unwrap();

    let mut commands = object.macho_load_commands().unwrap();

    let command = commands.next().unwrap().unwrap();
    let offset = command.offset();
    let cmdsize = command.cmdsize();
    assert_eq!(offset, 32);
    assert_eq!(
        &data[offset as usize..][..cmdsize as usize],
        command.raw_data()
    );

    let command = commands.last().unwrap().unwrap();
    let offset = command.offset();
    let cmdsize = command.cmdsize();
    assert_eq!(
        offset,
        (32 + object.macho_header().sizeofcmds.get(LittleEndian) - cmdsize) as u64
    );
    assert_eq!(
        &data[offset as usize..][..cmdsize as usize],
        command.raw_data()
    );
}
