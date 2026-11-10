use object::read::macho::MachHeader;
use object::read::{Object, ObjectSection};
use object::write::WritableBuffer;
use object::{Architecture, BigEndian, BinaryFormat, Endianness, macho, read, write};

// Test that segment size is valid when the first section needs alignment.
#[test]
fn issue_286_segment_file_size() {
    let mut object = write::Object::new(
        BinaryFormat::MachO,
        Architecture::X86_64,
        Endianness::Little,
    );

    let text = object.section_id(write::StandardSection::Text);
    object.append_section_data(text, &[1; 30], 0x1000);

    let bytes = &*object.write().unwrap();
    let header = macho::MachHeader64::parse(bytes, 0).unwrap();
    let endian: Endianness = header.endian().unwrap();
    let mut commands = header.load_commands(endian, bytes, 0).unwrap();
    let command = commands.next().unwrap().unwrap();
    let (segment, _) = command.segment_64().unwrap().unwrap();
    assert_eq!(segment.vmsize.get(endian), 30);
    assert_eq!(segment.filesize.get(endian), 30);
}

// We were emitting section file alignment padding that didn't match the address alignment padding.
#[test]
fn issue_552_section_file_alignment() {
    let mut object = write::Object::new(
        BinaryFormat::MachO,
        Architecture::X86_64,
        Endianness::Little,
    );

    // The starting file offset is not a multiple of 32 (checked later).
    // Length of 32 ensures that the file offset of the end of this section is still not a
    // multiple of 32.
    let section = object.add_section(vec![], vec![], object::SectionKind::ReadOnlyDataWithRel);
    object.append_section_data(section, &[0u8; 32], 1);

    // Address is already aligned correctly, so there must not any padding,
    // even though file offset is not aligned.
    let section = object.add_section(vec![], vec![], object::SectionKind::ReadOnlyData);
    object.append_section_data(section, &[0u8; 1], 32);

    let bytes = &*object.write().unwrap();
    //std::fs::write(&"align.o", &bytes).unwrap();
    let object = read::File::parse(bytes).unwrap();
    let mut sections = object.sections();

    let section = sections.next().unwrap();
    let offset = section.file_range().unwrap().0;
    // Check file offset is not aligned to 32.
    assert_ne!(offset % 32, 0);
    assert_eq!(section.address(), 0);
    assert_eq!(section.size(), 32);

    let section = sections.next().unwrap();
    // Check there is no padding.
    assert_eq!(section.file_range(), Some((offset + 32, 1)));
    assert_eq!(section.address(), 32);
    assert_eq!(section.size(), 1);
}

