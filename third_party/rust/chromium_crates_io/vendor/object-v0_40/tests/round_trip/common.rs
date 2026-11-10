#![cfg(all(feature = "read", feature = "write"))]

use object::read::elf::Sym;
use object::read::{Object, ObjectSymbol};
use object::{Architecture, BinaryFormat, Endianness, SymbolFlags, SymbolKind, SymbolScope};
use object::{read, write};

#[test]
fn coff_x86_64_common() {
    let mut object =
        write::Object::new(BinaryFormat::Coff, Architecture::X86_64, Endianness::Little);

    let symbol = write::Symbol {
        name: b"v1".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_common_symbol(symbol, 4, 4);

    let symbol = write::Symbol {
        name: b"v2".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_common_symbol(symbol, 8, 8);

    // Also check undefined symbols, which are very similar.
    let symbol = write::Symbol {
        name: b"v3".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_symbol(symbol);

    let bytes = object.write().unwrap();

    //std::fs::write(&"common.o", &bytes).unwrap();

    let object = read::File::parse(&*bytes).unwrap();
    assert_eq!(object.format(), BinaryFormat::Coff);
    assert_eq!(object.architecture(), Architecture::X86_64);

    let mut symbols = object.symbols();

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("v1"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Common);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(!symbol.is_undefined());
    assert!(symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 4);

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("v2"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Common);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(!symbol.is_undefined());
    assert!(symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 8);

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("v3"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Undefined);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(symbol.is_undefined());
    assert!(!symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 0);

    let symbol = symbols.next();
    assert!(symbol.is_none(), "unexpected symbol {:?}", symbol);
}

#[test]
fn elf_x86_64_common() {
    let mut object =
        write::Object::new(BinaryFormat::Elf, Architecture::X86_64, Endianness::Little);

    let symbol = write::Symbol {
        name: b"v1".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_common_symbol(symbol, 4, 4);

    let symbol = write::Symbol {
        name: b"v2".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_common_symbol(symbol, 8, 8);

    let bytes = object.write().unwrap();

    //std::fs::write(&"common.o", &bytes).unwrap();

    let object = read::elf::ElfFile64::<Endianness>::parse(&*bytes).unwrap();
    assert_eq!(object.architecture(), Architecture::X86_64);

    let mut symbols = object.symbols();

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("v1"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Common);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(!symbol.is_undefined());
    assert!(symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 4);
    // For ELF, the value of a common symbol is its alignment.
    assert_eq!(symbol.elf_symbol().st_value(Endianness::Little), 4);

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("v2"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Common);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(!symbol.is_undefined());
    assert!(symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 8);
    assert_eq!(symbol.elf_symbol().st_value(Endianness::Little), 8);

    let symbol = symbols.next();
    assert!(symbol.is_none(), "unexpected symbol {:?}", symbol);
}

#[test]
fn macho_x86_64_common() {
    let mut object = write::Object::new(
        BinaryFormat::MachO,
        Architecture::X86_64,
        Endianness::Little,
    );

    let symbol = write::Symbol {
        name: b"v1".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_common_symbol(symbol, 4, 4);

    let symbol = write::Symbol {
        name: b"v2".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Data,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Undefined,
        flags: SymbolFlags::None,
    };
    object.add_common_symbol(symbol, 8, 8);

    let bytes = object.write().unwrap();

    //std::fs::write(&"common.o", &bytes).unwrap();

    let object = read::File::parse(&*bytes).unwrap();
    assert_eq!(object.format(), BinaryFormat::MachO);
    assert_eq!(object.architecture(), Architecture::X86_64);

    let mut sections = object.sections();
    let section = sections.next();
    assert!(section.is_none(), "unexpected section {:?}", section);

    let mut symbols = object.symbols();

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("_v1"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Common);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(!symbol.is_undefined());
    assert!(symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 4);
    let SymbolFlags::MachO { n_desc, .. } = symbol.flags() else {
        panic!("unexpected symbol flags");
    };
    assert_eq!(n_desc.common_alignment(), 2);

    let symbol = symbols.next().unwrap();
    println!("{:?}", symbol);
    assert_eq!(symbol.name(), Ok("_v2"));
    assert_eq!(symbol.kind(), SymbolKind::Data);
    assert_eq!(symbol.section(), read::SymbolSection::Common);
    assert_eq!(symbol.scope(), SymbolScope::Linkage);
    assert!(!symbol.is_weak());
    assert!(!symbol.is_undefined());
    assert!(symbol.is_common());
    assert_eq!(symbol.address(), 0);
    assert_eq!(symbol.size(), 8);
    let SymbolFlags::MachO { n_desc, .. } = symbol.flags() else {
        panic!("unexpected symbol flags");
    };
    assert_eq!(n_desc.common_alignment(), 3);

    let symbol = symbols.next();
    assert!(symbol.is_none(), "unexpected symbol {:?}", symbol);
}
