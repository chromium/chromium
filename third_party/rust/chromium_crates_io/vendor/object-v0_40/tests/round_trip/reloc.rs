use object::read::{Object, ObjectSection};
use object::{
    Architecture, BinaryFormat, Endianness, RelocationEncoding, RelocationFlags, RelocationKind,
    SymbolFlags, SymbolKind, SymbolScope,
};
use object::{elf, macho, pe, xcoff};
use object::{read, write};

fn check_reloc(
    format: BinaryFormat,
    arch: Architecture,
    kind: RelocationKind,
    encoding: RelocationEncoding,
    size: u8,
    expected: RelocationFlags,
    canonical: bool,
) {
    let endian = match arch {
        Architecture::PowerPc | Architecture::PowerPc64 => Endianness::Big,
        _ => Endianness::Little,
    };
    let mut object = write::Object::new(format, arch, endian);
    let section = object.section_id(write::StandardSection::Text);
    object.append_section_data(section, &[0; 8], 1);
    let symbol = object.add_symbol(write::Symbol {
        name: b"sym".to_vec(),
        value: 0,
        size: 0,
        kind: SymbolKind::Text,
        scope: SymbolScope::Linkage,
        weak: false,
        section: write::SymbolSection::Section(section),
        flags: SymbolFlags::None,
    });
    object
        .add_relocation(
            section,
            write::Relocation {
                offset: 0,
                symbol,
                addend: 0,
                flags: RelocationFlags::Generic {
                    kind,
                    encoding,
                    size,
                },
            },
        )
        .unwrap();
    let bytes = object.write().unwrap();
    let file = read::File::parse(&*bytes).unwrap();
    let section = file.sections().next().unwrap();
    let (_, reloc) = section.relocations().next().unwrap();
    let context =
        format!("(K::{kind:?}, E::{encoding:?}, {size}) A::{arch:?} expected_flags={expected:?}");
    assert_eq!(reloc.flags(), expected, "{context}");
    if canonical {
        assert_eq!(
            reloc.kind(),
            if reloc.subtractor().is_some() {
                assert_eq!(kind, RelocationKind::Relative);
                RelocationKind::Absolute
            } else {
                kind
            },
            "{context}"
        );
        assert_eq!(reloc.encoding(), encoding, "{context}");
        assert_eq!(reloc.size(), size, "{context}");
    }
}