#[test]
fn code_signature_encoder() {
    const HASH_SIZE: u8 = 32;
    // A hash filled with a distinguishable byte value.
    let hash = |value: u8| [value; HASH_SIZE as usize];

    let encoder = object::write::macho::CodeSignatureEncoder;
    let version = macho::CS_SUPPORTSEXECSEG;
    let ident = b"com.example.test\0";
    let n_special_slots = 2u32;
    let n_code_slots = 3u32;

    // Lay out the code directory blob.
    let ident_offset = encoder.code_directory_size(version);
    let special_offset = ident_offset + ident.len() as u32;
    let hash_offset = special_offset + n_special_slots * u32::from(HASH_SIZE);
    let cd_length = hash_offset + n_code_slots * u32::from(HASH_SIZE);

    let cd = object::write::macho::CodeDirectory {
        length: cd_length,
        version,
        flags: macho::CsFlags(0),
        hash_offset,
        ident_offset,
        n_special_slots,
        n_code_slots,
        code_limit: 0x1234,
        hash_size: HASH_SIZE,
        hash_type: macho::CS_HASHTYPE_SHA256,
        platform: 0,
        page_size: 12,
        scatter_offset: 0,
        team_offset: 0,
        exec_seg_base: 0,
        exec_seg_limit: 0x4000,
        exec_seg_flags: macho::CS_EXECSEG_MAIN_BINARY,
    };

    // Write the code directory blob.
    let mut cd_blob = Vec::new();
    encoder.code_directory(&mut cd_blob, &cd);
    cd_blob.write_bytes(ident);
    // Special slots are stored in reverse order (highest slot at the lowest offset).
    for slot in (1..=n_special_slots).rev() {
        cd_blob.write_bytes(&hash(0x10 + slot as u8));
    }
    for index in 0..n_code_slots {
        cd_blob.write_bytes(&hash(0x20 + index as u8));
    }
    assert_eq!(cd_blob.len(), cd_length as usize);

    // Write an empty requirements blob (a super blob with no entries).
    let mut req_blob = Vec::new();
    encoder.requirements_super_blob(&mut req_blob, encoder.super_blob_size(), 0);

    // Assemble the embedded signature super blob.
    let count = 2u32;
    let cd_offset = encoder.super_blob_size() + count * encoder.blob_index_size();
    let req_offset = cd_offset + cd_blob.len() as u32;
    let total_length = req_offset + req_blob.len() as u32;

    let mut buffer = Vec::new();
    encoder.signature_super_blob(&mut buffer, total_length, count);
    encoder.blob_index(&mut buffer, macho::CSSLOT_CODEDIRECTORY, cd_offset);
    encoder.blob_index(&mut buffer, macho::CSSLOT_REQUIREMENTS, req_offset);
    buffer.write_bytes(&cd_blob);
    buffer.write_bytes(&req_blob);
    assert_eq!(buffer.len(), total_length as usize);

    // Parse it back and check everything matches what was written.
    let signature = object::read::macho::CodeSignature::parse(&buffer).unwrap();
    assert_eq!(
        signature.header().magic.get(BigEndian),
        macho::CSMAGIC_EMBEDDED_SIGNATURE
    );
    assert_eq!(signature.header().length.get(BigEndian), total_length);
    assert_eq!(signature.header().count.get(BigEndian), count);

    let mut blobs = signature.blobs();

    let cd_blob = blobs.next().unwrap().unwrap();
    assert_eq!(cd_blob.slot(), macho::CSSLOT_CODEDIRECTORY);
    assert_eq!(cd_blob.offset(), cd_offset);
    assert_eq!(cd_blob.magic(), macho::CSMAGIC_CODEDIRECTORY);

    let code_directory = cd_blob.code_directory().unwrap().unwrap();
    let header = code_directory.header();
    assert_eq!(code_directory.version(), version);
    assert_eq!(header.length.get(BigEndian), cd_length);
    assert_eq!(header.flags.get(BigEndian), macho::CsFlags(0));
    assert_eq!(header.hash_offset.get(BigEndian), hash_offset);
    assert_eq!(header.n_special_slots.get(BigEndian), n_special_slots);
    assert_eq!(header.n_code_slots.get(BigEndian), n_code_slots);
    assert_eq!(header.code_limit.get(BigEndian), 0x1234);
    assert_eq!(header.hash_size, HASH_SIZE);
    assert_eq!(header.hash_type, macho::CS_HASHTYPE_SHA256);
    assert_eq!(header.page_size, 12);
    assert_eq!(code_directory.ident().unwrap(), b"com.example.test");
    assert_eq!(code_directory.code_limit64(), Some(0));
    assert_eq!(code_directory.code_limit(), 0x1234);
    let exec_seg = code_directory.exec_seg().unwrap();
    assert_eq!(exec_seg.exec_seg_limit.get(BigEndian), 0x4000);
    assert_eq!(
        exec_seg.exec_seg_flags.get(BigEndian),
        macho::CS_EXECSEG_MAIN_BINARY
    );

    for slot in 1..=n_special_slots {
        assert_eq!(
            code_directory.special_hash(macho::CsSlot(slot)).unwrap(),
            hash(0x10 + slot as u8),
        );
    }
    for index in 0..n_code_slots {
        assert_eq!(
            code_directory.code_hash(index).unwrap(),
            hash(0x20 + index as u8)
        );
    }

    let req_blob = blobs.next().unwrap().unwrap();
    assert_eq!(req_blob.slot(), macho::CSSLOT_REQUIREMENTS);
    assert_eq!(req_blob.offset(), req_offset);
    assert_eq!(req_blob.magic(), macho::CSMAGIC_REQUIREMENTS);

    assert!(blobs.next().unwrap().is_none());
}

#[test]
fn code_signature_encoder_code_limit64() {
    let encoder = object::write::macho::CodeSignatureEncoder;
    let version = macho::CS_SUPPORTSEXECSEG;
    let code_limit = 0x1_2345_6789;

    let cd = object::write::macho::CodeDirectory {
        length: encoder.code_directory_size(version),
        version,
        code_limit,
        hash_size: 32,
        hash_type: macho::CS_HASHTYPE_SHA256,
        page_size: 12,
        ..Default::default()
    };

    let mut cd_blob = Vec::new();
    encoder.code_directory(&mut cd_blob, &cd);

    let code_directory = read::macho::CodeDirectory::parse(&cd_blob).unwrap();
    assert_eq!(code_directory.header().code_limit.get(BigEndian), u32::MAX);
    assert_eq!(code_directory.code_limit64(), Some(code_limit));
    assert_eq!(code_directory.code_limit(), code_limit);
}