#[test]
fn reloc_round_trip() {
    use Architecture as A;
    use BinaryFormat as F;
    use RelocationEncoding as E;
    use RelocationKind as K;

    let elf_r = |arch, r_type| (F::Elf, arch, RelocationFlags::Elf { r_type });
    let macho_r = |arch, r_type, r_pcrel, r_length| {
        (
            F::MachO,
            arch,
            RelocationFlags::MachO {
                r_type,
                r_pcrel,
                r_length,
            },
        )
    };
    let coff_r = |arch, typ| (F::Coff, arch, RelocationFlags::Coff { typ });
    let xcoff_r =
        |arch, r_rtype, r_rsize| (F::Xcoff, arch, RelocationFlags::Xcoff { r_rtype, r_rsize });

    let cases: Vec<(
        (RelocationKind, RelocationEncoding, u8),
        Vec<(BinaryFormat, Architecture, RelocationFlags)>,
        Vec<(BinaryFormat, Architecture, RelocationFlags)>,
    )> = vec![
        // No relocation. Only supported in ELF.
        (
            (K::None, E::Generic, 0),
            vec![
                elf_r(A::I386, elf::R_386_NONE),
                elf_r(A::X86_64, elf::R_X86_64_NONE),
                elf_r(A::Arm, elf::R_ARM_NONE),
                elf_r(A::Aarch64, elf::R_AARCH64_NONE),
                elf_r(A::PowerPc, elf::R_PPC_NONE),
                elf_r(A::PowerPc64, elf::R_PPC64_NONE),
            ],
            vec![],
        ),
        // Absolute 8-bit.
        (
            (K::Absolute, E::Generic, 8),
            vec![
                elf_r(A::I386, elf::R_386_8),
                elf_r(A::X86_64, elf::R_X86_64_8),
                macho_r(A::I386, macho::GENERIC_RELOC_VANILLA, false, 0),
                macho_r(A::X86_64, macho::X86_64_RELOC_UNSIGNED, false, 0),
            ],
            vec![],
        ),
        // Absolute 16-bit.
        (
            (K::Absolute, E::Generic, 16),
            vec![
                elf_r(A::I386, elf::R_386_16),
                elf_r(A::X86_64, elf::R_X86_64_16),
                elf_r(A::Aarch64, elf::R_AARCH64_ABS16),
                macho_r(A::I386, macho::GENERIC_RELOC_VANILLA, false, 1),
                macho_r(A::X86_64, macho::X86_64_RELOC_UNSIGNED, false, 1),
                macho_r(A::Aarch64, macho::ARM64_RELOC_UNSIGNED, false, 1),
                coff_r(A::I386, pe::IMAGE_REL_I386_DIR16),
            ],
            vec![],
        ),
        // Absolute 32-bit.
        // cranelift: Reloc::Abs4
        (
            (K::Absolute, E::Generic, 32),
            vec![
                elf_r(A::I386, elf::R_386_32),
                elf_r(A::X86_64, elf::R_X86_64_32),
                elf_r(A::Arm, elf::R_ARM_ABS32),
                elf_r(A::Aarch64, elf::R_AARCH64_ABS32),
                elf_r(A::PowerPc, elf::R_PPC_ADDR32),
                elf_r(A::PowerPc64, elf::R_PPC64_ADDR32),
                macho_r(A::I386, macho::GENERIC_RELOC_VANILLA, false, 2),
                macho_r(A::X86_64, macho::X86_64_RELOC_UNSIGNED, false, 2),
                macho_r(A::Arm, macho::ARM_RELOC_VANILLA, false, 2),
                macho_r(A::Aarch64, macho::ARM64_RELOC_UNSIGNED, false, 2),
                macho_r(A::PowerPc, macho::PPC_RELOC_VANILLA, false, 2),
                macho_r(A::PowerPc64, macho::PPC_RELOC_VANILLA, false, 2),
                coff_r(A::I386, pe::IMAGE_REL_I386_DIR32),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_ADDR32),
                coff_r(A::Arm, pe::IMAGE_REL_ARM_ADDR32),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_ADDR32),
                xcoff_r(A::PowerPc, xcoff::R_POS, 31),
                xcoff_r(A::PowerPc64, xcoff::R_POS, 31),
            ],
            vec![],
        ),
        // Absolute 64-bit.
        // cranelift: Reloc::Abs8
        (
            (K::Absolute, E::Generic, 64),
            vec![
                elf_r(A::X86_64, elf::R_X86_64_64),
                elf_r(A::Aarch64, elf::R_AARCH64_ABS64),
                elf_r(A::PowerPc64, elf::R_PPC64_ADDR64),
                macho_r(A::X86_64, macho::X86_64_RELOC_UNSIGNED, false, 3),
                macho_r(A::Aarch64, macho::ARM64_RELOC_UNSIGNED, false, 3),
                macho_r(A::PowerPc64, macho::PPC_RELOC_VANILLA, false, 3),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_ADDR64),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_ADDR64),
                xcoff_r(A::PowerPc64, xcoff::R_POS, 63),
            ],
            vec![],
        ),
        // PC-relative 8-bit.
        (
            (K::Relative, E::Generic, 8),
            vec![
                elf_r(A::I386, elf::R_386_PC8),
                elf_r(A::X86_64, elf::R_X86_64_PC8),
            ],
            vec![],
        ),
        // PC-relative 16-bit.
        (
            (K::Relative, E::Generic, 16),
            vec![
                elf_r(A::I386, elf::R_386_PC16),
                elf_r(A::X86_64, elf::R_X86_64_PC16),
                elf_r(A::Aarch64, elf::R_AARCH64_PREL16),
            ],
            vec![],
        ),
        // PC-relative 32-bit.
        // cranelift: Reloc::X86PCRel4 (only used for COFF "movl (%rip), %eax; IMAGE_REL_AMD64_REL32")
        (
            (K::Relative, E::Generic, 32),
            vec![
                elf_r(A::I386, elf::R_386_PC32),
                elf_r(A::X86_64, elf::R_X86_64_PC32),
                elf_r(A::Aarch64, elf::R_AARCH64_PREL32),
                macho_r(A::Aarch64, macho::ARM64_RELOC_UNSIGNED, false, 2),
                coff_r(A::I386, pe::IMAGE_REL_I386_REL32),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_REL32),
                coff_r(A::Arm, pe::IMAGE_REL_ARM_REL32),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_REL32),
                xcoff_r(A::PowerPc, xcoff::R_REL, 31),
                xcoff_r(A::PowerPc64, xcoff::R_REL, 31),
            ],
            vec![
                // Canonical: E::X86RipRelative
                // TODO: this might need to use unsigned with a subtractor instead
                macho_r(A::X86_64, macho::X86_64_RELOC_SIGNED, true, 2),
            ],
        ),
        // PLT-relative 32-bit.
        (
            (K::PltRelative, E::Generic, 32),
            vec![elf_r(A::I386, elf::R_386_PLT32)],
            vec![
                // Canonical: E::X86Branch
                elf_r(A::X86_64, elf::R_X86_64_PLT32),
                // Canonical: E::X86Branch
                macho_r(A::X86_64, macho::X86_64_RELOC_BRANCH, true, 2),
                // Canonical: E::AArch64Call
                macho_r(A::Aarch64, macho::ARM64_RELOC_BRANCH26, true, 2),
                // Canonical: K::Relative
                coff_r(A::I386, pe::IMAGE_REL_I386_REL32),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_REL32),
                coff_r(A::Arm, pe::IMAGE_REL_ARM_REL32),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_REL32),
            ],
        ),
        // x86 rip relative.
        // Should this be used for cranelift: Reloc::X86PCRel4?
        (
            (K::Relative, E::X86RipRelative, 32),
            vec![macho_r(A::X86_64, macho::X86_64_RELOC_SIGNED, true, 2)],
            vec![
                // Canonical: E::Generic
                elf_r(A::X86_64, elf::R_X86_64_PC32),
                // Canonical: E::Generic
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_REL32),
            ],
        ),
        // x86 branch or call.
        // cranelift: Reloc::X86CallPCRel4
        (
            (K::Relative, E::X86Branch, 32),
            vec![],
            vec![
                // Canonical: K::PltRelative
                elf_r(A::X86_64, elf::R_X86_64_PLT32),
                // Canonical: K::PltRelative
                macho_r(A::X86_64, macho::X86_64_RELOC_BRANCH, true, 2),
                // Canonical: E::Generic
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_REL32),
            ],
        ),
        // x86 PLT branch or call.
        // cranelift: Reloc::X86CallPLTRel4
        (
            (K::PltRelative, E::X86Branch, 32),
            vec![
                elf_r(A::X86_64, elf::R_X86_64_PLT32),
                macho_r(A::X86_64, macho::X86_64_RELOC_BRANCH, true, 2),
            ],
            vec![
                // Canonical: K::Relative, E::Generic
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_REL32),
            ],
        ),
        // AArch64 branch or call.
        // cranelift: Reloc::Arm64Call
        (
            (K::Relative, E::AArch64Call, 26),
            vec![coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_BRANCH26)],
            vec![
                // Canonical: K::PltRelative
                elf_r(A::Aarch64, elf::R_AARCH64_CALL26),
                // Canonical: K::PltRelative
                macho_r(A::Aarch64, macho::ARM64_RELOC_BRANCH26, true, 2),
            ],
        ),
        // AArch64 PLT branch or call.
        (
            (K::PltRelative, E::AArch64Call, 26),
            vec![
                elf_r(A::Aarch64, elf::R_AARCH64_CALL26),
                macho_r(A::Aarch64, macho::ARM64_RELOC_BRANCH26, true, 2),
            ],
            vec![
                // Canonical: K::Relative
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_BRANCH26),
            ],
        ),
        // GOT entry 32-bit offset. Only supported in ELF.
        (
            (K::Got, E::Generic, 32),
            vec![
                elf_r(A::I386, elf::R_386_GOT32),
                elf_r(A::X86_64, elf::R_X86_64_GOT32),
            ],
            vec![],
        ),
        // GOT-relative 32-bit (PC-relative reference to GOT entry).
        // cranelift: Reloc::X86GOTPCRel4
        (
            (K::GotRelative, E::Generic, 32),
            vec![
                elf_r(A::X86_64, elf::R_X86_64_GOTPCREL),
                macho_r(A::X86_64, macho::X86_64_RELOC_GOT, true, 2),
                macho_r(A::Aarch64, macho::ARM64_RELOC_POINTER_TO_GOT, true, 2),
            ],
            vec![],
        ),
        // Image-relative 32-bit (PE RVA). Only supported in COFF.
        (
            (K::ImageOffset, E::Generic, 32),
            vec![
                coff_r(A::I386, pe::IMAGE_REL_I386_DIR32NB),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_ADDR32NB),
                coff_r(A::Arm, pe::IMAGE_REL_ARM_ADDR32NB),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_ADDR32NB),
            ],
            vec![],
        ),
        // Section-relative 32-bit offset. Only supported in COFF.
        // cranelift: Reloc::X86SecRel
        (
            (K::SectionOffset, E::Generic, 32),
            vec![
                coff_r(A::I386, pe::IMAGE_REL_I386_SECREL),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_SECREL),
                coff_r(A::Arm, pe::IMAGE_REL_ARM_SECREL),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_SECREL),
            ],
            vec![],
        ),
        // Section index. Only supported in COFF.
        (
            (K::SectionIndex, E::Generic, 16),
            vec![
                coff_r(A::I386, pe::IMAGE_REL_I386_SECTION),
                coff_r(A::X86_64, pe::IMAGE_REL_AMD64_SECTION),
                coff_r(A::Arm, pe::IMAGE_REL_ARM_SECTION),
                coff_r(A::Aarch64, pe::IMAGE_REL_ARM64_SECTION),
            ],
            vec![],
        ),
    ];

    for ((kind, encoding, size), expected, expected_extra) in cases {
        for (format, arch, expected_flags) in expected {
            check_reloc(format, arch, kind, encoding, size, expected_flags, true);
        }
        for (format, arch, expected_flags) in expected_extra {
            check_reloc(format, arch, kind, encoding, size, expected_flags, false);
        }
    }
}
